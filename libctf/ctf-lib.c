/* ELF opening and file writing.
   Copyright (c) 2003, 2019, Oracle and/or its affiliates. All rights reserved.

   Licensed under the Universal Permissive License v 1.0 as shown at
   http://oss.oracle.com/licenses/upl.

   Licensed under the GNU General Public License (GPL), version 2. See the file
   COPYING in the top level of this tree.  */

#include <ctf-impl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

void
libctf_init_debug (void)
{
  static int inited;
  if (!inited)
    {
      _libctf_debug = getenv ("LIBCTF_DEBUG") != NULL;
      inited = 1;
    }
}

/* Set the CTF library client version to the specified version.  If version is
   zero, we just return the default library version number.  */
int
ctf_version (int version)
{
  if (version < 0)
    {
      errno = EINVAL;
      return -1;
    }

  if (version > 0)
    {
      /*  Dynamic version switching is not presently supported. */
      if (version != CTF_VERSION)
	{
	  errno = ENOTSUP;
	  return -1;
	}
      ctf_dprintf ("ctf_version: client using version %d\n", version);
      _libctf_version = version;
    }

  return _libctf_version;
}
