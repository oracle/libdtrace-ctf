/* Endianness-swapping wrapper.
   Copyright (c) 2006, 2019, Oracle and/or its affiliates. All rights reserved.

   Licensed under the Universal Permissive License v 1.0 as shown at
   http://oss.oracle.com/licenses/upl.

   Licensed under the GNU General Public License (GPL), version 2. See the file
   COPYING in the top level of this tree.

   Not synced with GNU.  */

#ifndef	_CTF_ENDIAN_H
#define	_CTF_ENDIAN_H

#include "config.h"
#include <stdint.h>

#ifdef HAVE_ENDIAN_H
#include <endian.h>
#else
#error <endian.h> is required.
#endif

#endif
