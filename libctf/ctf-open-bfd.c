/* Opening CTF files with BFD.
   Copyright (c) 2006, 2019, Oracle and/or its affiliates. All rights reserved.

   Licensed under the Universal Permissive License v 1.0 as shown at
   http://oss.oracle.com/licenses/upl.

   Licensed under the GNU General Public License (GPL), version 2. See the file
   COPYING in the top level of this tree.  */

#include <ctf-impl.h>
#include <stddef.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <elf.h>
#include <bfd.h>

#ifdef BFD_ONLY
#include "elf-bfd.h"
#else
#define SHN_EXTABS SHN_ABS
#endif

/* Make a new struct ctf_archive_internal wrapper for a ctf_archive or a
   ctf_file.  Closes ARC and/or FP on error.  Arrange to free the SYMSECT and
   STRSECT interior on close.  */

static struct ctf_archive_internal *
ctf_new_archive_internal (int is_archive, struct ctf_archive *arc,
			  ctf_file_t *fp, const ctf_sect_t *symsect,
			  const ctf_sect_t *strsect,
			  int *errp)
{
  struct ctf_archive_internal *arci;

  if ((arci = calloc (1, sizeof (struct ctf_archive_internal))) == NULL)
    {
      if (is_archive)
	ctf_arc_close_internal (arc);
      else
	ctf_file_close (fp);
      return (ctf_set_open_errno (errp, errno));
    }
  arci->ctfi_is_archive = is_archive;
  if (is_archive)
    arci->ctfi_archive = arc;
  else
    arci->ctfi_file = fp;
#ifndef BFD_ONLY
  arci->ctfi_magic = CTFI_MAGIC;
#endif
  if (symsect)
     memcpy (&arci->ctfi_symsect, symsect, sizeof (struct ctf_sect));
  if (strsect)
     memcpy (&arci->ctfi_strsect, strsect, sizeof (struct ctf_sect));

  return arci;
}

/* Free the BFD bits of a CTF file on ctf_file_close().  */

static void
ctf_bfdclose (struct ctf_archive_internal *arci)
{
  if (arci->ctfi_abfd != NULL)
    if (!bfd_close_all_done (arci->ctfi_abfd))
      ctf_dprintf ("Cannot close BFD: %s\n", bfd_errmsg (bfd_get_error()));
}

/* Open a CTF file given the specified BFD.  */

ctf_archive_t *
ctf_bfdopen (struct bfd *abfd, int *errp)
{
  ctf_archive_t *arc;
  asection *ctf_asect;
  bfd_byte *contents;
  ctf_sect_t ctfsect;

  libctf_init_debug();

  if ((ctf_asect = bfd_get_section_by_name (abfd, _CTF_SECTION)) == NULL)
    {
      return (ctf_set_open_errno (errp, ECTF_NOCTFDATA));
    }

  if (!bfd_malloc_and_get_section (abfd, ctf_asect, &contents))
    {
      ctf_dprintf ("ctf_bfdopen(): cannot malloc CTF section: %s\n",
		   bfd_errmsg (bfd_get_error()));
      return (ctf_set_open_errno (errp, ECTF_FMT));
    }

  ctfsect.cts_name = _CTF_SECTION;
  ctfsect.cts_type = SHT_PROGBITS;
  ctfsect.cts_flags = 0;
  ctfsect.cts_entsize = 1;
  ctfsect.cts_offset = 0;
  ctfsect.cts_size = bfd_section_size (abfd, ctf_asect);
  ctfsect.cts_data = contents;

  if ((arc = ctf_bfdopen_ctfsect (abfd, &ctfsect, errp)) != NULL)
    {
      arc->ctfi_data = (void *) ctfsect.cts_data;
      return arc;
    }

  free (contents);
  return NULL;				/* errno is set for us.  */
}

/* Open a CTF file given the specified BFD and CTF section (which may contain a
   CTF archive or a file).  Takes ownership of the ctfsect, and frees it
   later.  */

ctf_archive_t *
ctf_bfdopen_ctfsect (struct bfd *abfd, const ctf_sect_t *ctfsect, int *errp)
{
  struct ctf_archive *arc = NULL;
  ctf_archive_t *arci;
  ctf_file_t *fp = NULL;
  ctf_sect_t *symsectp = NULL;
  ctf_sect_t *strsectp = NULL;
  const char *bfderrstr = NULL;
  int is_archive;

#ifdef BFD_ONLY
  asection *sym_asect;
  ctf_sect_t symsect, strsect;
  /* TODO: handle SYMTAB_SHNDX.  */

  if ((sym_asect = bfd_section_from_elf_index (abfd,
					       elf_onesymtab (abfd))) != NULL)
    {
      Elf_Internal_Shdr *symhdr = &elf_symtab_hdr (abfd);
      asection *str_asect = NULL;
      bfd_byte *contents;

      if (symhdr->sh_link != SHN_UNDEF &&
	  symhdr->sh_link <= elf_numsections (abfd))
	str_asect = bfd_section_from_elf_index (abfd, symhdr->sh_link);

      Elf_Internal_Shdr *strhdr = elf_elfsections (abfd)[symhdr->sh_link];

      if (sym_asect && str_asect)
	{
	  if (!bfd_malloc_and_get_section (abfd, str_asect, &contents))
	    {
	      bfderrstr = "Cannot malloc string table";
	      free (contents);
	      goto err;
	    }
	  strsect.cts_data = contents;
	  strsect.cts_name = (char *) strsect.cts_data + strhdr->sh_name;
	  strsect.cts_type = strhdr->sh_type;
	  strsect.cts_flags = strhdr->sh_flags;
	  strsect.cts_entsize = strhdr->sh_size;
	  strsect.cts_offset = strhdr->sh_offset;
	  strsectp = &strsect;

	  if (!bfd_malloc_and_get_section (abfd, sym_asect, &contents))
	    {
	      bfderrstr = "Cannot malloc symbol table";
	      free (contents);
	      goto err_free_str;
	    }

	  symsect.cts_name = (char *) strsect.cts_data + symhdr->sh_name;
	  symsect.cts_type = symhdr->sh_type;
	  symsect.cts_flags = symhdr->sh_flags;
	  symsect.cts_entsize = symhdr->sh_size;
	  symsect.cts_data = contents;
	  symsect.cts_offset = symhdr->sh_offset;
	  symsectp = &symsect;
	}
    }
#endif

  if (ctfsect->cts_size > sizeof (uint64_t) &&
      ((*(uint64_t *) ctfsect->cts_data) == CTFA_MAGIC))
    {
      is_archive = 1;
      if ((arc = ctf_arc_bufopen ((void *) ctfsect->cts_data,
				  ctfsect->cts_size, errp)) == NULL)
	goto err_free_sym;
    }
  else
    {
      is_archive = 0;
      if ((fp = ctf_bufopen (ctfsect, symsectp, strsectp, errp)) == NULL)
	{
	  ctf_dprintf ("ctf_internal_open(): cannot open CTF: %s\n",
		       ctf_errmsg (*errp));
	  goto err_free_sym;
	}
    }
  arci = ctf_new_archive_internal (is_archive, arc, fp, symsectp, strsectp,
				   errp);

  if (arci)
    return arci;
 err_free_sym:
#ifdef BFD_ONLY
  free ((void *) symsect.cts_data);
err_free_str:
  free ((void *) strsect.cts_data);
#endif
err: _libctf_unused_;
  if (bfderrstr)
    {
      ctf_dprintf ("ctf_bfdopen(): %s: %s\n", bfderrstr,
		   bfd_errmsg (bfd_get_error()));
      ctf_set_open_errno (errp, ECTF_FMT);
    }
  return NULL;
}

/* Open the specified file descriptor and return a pointer to a CTF archive that
   contains one or more CTF containers.  The file can be an ELF file, a raw CTF
   file, or a CTF archive.  The caller is responsible for closing the file
   descriptor when it is no longer needed.  If this is an ELF file, TARGET, if
   non-NULL, should be the name of a suitable BFD target.  */

ctf_archive_t *
ctf_fdopen (int fd, const char *filename, const char *target, int *errp)
{
  ctf_archive_t *arci;
  bfd *abfd;
  int nfd;

  struct stat st;
  ssize_t nbytes;

  ctf_preamble_t ctfhdr;
  uint64_t arc_magic;

  memset (&ctfhdr, 0, sizeof (ctfhdr));

  libctf_init_debug();

  if (fstat (fd, &st) == -1)
    return (ctf_set_open_errno (errp, errno));

  if ((nbytes = ctf_pread (fd, &ctfhdr, sizeof (ctfhdr), 0)) <= 0)
    return (ctf_set_open_errno (errp, nbytes < 0 ? errno : ECTF_FMT));

  /* If we have read enough bytes to form a CTF header and the magic
     string matches, attempt to interpret the file as raw CTF.  */

  if ((size_t) nbytes >= sizeof (ctf_preamble_t) &&
      ctfhdr.ctp_magic == CTF_MAGIC)
    {
      ctf_file_t *fp = NULL;
      void *data;

      if (ctfhdr.ctp_version > CTF_VERSION)
	return (ctf_set_open_errno (errp, ECTF_CTFVERS));

      if ((data = ctf_mmap (st.st_size, 0, fd)) == NULL)
	return (ctf_set_open_errno (errp, errno));

      if ((fp = ctf_simple_open (data, (size_t) st.st_size, NULL, 0, 0,
				 NULL, 0, errp)) == NULL)
	ctf_munmap (data, (size_t) st.st_size);
      fp->ctf_data_mmapped = data;
      fp->ctf_data_mmapped_len = (size_t) st.st_size;

      return ctf_new_archive_internal (0, NULL, fp, NULL, NULL, errp);
    }

  if ((nbytes = ctf_pread (fd, &arc_magic, sizeof (arc_magic), 0)) <= 0)
    return (ctf_set_open_errno (errp, nbytes < 0 ? errno : ECTF_FMT));

  if ((size_t) nbytes >= sizeof (uint64_t) && arc_magic == CTFA_MAGIC)
    {
      struct ctf_archive *arc;

      if ((arc = ctf_arc_open_internal (filename, errp)) == NULL)
	return NULL;			/* errno is set for us.  */

      return ctf_new_archive_internal (1, arc, NULL, NULL, NULL, errp);
    }

  /* Attempt to open the file with BFD.  We must dup the fd first, since bfd
     takes ownership of the passed fd.  */

  if ((nfd = dup (fd)) < 0)
      return (ctf_set_open_errno (errp, errno));

  if ((abfd = bfd_fdopenr (filename, target, nfd)) == NULL)
    {
      ctf_dprintf ("Cannot open BFD from %s: %s\n",
		   filename ? filename : "(unknown file)",
		   bfd_errmsg (bfd_get_error()));
      return (ctf_set_open_errno (errp, ECTF_FMT));
    }

  if (!bfd_check_format (abfd, bfd_object))
    {
      ctf_dprintf ("BFD format problem in %s: %s\n",
		   filename ? filename : "(unknown file)",
		   bfd_errmsg (bfd_get_error()));
      if (bfd_get_error() == bfd_error_file_ambiguously_recognized)
	return (ctf_set_open_errno (errp, ECTF_BFD_AMBIGUOUS));
      else
	return (ctf_set_open_errno (errp, ECTF_FMT));
    }

  if ((arci = ctf_bfdopen (abfd, errp)) == NULL)
    {
      if (!bfd_close_all_done (abfd))
	ctf_dprintf ("Cannot close BFD: %s\n", bfd_errmsg (bfd_get_error()));
      return NULL;			/* errno is set for us.  */
    }
  arci->ctfi_bfd_close = ctf_bfdclose;
  arci->ctfi_abfd = abfd;

  return arci;
}

/* Open the specified file and return a pointer to a CTF container.  The file
   can be either an ELF file or raw CTF file.  This is just a convenient
   wrapper around ctf_fdopen() for callers.  */

ctf_archive_t *
ctf_open (const char *filename, const char *target, int *errp)
{
  ctf_archive_t *arc;
  int fd;

  if ((fd = open (filename, O_RDONLY)) == -1)
    {
      if (errp != NULL)
	*errp = errno;
      return NULL;
    }

  arc = ctf_fdopen (fd, filename, target, errp);
  (void) close (fd);
  return arc;
}

/* Public entry point: open a CTF archive, or CTF file.  Returns the archive, or
   NULL and an error in *err.  Despite the fact that this uses CTF archives, it
   must be in this file to avoid dragging in BFD into non-BFD-using programs.  */
ctf_archive_t *
ctf_arc_open (const char *filename, int *errp)
{
  return ctf_open (filename, NULL, errp);
}
