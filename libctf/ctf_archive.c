/*
 * Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.
 *
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 *
 * Licensed under the GNU General Public License (GPL), version 2. See the file
 * COPYING in the top level of this tree.
 */

#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <elf.h>
#include <endian.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <ctf_impl.h>

static off_t arc_write_one_ctf(ctf_file_t *f, int fd, size_t threshold);
static ctf_file_t *ctf_arc_open_by_offset(const ctf_archive_t *arc,
    size_t offset, int *errp);
static int sort_modent_by_name(const void *one, const void *two, void *n);

/*
 * bsearch() internal state.
 */
static __thread char *search_nametbl;

/*
 * Write out a CTF archive.  The entries in ctf_files are referenced by name:
 * the names are passed in the names array, which must have ctf_files entries.
 *
 * Returns 0 on success, or an errno, or an ECTF_* value.
 */
int
ctf_arc_write(const char *file, ctf_file_t **ctf_files, size_t ctf_file_cnt,
    const char **names, size_t threshold)
{
	const char *errmsg;
	struct ctf_archive *archdr;
	int fd;
	size_t i;
	char dummy = 0;
	size_t headersz;
	ssize_t namesz;
	size_t ctf_startoffs;	 /* start of the section we are working over */
	char *nametbl = NULL;	 /* the name table */
	char *np;
	off_t nameoffs;
	struct ctf_archive_modent *modent;

	ctf_dprintf("Writing archive %s with %zi files\n",
	    file, ctf_file_cnt);

	if ((fd = open(file, O_RDWR | O_CREAT | O_TRUNC | O_CLOEXEC, 0666)) < 0) {
		errmsg = "ctf_arc_write(): cannot create %s: %s\n";
		goto err;
	}

	/*
	 * Figure out the size of the mmap()ed header, including the
	 * ctf_archive_modent array.  We assume that all of this needs no
	 * padding: a likely assumption, given that it's all made up of
	 * uint64_t's.
	 */
	headersz = sizeof (struct ctf_archive) +
	    (ctf_file_cnt * sizeof (uint64_t) * 2);
	ctf_dprintf("headersz is %zi\n", headersz);

	/*
	 * From now on we work in two pieces: an mmap()ed region from zero up to
	 * the headersz, and a region updated via write() starting after that,
	 * containing all the tables.
	 */
	ctf_startoffs = headersz;
	if (lseek(fd, ctf_startoffs - 1, SEEK_SET) < 0) {
		errmsg = "ctf_arc_write(): cannot extend file while writing %s: %s\n";
		goto err_close;
	}

	if (write(fd, &dummy, 1) < 0) {
		errmsg = "ctf_arc_write(): cannot extend file while writing %s: %s\n";
		goto err_close;
	}

	if ((archdr = mmap(NULL, headersz, PROT_READ | PROT_WRITE, MAP_SHARED, fd,
		    0)) == MAP_FAILED) {
		errmsg = "ctf_arc_write(): Cannot mmap() %s: %s\n";
		goto err_close;
	}

	/*
	 * Fill in everything we can, which is everything other than the name
	 * table offset.
	 */
	archdr->ctfa_magic = htole64(CTFA_MAGIC);
	archdr->ctfa_nfiles = htole64(ctf_file_cnt);
	archdr->ctfa_ctfs = htole64(ctf_startoffs);

	/*
	 * We could validate that all CTF files have the same data model, but
	 * since any reasonable construction process will be building things of
	 * only one bitness anyway, this is pretty pointless, so just use the
	 * model of the first CTF file for all of them.  (It *is* valid to
	 * create an empty archive: the value of ctfa_model is irrelevant in
	 * this case, but we must be sure not to dereference uninitialized
	 * memory.)
	 */
	if (ctf_file_cnt > 0)
		archdr->ctfa_model = htole64(ctf_getmodel(ctf_files[0]));

	/*
	 * Now write out the CTFs: ctf_archive_modent array via the mapping,
	 * ctfs via write().  The names themselves have not been written yet: we
	 * track them in a local strtab until the time is right, and sort the
	 * modents array after construction.
	 *
	 * The name table is not sorted.
	 */
	for (i = 0, namesz = 0; i < le64toh(archdr->ctfa_nfiles); i++)
		namesz += strlen(names[i]) + 1;

	nametbl = malloc(namesz);
	if (nametbl == NULL) {
		errmsg = "Error writing named CTF to %s: %s\n";
		goto err_unmap;
	}

	for (i = 0, namesz = 0,
		 modent = (ctf_archive_modent_t *) ((char *) archdr +
		     sizeof (struct ctf_archive));
	     i < le64toh(archdr->ctfa_nfiles); i++) {
		off_t off;

		strcpy(&nametbl[namesz], names[i]);

		off = arc_write_one_ctf(ctf_files[i], fd, threshold);
		ctf_dprintf("Written %s, offset now %zi\n", names[i], off);
		if ((off < 0) && (off > -ECTF_BASE)) {
			errmsg = "ctf_arc_write(): Cannot determine file "
			    "position while writing %s: %s";
			goto err_free;
		}
		if (off < 0) {
			errmsg = "ctf_arc_write(): Cannot write CTF file "
			    "to %s: %s\n";
			errno = off * -1;
			goto err_free;
		}

		modent->name_offset = htole64(namesz);
		modent->ctf_offset = htole64(off - ctf_startoffs);
		namesz += strlen(names[i]) + 1;
		modent++;
	}

	qsort_r((ctf_archive_modent_t *) ((char *) archdr +
		sizeof (struct ctf_archive)),
	    le64toh(archdr->ctfa_nfiles),
	    sizeof (struct ctf_archive_modent),
	    sort_modent_by_name, nametbl);

	/*
	 * Now the name table.
	 */
	if ((nameoffs = lseek(fd, 0, SEEK_CUR)) < 0) {
		errmsg = "ctf_arc_write(): Cannot get current file position "
		    "in %s: %s\n";
		goto err_free;
	}
	archdr->ctfa_names = htole64(nameoffs);
	np = nametbl;
	while (namesz > 0) {
		size_t len;
		if ((len = write(fd, np, namesz)) < 0) {
			errmsg = "ctf_arc_write(): Cannot write name table "
			    "in %s: %s\n";
			goto err_free;
		}
		namesz -= len;
		np += len;
	}
	free(nametbl);
	if (msync(archdr, headersz, MS_ASYNC) < 0) {
		errmsg = "ctf_arc_write(): Cannot sync after writing "
		    "to %s: %s\n";
		goto err_unmap;
	}
	munmap(archdr, headersz);
	if (close(fd) < 0) {
		errmsg = "ctf_arc_write(): Cannot close after writing "
		    "to %s: %s\n";
		goto err_unlink;
	}

	return 0;

err_free:
	free(nametbl);
err_unmap:
	munmap(archdr, headersz);
err_close:
	close(fd);
err_unlink:
	unlink(file);
err:
	ctf_dprintf(errmsg, file, errno < ECTF_BASE ? strerror(errno) :
	    ctf_errmsg(errno));
	return errno;
}

/*
 * Write one CTF file out.  Return the file position of the written file (or
 * rather, of the file-size uint64_t that precedes it): negative return is a
 * negative errno or ctf_errno value.  On error, the file position may no longer
 * be at the end of the file.
 */
static off_t
arc_write_one_ctf(ctf_file_t *f, int fd, size_t threshold)
{
	off_t off, end_off;
	uint64_t ctfsz = 0;
	char *ctfszp;
	size_t ctfsz_len;
	int (*writefn) (ctf_file_t *fp, int fd);
	struct stat s;

	if (fstat(fd, &s) >= 0) {
		ctf_dprintf("file size now %lu\n", s.st_size);
	}

	if ((off = lseek(fd, 0, SEEK_CUR)) < 0)
		return errno * -1;
	ctf_dprintf("initial offset: %li\n", off);

	if (f->ctf_size > threshold)
		writefn = ctf_compress_write;
	else
		writefn = ctf_write;

	/*
	 * This turns into the size in a moment.
	 */
	ctfsz_len = sizeof (ctfsz);
	ctfszp = (char *) &ctfsz;
	while (ctfsz_len > 0) {
		ssize_t writelen = write(fd, ctfszp, ctfsz_len);
		if (writelen < 0)
			return errno * -1;
		ctfsz_len -= writelen;
		ctfszp += writelen;
	}

	if (writefn(f, fd) != 0)
		return f->ctf_errno * -1;

	if ((end_off = lseek(fd, 0, SEEK_CUR)) < 0)
		return errno * -1;
	ctfsz = htole64(end_off - off);

	if ((lseek(fd, off, SEEK_SET)) < 0)
		return errno * -1;

	ctfsz_len = sizeof (ctfsz);
	ctfszp = (char *) &ctfsz;
	while (ctfsz_len > 0) {
		ssize_t writelen = write(fd, ctfszp, ctfsz_len);
		if (writelen < 0)
			return errno * -1;
		ctfsz_len -= writelen;
		ctfszp += writelen;
	}

	end_off = LCTF_ALIGN_OFFS(end_off, 8);
	if ((lseek(fd, end_off, SEEK_SET)) < 0)
		return errno * -1;

	return off;
}

/*
 * qsort() function to sort the array of struct ctf_archive_modents into
 * ascending name order.
 */
static int
sort_modent_by_name(const void *one, const void *two, void *n)
{
	const struct ctf_archive_modent *a = one;
	const struct ctf_archive_modent *b = two;
	char *nametbl = n;

	return strcmp(&nametbl[le64toh(a->name_offset)],
	    &nametbl[le64toh(b->name_offset)]);
}

/*
 * bsearch() function to search for a given name in the sorted array of struct
 * ctf_archive_modents.
 */
static int
search_modent_by_name(const void *key, const void *ent)
{
	const char *k = key;
	const struct ctf_archive_modent *v = ent;

	return strcmp(k, &search_nametbl[le64toh(v->name_offset)]);
}

/*
 * Open a CTF archive.  Returns the archive, or NULL and an error in *err (if
 * not NULL).
 */
ctf_archive_t *
ctf_arc_open(const char *filename, int *errp)
{
	const char *errmsg;
	int fd;
	struct stat s;
	ctf_archive_t *arc;		/* actually the whole file */

	if ((fd = open(filename, O_RDONLY)) < 0) {
		errmsg = "ctf_arc_open(): cannot open %s: %s\n";
		goto err;
	}
	if (fstat(fd, &s) < 0) {
		errmsg = "ctf_arc_open(): cannot stat %s: %s\n";
		goto err_close;
	}

	if ((arc = mmap(NULL, s.st_size, PROT_READ | PROT_WRITE,
		    MAP_PRIVATE, fd, 0)) == MAP_FAILED) {
		errmsg = "ctf_arc_open(): Cannot mmap() %s: %s\n";
		goto err_close;
	}

	if (le64toh(arc->ctfa_magic) != CTFA_MAGIC) {
		errmsg = "ctf_arc_open(): Invalid magic number";
		errno = ECTF_FMT;
		goto err_unmap;
	}

	/*
	 * This horrible hack lets us know how much to unmap when the file is
	 * closed.  (We no longer need the magic number, and the mapping
	 * is private.)
	 */
	arc->ctfa_magic = s.st_size;
	close(fd);
	return arc;

err_unmap:
	munmap(NULL, s.st_size);
err_close:
	close(fd);
err:
	if (errp)
		*errp = errno;
	ctf_dprintf(errmsg, filename, errno < ECTF_BASE ? strerror(errno) :
	    ctf_errmsg(errno));
	return NULL;
}

/*
 * Close an archive.
 */
void
ctf_arc_close(ctf_archive_t *arc)
{
	if (arc == NULL)
		return;

	/*
	 * See the comment in ctf_arc_open().
	 */
	munmap(arc, arc->ctfa_magic);
}

/*
 * Return the ctf_file_t with the given name, or NULL if none,
 * setting 'err' if non-NULL.
 */
ctf_file_t *
ctf_arc_open_by_name(const ctf_archive_t *arc, const char *name, int *errp)
{
	struct ctf_archive_modent *modent;

	ctf_dprintf("ctf_arc_open_by_name(%s): opening\n", name);

	modent = (ctf_archive_modent_t *) ((char *) arc +
	    sizeof (struct ctf_archive));

	search_nametbl = (char *) arc + le64toh(arc->ctfa_names);
	modent = bsearch(name, modent, le64toh(arc->ctfa_nfiles),
	    sizeof (struct ctf_archive_modent), search_modent_by_name);

	/*
	 * This is actually a common case and normal operation: no error
	 * debug output.
	 */
	if (modent == NULL) {
		if (errp)
			*errp = ECTF_ARNNAME;
		return NULL;
	}

	return ctf_arc_open_by_offset(arc, le64toh(modent->ctf_offset), errp);
}

/*
 * Return the ctf_file_t at the given ctfa_ctfs-relative offset, or NULL if
 * none, setting 'err' if non-NULL.
 */
static ctf_file_t *
ctf_arc_open_by_offset(const ctf_archive_t *arc, size_t offset, int *errp)
{
	ctf_sect_t ctfsect;
	ctf_file_t *fp;

	ctf_dprintf("ctf_arc_open_by_offset(%zi): opening\n", offset);

	bzero(&ctfsect, sizeof (ctf_sect_t));

	offset += le64toh(arc->ctfa_ctfs);

	ctfsect.cts_name = _CTF_SECTION;
	ctfsect.cts_type = SHT_PROGBITS;
	ctfsect.cts_flags = SHF_ALLOC;
	ctfsect.cts_size = le64toh(*((uint64_t *) ((char *)arc + offset)));
	ctfsect.cts_entsize = 1;
	ctfsect.cts_offset = 0;
	ctfsect.cts_data = (void *) ((char *)arc + offset + sizeof (uint64_t));
	fp = ctf_bufopen(&ctfsect, NULL, NULL, errp);
	if (fp)
		ctf_setmodel(fp, le64toh(arc->ctfa_model));
	return fp;
}

/*
 * Iterate over all CTF files in an archive.  We pass the raw data for all CTF
 * files in turn to the specified callback function.
 */
int
ctf_archive_raw_iter(const ctf_archive_t *arc, ctf_archive_raw_member_f *func,
    void *data)
{
	int rc;
	size_t i;
	struct ctf_archive_modent *modent;
	const char *nametbl;

	modent = (ctf_archive_modent_t *) ((char *) arc +
	    sizeof (struct ctf_archive));
	nametbl = (((const char *) arc) + le64toh(arc->ctfa_names));

	for (i = 0; i < le64toh(arc->ctfa_nfiles); i++) {
		const char *name;
		char *fp;

		name = &nametbl[le64toh(modent[i].name_offset)];
		fp = ((char *)arc + le64toh(arc->ctfa_ctfs) +
		    le64toh(modent[i].ctf_offset));

		if ((rc = func(name, (void *) (fp + sizeof (uint64_t)),
			    le64toh(*((uint64_t *) fp)), data)) != 0)
			return rc;
	}
	return(0);
}

/*
 * Iterate over all CTF files in an archive.  We pass all CTF files in turn to
 * the specified callback function.
 */
int
ctf_archive_iter(const ctf_archive_t *arc, ctf_archive_member_f *func,
    void *data)
{
	int rc;
	size_t i;
	ctf_file_t *f;
	struct ctf_archive_modent *modent;
	const char *nametbl;

	modent = (ctf_archive_modent_t *) ((char *) arc +
	    sizeof (struct ctf_archive));
	nametbl = (((const char *) arc) + le64toh(arc->ctfa_names));

	for (i = 0; i < le64toh(arc->ctfa_nfiles); i++) {
		const char *name;

		name = &nametbl[le64toh(modent[i].name_offset)];
		if ((f = ctf_arc_open_by_name(arc, name, &rc)) == NULL)
			return rc;

		if ((rc = func(f, name, data)) != 0) {
			ctf_close(f);
			return rc;
		}

		ctf_close(f);
	}
	return(0);
}
