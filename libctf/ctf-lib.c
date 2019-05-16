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
#include <zlib.h>

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

/* Write the compressed CTF data stream to the specified gzFile descriptor.
   This is useful for saving the results of dynamic CTF containers.  */
int
ctf_gzwrite (ctf_file_t *fp, gzFile fd)
{
  const unsigned char *buf = fp->ctf_base;
  ssize_t resid = fp->ctf_size;
  ssize_t len;

  while (resid != 0)
    {
      if ((len = gzwrite (fd, buf, resid)) <= 0)
	return (ctf_set_errno (fp, errno));
      resid -= len;
      buf += len;
    }

  return 0;
}

/* Compress the specified CTF data stream and write it to the specified file
   descriptor.  */
int
ctf_compress_write (ctf_file_t *fp, int fd)
{
  unsigned char *buf;
  unsigned char *bp;
  ctf_header_t h;
  ctf_header_t *hp = &h;
  ssize_t header_len = sizeof (ctf_header_t);
  ssize_t compress_len;
  size_t max_compress_len = compressBound (fp->ctf_size - header_len);
  ssize_t len;
  int rc;
  int err = 0;

  memcpy (hp, fp->ctf_base, header_len);
  hp->cth_flags |= CTF_F_COMPRESS;

  if ((buf = ctf_data_alloc (max_compress_len)) == NULL)
    return (ctf_set_errno (fp, ECTF_ZALLOC));

  compress_len = max_compress_len;
  if ((rc = compress (buf, (uLongf *) & compress_len,
		      fp->ctf_base + header_len,
		      fp->ctf_size - header_len)) != Z_OK)
    {
      ctf_dprintf ("zlib deflate err: %s\n", zError (rc));
      err = ctf_set_errno (fp, ECTF_COMPRESS);
      goto ret;
    }

  while (header_len > 0)
    {
      if ((len = write (fd, hp, header_len)) < 0)
	{
	  err = ctf_set_errno (fp, errno);
	  goto ret;
	}
      header_len -= len;
      hp += len;
    }

  bp = buf;
  while (compress_len > 0)
    {
      if ((len = write (fd, bp, compress_len)) < 0)
	{
	  err = ctf_set_errno (fp, errno);
	  goto ret;
	}
      compress_len -= len;
      bp += len;
    }

ret:
  ctf_data_free (buf, max_compress_len);
  return err;
}

/* Write the uncompressed CTF data stream to the specified file descriptor.
   This is useful for saving the results of dynamic CTF containers.  */
int
ctf_write (ctf_file_t *fp, int fd)
{
  const unsigned char *buf = fp->ctf_base;
  ssize_t resid = fp->ctf_size;
  ssize_t len;

  while (resid != 0)
    {
      if ((len = write (fd, buf, resid)) < 0)
	return (ctf_set_errno (fp, errno));
      resid -= len;
      buf += len;
    }

  return 0;
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
