/* Simple subrs.
   Copyright (c) 2003, 2019, Oracle and/or its affiliates. All rights reserved.

   Licensed under the Universal Permissive License v 1.0 as shown at
   http://oss.oracle.com/licenses/upl.

   Licensed under the GNU General Public License (GPL), version 2. See the file
   COPYING in the top level of this tree.  */

#include <ctf-impl.h>
#ifdef HAVE_MMAP
#include <sys/mman.h>
#endif
#include <stdarg.h>
#include <string.h>
#include <unistd.h>

void *
ctf_data_alloc (size_t size)
{
  void *ret;

#ifdef HAVE_MMAP
  ret = mmap (NULL, size, PROT_READ | PROT_WRITE,
	      MAP_PRIVATE | MAP_ANON, -1, 0);
  if (ret == MAP_FAILED)
    ret = NULL;
#else
  ret = malloc (size);
#endif
  return ret;
}

void
ctf_data_free (void *buf, size_t size _libctf_unused_)
{
#ifdef HAVE_MMAP
  (void) munmap (buf, size);
#else
  free (buf);
#endif
}

/* Private, read-only mmap from a file, with fallback to copying.

   No handling of page-offset issues at all: the caller must allow for that. */

void *
ctf_mmap (size_t length, size_t offset, int fd)
{
  void *data;

#ifdef HAVE_MMAP
  data = mmap (NULL, length, PROT_READ, MAP_PRIVATE, fd, offset);
  if (data == MAP_FAILED)
    data = NULL;
#else
  if ((data = malloc (length)) != NULL)
    {
      if (pread (fd, data, size, offset) <= 0)
	{
	  free (data);
	  data = NULL;
	}
    }
#endif
  return data;
}

void
ctf_munmap (void *buf, size_t length _libctf_unused_)
{
#ifdef HAVE_MMAP
  (void) munmap (buf, length);
#else
  free (buf);
#endif
}

void
ctf_data_protect (void *buf, size_t size)
{
  (void) mprotect (buf, size, PROT_READ);
}

void *
ctf_alloc (size_t size)
{
  return (malloc (size));
}

void
ctf_free (void *buf, size_t size _libctf_unused_)
{
  free (buf);
}

const char *
ctf_strerror (int err)
{
  return (const char *) (strerror (err));
}

_libctf_printflike_ (1, 2)
void ctf_dprintf (const char *format, ...)
{
  if (_libctf_debug)
    {
      va_list alist;

      va_start (alist, format);
      (void) fputs ("libctf DEBUG: ", stderr);
      (void) vfprintf (stderr, format, alist);
      va_end (alist);
    }
}
