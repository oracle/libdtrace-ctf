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

#define xstrdup(str) strdup (str)
#define xstrndup(str, n) strndup (str, n)

#endif
