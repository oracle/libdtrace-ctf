/* Simple subrs.
   Copyright (c) 2003, 2019, Oracle and/or its affiliates. All rights reserved.

   Licensed under the Universal Permissive License v 1.0 as shown at
   http://oss.oracle.com/licenses/upl.

   Licensed under the GNU General Public License (GPL), version 2. See the file
   COPYING in the top level of this tree.  */

#include <ctf-impl.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <string.h>

void *
ctf_data_alloc (size_t size)
{
  return (mmap (NULL, size, PROT_READ | PROT_WRITE,
		MAP_PRIVATE | MAP_ANON, -1, 0));
}

void
ctf_data_free (void *buf, size_t size)
{
  (void) munmap (buf, size);
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
