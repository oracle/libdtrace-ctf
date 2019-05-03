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
#include <sys/types.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>

static size_t _PAGESIZE;

void *
ctf_data_alloc (size_t size)
{
  void *ret;

#ifdef HAVE_MMAP
  if (_PAGESIZE == 0)
    _PAGESIZE = sysconf(_SC_PAGESIZE);

  if (size > _PAGESIZE)
    {
      ret = mmap (NULL, size, PROT_READ | PROT_WRITE,
		  MAP_PRIVATE | MAP_ANON, -1, 0);
      if (ret == MAP_FAILED)
	ret = NULL;
    }
  else
    ret = malloc (size);
#else
  ret = malloc (size);
#endif
  return ret;
}

void
ctf_data_free (void *buf, size_t size _libctf_unused_)
{
#ifdef HAVE_MMAP
  /* Must be the same as the check in ctf_data_alloc().  */

  if (size > _PAGESIZE)
    (void) munmap (buf, size);
  else
    free (buf);
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
      if (ctf_pread (fd, data, size, offset) <= 0)
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
  /* Must be the same as the check in ctf_data_alloc().  */

  if (size > _PAGESIZE)
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

ssize_t
ctf_pread (int fd, void *buf, ssize_t count, off_t offset)
{
  ssize_t len;
  size_t acc = 0;
  char *data = (char *) buf;

#ifdef HAVE_PREAD
  while (count > 0)
    {
      if ((len = pread (fd, data, count, offset)) < 0)
	  return len;
      if (len == EINTR)
	continue;

      acc += len;
      if (len == 0)				/* EOF.  */
	return acc;

      count -= len;
      offset += len;
      data += len;
    }
  return acc;
#else
  off_t orig_off;

  if ((orig_off = lseek (fd, 0, SEEK_CUR)) < 0)
    return -1;
  if ((lseek (fd, offset, SEEK_SET)) < 0)
    return -1;

  while (count > 0)
    {
      if ((len = read (fd, data, count)) < 0)
	  return len;
      if (len == EINTR)
	continue;

      acc += len;
      if (len == 0)				/* EOF.  */
	break;

      count -= len;
      data += len;
    }
  if ((lseek (fd, orig_off, SEEK_SET)) < 0)
    return -1;					/* offset is smashed.  */
#endif

  return acc;
}

const char *
ctf_strerror (int err)
{
  return (const char *) (strerror (err));
}

void ctf_setdebug (int debug)
{
  /* Ensure that libctf_init_debug() has been called, so that we don't get our
     debugging-on-or-off smashed by the next call.  */

  libctf_init_debug();
  _libctf_debug = debug;
  ctf_dprintf ("CTF debugging set to %i\n", debug);
}

int ctf_getdebug (void)
{
  return _libctf_debug;
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
