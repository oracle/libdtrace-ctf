/* Escape hatch for autoconfiscated declarations.
   Copyright (c) 2019, Oracle and/or its affiliates. All rights reserved.

   Licensed under the Universal Permissive License v 1.0 as shown at
   http://oss.oracle.com/licenses/upl.

   Licensed under the GNU General Public License (GPL), version 2. See the file
   COPYING in the top level of this tree.

   Not synced with GNU.  */

#ifndef	_CTF_DECLS_H
#define	_CTF_DECLS_H

#include "config.h"
#include <string.h>

#define ctf_qsort_r(base, nmemb, size, compar, arg)	\
  qsort_r ((base), (nmemb), (size), (compar), (arg))

#ifndef HAVE_BSEARCH_R
extern void *
bsearch_r (register const void *key, const void *base0,
	   size_t nmemb, register size_t size,
	   register int (*compar)(const void *, const void *, void *),
	   void *arg);
#endif

#define xstrdup(str) strdup (str)
#define xstrndup(str, n) strndup (str, n)

/* Work around recent changes in binutils APIs.

   Since most of them are macros, we have to outright redeclare them, since we
   can't wrap them.  */

#ifndef HAVE_ONE_ARG_BFD_SECTION_SIZE
#include <bfd.h>
#undef bfd_section_size
#define bfd_section_size(ptr) ((ptr)->size)
#endif

#endif
