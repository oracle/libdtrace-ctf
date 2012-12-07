/*
 * Copyright 2001 -- 2012 Oracle, Inc.
 *
 * Licensed under the GNU General Public License (GPL), version 2. See the file
 * COPYING in the top level of this tree.
 */

/*
 * This header file defines the interfaces available from the CTF debugger
 * library, libctf.  This library provides functions that a debugger can
 * use to operate on data in the Compact ANSI-C Type Format (CTF).
 */

#ifndef	_LIBCTF_H
#define	_LIBCTF_H

#include <sys/ctf_api.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * This flag can be used to enable debug messages.
 */
extern int _libctf_debug;

#ifdef	__cplusplus
}
#endif

#endif	/* _LIBCTF_H */
