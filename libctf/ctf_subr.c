/*
 * Copyright 2003 -- 2012 Oracle, Inc.
 *
 * Licensed under the GNU General Public License (GPL), version 2. See the file
 * COPYING in the top level of this tree.
 */

#include <ctf_impl.h>
#include <libctf.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <string.h>

void *
ctf_data_alloc(size_t size)
{
	return (mmap(NULL, size, PROT_READ | PROT_WRITE,
	    MAP_PRIVATE | MAP_ANON, -1, 0));
}

void
ctf_data_free(void *buf, size_t size)
{
	(void) munmap(buf, size);
}

void
ctf_data_protect(void *buf, size_t size)
{
	(void) mprotect(buf, size, PROT_READ);
}

void *
ctf_alloc(size_t size)
{
	return (malloc(size));
}

/*ARGSUSED*/
void
ctf_free(void *buf, size_t size)
{
	free(buf);
}

const char *
ctf_strerror(int err)
{
	return (const char *)(strerror(err));
}

/*PRINTFLIKE1*/
_dt_printflike_(1,2)
void
ctf_dprintf(const char *format, ...)
{
	if (_libctf_debug) {
		va_list alist;

		va_start(alist, format);
		(void) fputs("libctf DEBUG: ", stderr);
		(void) vfprintf(stderr, format, alist);
		va_end(alist);
	}
}
