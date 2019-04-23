/* A simple CTF dumper.

   Copyright (c) 2012, 2019, Oracle and/or its affiliates. All rights reserved.

   Licensed under the Universal Permissive License v 1.0 as shown at
   http://oss.oracle.com/licenses/upl.

   Licensed under the GNU General Public License (GPL), version 2. See the file
   COPYING in the top level of this tree.  */

#define _GNU_SOURCE 1
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <ctf-impl.h>
#include <sys/ctf-api.h>
#include <zlib.h>

#define GZCHUNKSIZE (1024*512)	/* gzip uncompression chunk size.  */

static ctf_sect_t
ctf_uncompress (const char *name)
{
  gzFile f;
  size_t chunklen;
  char buf[8192];
  const char *errstr;
  int err;

  size_t len = 0;
  char *result = NULL;
  ctf_sect_t sect = { 0 };

  f = gzopen (name, "r");
  if (!f)
    return sect;

  while ((chunklen = gzread (f, buf, 8192)) > 0)
    {
      result = realloc (result, len + chunklen);
      if (result == NULL)
	{
	  fprintf (stderr, "Cannot reallocate: OOM\n");
	  exit (1);
	}
      memcpy (result + len, buf, chunklen);
      len += chunklen;
    }

  errstr = gzerror (f, &err);
  if ((err != Z_OK) && (err != Z_STREAM_END))
    {
      fprintf (stderr, "zlib error: %s\n", errstr);
      exit (2);
    }

  gzclose (f);

  sect.cts_data = result;
  sect.cts_size = len;

  return sect;
}

static ctf_file_t *
read_ctf (const char *file)
{
  ctf_sect_t sect = ctf_uncompress (file);
  ctf_file_t *ctfp;
  int err = 0;

  if (sect.cts_data == NULL)
    {
      fprintf (stderr, "%s open failure\n", file);
      exit (1);
    }

  /* Skip 'CTF' files with no CTF data in them (there to placate the
     kernel's build system).  */

  if ((sect.cts_size == 0) || (sect.cts_size == 1))
    return (NULL);

  ctfp = ctf_bufopen (&sect, NULL, NULL, &err);

  if (err != 0)
    {
      fprintf (stderr, "%s bufopen failure: %s\n", file, ctf_errmsg (err));
      exit (1);
    }

  return (ctfp);
}

static char *indent_lines (ctf_sect_names_t sect _libctf_unused_,
			   char *line, void *arg)
{
  char *spaces = arg;
  char *new_str;

  if (asprintf (&new_str, "%s%s", spaces, line) < 0)
    return line;
  return new_str;
}

static void
dump_ctf (const char *file, ctf_file_t *fp, int quiet, const char *one_section)
{
  const char *things[] = {"Labels", "Data objects", "Function objects",
			  "Variables", "Types", "Strings", ""};
  int i;
  const char **thing;

  if (!quiet)
    printf ("\nCTF file: %s\n", file);

  for (i = 1, thing = things; *thing[0] ; thing++, i++)
    {
      ctf_dump_state_t *s = NULL;
      char *item;

      if (one_section && strcmp (one_section, *thing) != 0)
	continue;

      printf ("\n  %s:\n", *thing);
      while ((item = ctf_dump (fp, &s, i, indent_lines,
			       (void *) "    ")) != NULL)
	{
	  printf ("%s\n", item);
	  free (item);
	}

      if (ctf_errno (fp))
	goto err;
    }

  return;

err:
  fflush (stdout);
  fprintf (stderr, "%s %s iteration failed: %s\n", file, *thing,
	   ctf_errmsg (ctf_errno (fp)));
  exit (1);
}

static void
usage (int argc _libctf_unused_, char *argv[])
{
  fprintf (stderr, "Syntax: %s [-p parent-ctf] [-s section] q -n ctf...\n\n", argv[0]);
  fprintf (stderr, "-n: Do not dump parent's contents after loading.\n\n");
  fprintf (stderr, "-q: Quiet: do not dump the CTF filename.\n\n");
  fprintf (stderr, "-p is mandatory if any CTF files have parents.\n");
  fprintf (stderr, "If any CTF file has parents, all CTF files must have "
	   "the same parent.\n");
  fprintf (stderr, "-s NAME: only dump one section (names are the same as in the full output\n");
  fprintf (stderr, "         e.g. \"Types\" or \"Data objects\".\n");
}

int
main (int argc, char *argv[])
{
  const char *parent = NULL;
  const char *one_section = NULL;
  ctf_file_t *pfp = NULL;
  int opt;
  int skip_parent = 0;
  int quiet = 0;

  while ((opt = getopt (argc, argv, "hnqp:s:")) != -1)
    {
      switch (opt)
	{
	case 'h':
	  usage (argc, argv);
	  exit (1);
	case 'p':
	  parent = optarg;
	  pfp = read_ctf (parent);
	  break;
	case 's':
	  one_section = optarg;
	  break;
	case 'q':
	  quiet = 1;
	  break;
	case 'n':
	  skip_parent = 1;
	  break;
	}
    }

  char **name;

  if (pfp && !skip_parent)
    dump_ctf (parent, pfp, quiet, one_section);

  for (name = &argv[optind]; *name; name++)
    {
      ctf_file_t *fp = read_ctf (*name);
      ctf_sect_t sect;

      if (!fp)
	continue;

      if (parent)
	ctf_import (fp, pfp);

      dump_ctf (*name, fp, quiet, one_section);

      sect = ctf_getdatasect (fp);
      ctf_close (fp);
      free ((void *) sect.cts_data);
    }

  if (pfp)
    {
      ctf_sect_t sect;
      sect = ctf_getdatasect (pfp);
      ctf_close (pfp);
      free ((void *) sect.cts_data);
    }

  return 0;
}
