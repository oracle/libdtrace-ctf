/*
 * Copyright (c) 2011, 2012, Oracle and/or its affiliates. All rights reserved.
 *
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 *
 * Licensed under the GNU General Public License (GPL), version 2.
 */

#ifndef CTF_SYS_TYPES_H
#define CTF_SYS_TYPES_H

#include <stdint.h>

/*
 * POSIX Extensions
 */
typedef unsigned char   uchar_t;
typedef unsigned short  ushort_t;
typedef unsigned int    uint_t;
typedef unsigned long   ulong_t;

/*
 * return x rounded up to an alignment boundary
 * eg, P2ROUNDUP(0x1234, 0x100) == 0x1300 (0x13*align)
 * eg, P2ROUNDUP(0x5600, 0x100) == 0x5600 (0x56*align)
 */
#define	P2ROUNDUP(x, align)		(-(-(x) & -(align)))

#endif
