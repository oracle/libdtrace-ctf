/* Miscellary.
   Copyright (c) 2003, 2019, Oracle and/or its affiliates. All rights reserved.

   Licensed under the Universal Permissive License v 1.0 as shown at
   http://oss.oracle.com/licenses/upl.

   Licensed under the GNU General Public License (GPL), version 2. See the file
   COPYING in the top level of this tree.  */

#include <ctf-impl.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifdef HAVE_MMAP
#include <sys/mman.h>
#endif
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <endian.h>
#include <gelf.h>
#include <zlib.h>

static size_t _PAGESIZE;
static size_t _PAGEMASK;

static void
maybe_init_pagemask (void)
{
  if (_PAGESIZE == 0)
    {
      _PAGESIZE = sysconf(_SC_PAGESIZE);
      _PAGEMASK = ~(_PAGESIZE - 1);
    }
}

void
libctf_init_debug (void)
{
  static int inited;
  if (!inited)
    {
      _libctf_debug = getenv ("LIBCTF_DEBUG") != NULL;
      inited = 1;
    }
}

/* Convert a 32-bit ELF file header into GElf.  */

static void
ehdr_to_gelf (const Elf32_Ehdr *src, GElf_Ehdr *dst)
{
  memcpy (dst->e_ident, src->e_ident, EI_NIDENT);
  dst->e_type = src->e_type;
  dst->e_machine = src->e_machine;
  dst->e_version = src->e_version;
  dst->e_entry = (Elf64_Addr) src->e_entry;
  dst->e_phoff = (Elf64_Off) src->e_phoff;
  dst->e_shoff = (Elf64_Off) src->e_shoff;
  dst->e_flags = src->e_flags;
  dst->e_ehsize = src->e_ehsize;
  dst->e_phentsize = src->e_phentsize;
  dst->e_phnum = src->e_phnum;
  dst->e_shentsize = src->e_shentsize;
  dst->e_shnum = src->e_shnum;
  dst->e_shstrndx = src->e_shstrndx;
}

/* Convert a 32-bit ELF section header into GElf.  */

static void
shdr_to_gelf (const Elf32_Shdr *src, GElf_Shdr *dst)
{
  dst->sh_name = src->sh_name;
  dst->sh_type = src->sh_type;
  dst->sh_flags = src->sh_flags;
  dst->sh_addr = src->sh_addr;
  dst->sh_offset = src->sh_offset;
  dst->sh_size = src->sh_size;
  dst->sh_link = src->sh_link;
  dst->sh_info = src->sh_info;
  dst->sh_addralign = src->sh_addralign;
  dst->sh_entsize = src->sh_entsize;
}

/* In order to mmap a section from the ELF file, we must round down sh_offset to
   the previous page boundary, and mmap the surrounding page.  We store the
   pointer to the start of the actual section data back into sp->cts_data.

   Fall back to copying when mmap() is not available.  */

const void *
ctf_sect_mmap (ctf_sect_t *sp, int fd)
{
  maybe_init_pagemask();

#ifdef HAVE_MMAP
  size_t pageoff = sp->cts_offset & ~_PAGEMASK;

  caddr_t base = mmap (NULL, sp->cts_size + pageoff, PROT_READ,
		       MAP_PRIVATE, fd, sp->cts_offset & _PAGEMASK);

  if (base != MAP_FAILED)
    sp->cts_data = base + pageoff;
  return base;
#else
  return (ctf_mmap (sp->cts_size, sp->cts_offset, fd));
#endif
}

/* Since sp->cts_data has the adjusted offset, we have to again round down
   to get the actual mmap address and round up to get the size.  */

void
ctf_sect_munmap (const ctf_sect_t *sp)
{
  maybe_init_pagemask();

#ifdef HAVE_MMAP
  uintptr_t addr = (uintptr_t) sp->cts_data;
  uintptr_t pageoff = addr & ~_PAGEMASK;

  (void) munmap ((void *) (addr - pageoff), sp->cts_size + pageoff);
#else
  free (sp->cts_data);
#endif
}

/* Open the specified file descriptor and return a pointer to a CTF container.
   The file can be either an ELF file or raw CTF file.  The caller is
   responsible for closing the file descriptor when it is no longer needed.  */

ctf_file_t *
ctf_fdopen (int fd, int *errp)
{
  ctf_sect_t ctfsect, symsect, strsect;
  ctf_file_t *fp = NULL;

  struct stat st;
  ssize_t nbytes;

  union
  {
    ctf_preamble_t ctf;
    Elf32_Ehdr e32;
    GElf_Ehdr e64;
  } hdr;

  memset (&ctfsect, 0, sizeof (ctf_sect_t));
  memset (&symsect, 0, sizeof (ctf_sect_t));
  memset (&strsect, 0, sizeof (ctf_sect_t));
  memset (&hdr.ctf, 0, sizeof (hdr));

  libctf_init_debug();
  maybe_init_pagemask();

  if (fstat (fd, &st) == -1)
    return (ctf_set_open_errno (errp, errno));

  if ((nbytes = ctf_pread (fd, &hdr.ctf, sizeof (hdr), 0)) <= 0)
    return (ctf_set_open_errno (errp, nbytes < 0 ? errno : ECTF_FMT));

  /*
   * If we have read enough bytes to form a CTF header and the magic
   * string matches, attempt to interpret the file as raw CTF.
   */
  if ((size_t) nbytes >= sizeof (ctf_preamble_t) &&
      hdr.ctf.ctp_magic == CTF_MAGIC)
    {
      void *data;

      if (hdr.ctf.ctp_version > CTF_VERSION)
	return (ctf_set_open_errno (errp, ECTF_CTFVERS));

      if ((data = ctf_mmap (st.st_size, 0, fd)) == NULL)
	return (ctf_set_open_errno (errp, errno));

      ctfsect.cts_name = _CTF_SECTION;
      ctfsect.cts_type = SHT_PROGBITS;
      ctfsect.cts_flags = SHF_ALLOC;
      ctfsect.cts_size = (size_t) st.st_size;
      ctfsect.cts_entsize = 1;
      ctfsect.cts_offset = 0;

      if ((fp = ctf_simple_open (data, (size_t) st.st_size, NULL, 0, 0,
				 NULL, 0, errp)) == NULL)
	ctf_sect_munmap (&ctfsect);

      return fp;
    }

  /* If we have read enough bytes to form an ELF header and the magic
     string matches, attempt to interpret the file as an ELF file.  We
     do our own largefile ELF processing, and convert everything to
     GElf structures so that clients can operate on any data model.  */

  if ((size_t) nbytes >= sizeof (Elf32_Ehdr)
     && memcmp (&hdr.e32.e_ident[EI_MAG0], ELFMAG, SELFMAG) == 0)
    {
#if __BYTE_ORDER == __BIG_ENDIAN
      unsigned char order = ELFDATA2MSB;
#elif __BYTE_ORDER == __LITTLE_ENDIAN
      unsigned char order = ELFDATA2LSB;
#else
#error Unknown endianness
#endif
      GElf_Half i, n;
      GElf_Shdr *sp;

      void *strs_map;
      size_t strs_mapsz;
      const char *strs;

      if (hdr.e32.e_ident[EI_DATA] != order)
	return (ctf_set_open_errno (errp, ECTF_ENDIAN));
      if (hdr.e32.e_version != EV_CURRENT)
	return (ctf_set_open_errno (errp, ECTF_ELFVERS));

      if (hdr.e32.e_ident[EI_CLASS] == ELFCLASS64)
	{
	  if ((size_t) nbytes < sizeof (GElf_Ehdr))
	    return (ctf_set_open_errno (errp, ECTF_FMT));
	}
      else
	{
	  Elf32_Ehdr e32 = hdr.e32;
	  ehdr_to_gelf (&e32, &hdr.e64);
	}

      if (hdr.e64.e_shstrndx >= hdr.e64.e_shnum)
	return (ctf_set_open_errno (errp, ECTF_CORRUPT));

      n = hdr.e64.e_shnum;
      nbytes = sizeof (GElf_Shdr) * n;

      if ((sp = malloc (nbytes)) == NULL)
	return (ctf_set_open_errno (errp, errno));

      /* Read in and convert to GElf the array of Shdr structures
	 from e_shoff so we can locate sections of interest.  */

      if (hdr.e32.e_ident[EI_CLASS] == ELFCLASS32)
	{
	  Elf32_Shdr *sp32;

	  nbytes = sizeof (Elf32_Shdr) * n;

	  if ((sp32 = malloc (nbytes)) == NULL || ctf_pread (fd, sp32, nbytes,
							 hdr.e64.e_shoff) !=
	      nbytes)
	    {
	      free (sp);
	      return (ctf_set_open_errno (errp, errno));
	    }

	  for (i = 0; i < n; i++)
	    shdr_to_gelf (&sp32[i], &sp[i]);

	  free (sp32);

	}
      else if (ctf_pread (fd, sp, nbytes, hdr.e64.e_shoff) != nbytes)
	{
	  free (sp);
	  return (ctf_set_open_errno (errp, errno));
	}

      /* Now mmap the section header strings section so that we can
         perform string comparison on the section names.  */

      strs_mapsz = sp[hdr.e64.e_shstrndx].sh_size +
	(sp[hdr.e64.e_shstrndx].sh_offset & ~_PAGEMASK);

      strs_map = ctf_mmap (strs_mapsz, fd,
			   sp[hdr.e64.e_shstrndx].sh_offset & _PAGEMASK);

      strs = (const char *) strs_map
	+ (sp[hdr.e64.e_shstrndx].sh_offset & ~_PAGEMASK);

      if (strs_map == MAP_FAILED)
	{
	  free (sp);
	  return (ctf_set_open_errno (errp, ECTF_MMAP));
	}

      /* Iterate over the section header array looking for the CTF
	 section and symbol table.  The strtab is linked to symtab.  */

      for (i = 0; i < n; i++)
	{
	  const GElf_Shdr *shp = &sp[i];
	  const GElf_Shdr *lhp = &sp[shp->sh_link];

	  if (shp->sh_link >= hdr.e64.e_shnum)
	    continue;		/* Corrupt sh_link field.  */

	  if (shp->sh_name >= sp[hdr.e64.e_shstrndx].sh_size ||
	      lhp->sh_name >= sp[hdr.e64.e_shstrndx].sh_size)
	    continue;		/* Corrupt sh_name field.  */

	  if (shp->sh_type == SHT_PROGBITS
	      && strcmp (strs + shp->sh_name, _CTF_SECTION) == 0)
	    {
	      ctfsect.cts_name = strs + shp->sh_name;
	      ctfsect.cts_type = shp->sh_type;
	      ctfsect.cts_flags = shp->sh_flags;
	      ctfsect.cts_size = shp->sh_size;
	      ctfsect.cts_entsize = shp->sh_entsize;
	      ctfsect.cts_offset = (off64_t) shp->sh_offset;

	    }
	  else if (shp->sh_type == SHT_SYMTAB)
	    {
	      symsect.cts_name = strs + shp->sh_name;
	      symsect.cts_type = shp->sh_type;
	      symsect.cts_flags = shp->sh_flags;
	      symsect.cts_size = shp->sh_size;
	      symsect.cts_entsize = shp->sh_entsize;
	      symsect.cts_offset = (off64_t) shp->sh_offset;

	      strsect.cts_name = strs + lhp->sh_name;
	      strsect.cts_type = lhp->sh_type;
	      strsect.cts_flags = lhp->sh_flags;
	      strsect.cts_size = lhp->sh_size;
	      strsect.cts_entsize = lhp->sh_entsize;
	      strsect.cts_offset = (off64_t) lhp->sh_offset;
	    }
	}

      free (sp);		/* Free section header array.  */

      if (ctfsect.cts_type == SHT_NULL)
	{
	  ctf_munmap (strs_map, strs_mapsz);
	  return (ctf_set_open_errno (errp, ECTF_NOCTFDATA));
	}

      /* Now mmap the CTF data, symtab, and strtab sections and
	 call ctf_bufopen() to do the rest of the work.  */

      if (ctf_sect_mmap (&ctfsect, fd) == MAP_FAILED)
	{
	  ctf_munmap (strs_map, strs_mapsz);
	  return (ctf_set_open_errno (errp, ECTF_MMAP));
	}

      if (symsect.cts_type != SHT_NULL && strsect.cts_type != SHT_NULL)
	{
	  if (ctf_sect_mmap (&symsect, fd) == MAP_FAILED ||
	      ctf_sect_mmap (&strsect, fd) == MAP_FAILED)
	    {
	      (void) ctf_set_open_errno (errp, ECTF_MMAP);
	      goto bad;
	    }
	  fp = ctf_bufopen (&ctfsect, &symsect, &strsect, errp);
	}
      else
	fp = ctf_bufopen (&ctfsect, NULL, NULL, errp);

    bad:
      /* Unmap all and abort.  */
      if (fp == NULL)
	{
	  ctf_sect_munmap (&ctfsect);
	  ctf_sect_munmap (&symsect);
	  ctf_sect_munmap (&strsect);
	}
      else
	fp->ctf_flags |= LCTF_MMAP;

      ctf_munmap (strs_map, strs_mapsz);
      return fp;
    }

  return (ctf_set_open_errno (errp, ECTF_FMT));
}

/* Open the specified file and return a pointer to a CTF container.  The file
   can be either an ELF file or raw CTF file.  This is just a convenient
   wrapper around ctf_fdopen() for callers.  */
ctf_file_t *
ctf_open (const char *filename, int *errp)
{
  ctf_file_t *fp;
  int fd;

  if ((fd = open (filename, O_RDONLY)) == -1)
    {
      if (errp != NULL)
	*errp = errno;
      return NULL;
    }

  fp = ctf_fdopen (fd, errp);
  (void) close (fd);
  return fp;
}

/* Write the compressed CTF data stream to the specified gzFile descriptor.
   This is useful for saving the results of dynamic CTF containers.  */
int
ctf_gzwrite (ctf_file_t *fp, gzFile fd)
{
  const unsigned char *buf = fp->ctf_base;
  ssize_t resid = fp->ctf_size;
  ssize_t len;

  while (resid != 0)
    {
      if ((len = gzwrite (fd, buf, resid)) <= 0)
	return (ctf_set_errno (fp, errno));
      resid -= len;
      buf += len;
    }

  return 0;
}

/* Compress the specified CTF data stream and write it to the specified file
   descriptor.  */
int
ctf_compress_write (ctf_file_t *fp, int fd)
{
  unsigned char *buf;
  unsigned char *bp;
  ctf_header_t h;
  ctf_header_t *hp = &h;
  ssize_t header_len = sizeof (ctf_header_t);
  ssize_t compress_len;
  size_t max_compress_len = compressBound (fp->ctf_size - header_len);
  ssize_t len;
  int rc;
  int err = 0;

  memcpy (hp, fp->ctf_base, header_len);
  hp->cth_flags |= CTF_F_COMPRESS;

  if ((buf = ctf_data_alloc (max_compress_len)) == MAP_FAILED)
    return (ctf_set_errno (fp, ECTF_ZALLOC));

  compress_len = max_compress_len;
  if ((rc = compress (buf, (uLongf *) & compress_len,
		      fp->ctf_base + header_len,
		      fp->ctf_size - header_len)) != Z_OK)
    {
      ctf_dprintf ("zlib deflate err: %s\n", zError (rc));
      err = ctf_set_errno (fp, ECTF_COMPRESS);
      goto ret;
    }

  while (header_len > 0)
    {
      if ((len = write (fd, hp, header_len)) < 0)
	{
	  err = ctf_set_errno (fp, errno);
	  goto ret;
	}
      header_len -= len;
      hp += len;
    }

  bp = buf;
  while (compress_len > 0)
    {
      if ((len = write (fd, bp, compress_len)) < 0)
	{
	  err = ctf_set_errno (fp, errno);
	  goto ret;
	}
      compress_len -= len;
      bp += len;
    }

ret:
  ctf_data_free (buf, max_compress_len);
  return err;
}

/* Write the uncompressed CTF data stream to the specified file descriptor.
   This is useful for saving the results of dynamic CTF containers.  */
int
ctf_write (ctf_file_t *fp, int fd)
{
  const unsigned char *buf = fp->ctf_base;
  ssize_t resid = fp->ctf_size;
  ssize_t len;

  while (resid != 0)
    {
      if ((len = write (fd, buf, resid)) < 0)
	return (ctf_set_errno (fp, errno));
      resid -= len;
      buf += len;
    }

  return 0;
}

/* Set the CTF library client version to the specified version.  If version is
   zero, we just return the default library version number.  */
int
ctf_version (int version)
{
  if (version < 0)
    {
      errno = EINVAL;
      return -1;
    }

  if (version > 0)
    {
      /*  Dynamic version switching is not presently supported. */
      if (version != CTF_VERSION)
	{
	  errno = ENOTSUP;
	  return -1;
	}
      ctf_dprintf ("ctf_version: client using version %d\n", version);
      _libctf_version = version;
    }

  return _libctf_version;
}
