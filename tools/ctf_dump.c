/*
   A simple CTF dumper.

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
#include <sys/ctf-api.h>
#include <zlib.h>

#define GZCHUNKSIZE (1024*512)	/* gzip uncompression chunk size.  */

static int ctf_type_print (ctf_id_t id, void *state);

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

static int
ctf_member_print (const char *name, ctf_id_t id, unsigned long offset,
		  int depth, void *state)
{
  ctf_file_t **fp = state;
  char buf[512];
  int i;

  printf ("	   ");
  for (i = 1; i < depth; i++)
    printf ("    ");

  ctf_type_lname (*fp, id, buf, sizeof (buf));
  printf ("    [0x%lx] (ID 0x%lx) (kind %i) %s %s (aligned at 0x%lx",
	  offset, id, ctf_type_kind (*fp, id), buf, name, ctf_type_align (*fp,
									  id));
  if ((ctf_type_kind (*fp, id) == CTF_K_INTEGER)
      || (ctf_type_kind (*fp, id) == CTF_K_FLOAT))
    {
      ctf_encoding_t ep;
      ctf_type_encoding (*fp, id, &ep);
      printf (", format 0x%x, offset 0x%x, bits 0x%x", ep.cte_format,
	      ep.cte_offset, ep.cte_bits);
    }
  printf (")\n");

  return 0;
}

static int
ctf_type_print (ctf_id_t id, void *state)
{
  ctf_file_t **fp = state;
  ctf_id_t ref, newref;
  char buf[512];

  ctf_type_lname (*fp, id, buf, sizeof (buf));
  printf ("    ID %lx", id);
  ref = id;
  while ((newref = ctf_type_reference (*fp, ref)) != CTF_ERR)
    {
      ref = newref;
      printf (" -> %lx", ref);
    }
  if (ctf_errno (*fp) != ECTF_NOTREF)
    {
      printf ("%p: reference lookup error: %s\n",
	      (void *) *fp, ctf_errmsg (ctf_errno (*fp)));
      return 0;
    }
  printf (": %s (size: %lx)\n", buf, ctf_type_size (*fp, id));

  ctf_type_visit (*fp, ref, ctf_member_print, state);

  return 0;
}

static int
ctf_var_print (const char *name, ctf_id_t id, void *state)
{
  ctf_file_t **fp = state;
  char buf[512];

  ctf_type_lname (*fp, id, buf, sizeof (buf));
  printf ("    %s -> ID %lx: %s\n", name, id, buf);
  return 0;
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

static void
dump_ctf (const char *file, ctf_file_t * fp, int quiet)
{
  const char *errmsg;
  int err;

  if (!quiet)
    printf ("\nCTF file: %s\n", file);

  printf ("\n  Types: \n");
  err = ctf_type_iter (fp, ctf_type_print, &fp);

  if (err != 0)
    {
      errmsg = "type";
      goto err;
    }

  printf ("\n  Variables: \n");
  err = ctf_variable_iter (fp, ctf_var_print, &fp);

  if (err != 0)
    {
      errmsg = "variable";
      goto err;
    }

  return;

err:
  fflush (stdout);
  fprintf (stderr, "%s %s iteration failed: %s\n", file, errmsg,
	   ctf_errmsg (err));
  exit (1);
}

static void
usage (int argc, char *argv[])
{
  fprintf (stderr, "Syntax: %s [-p parent-ctf] -n ctf...\n\n", argv[0]);
  fprintf (stderr, "-n: Do not dump parent's contents after loading.\n\n");
  fprintf (stderr, "-q: Quiet: do not dump the CTF filename.\n\n");
  fprintf (stderr, "-p is mandatory if any CTF files have parents.\n");
  fprintf (stderr, "If any CTF file has parents, all CTF files must have "
	   "the same parent.\n");
}

int
main (int argc, char *argv[])
{
  const char *parent = NULL;
  ctf_file_t *pfp = NULL;
  int opt;
  int skip_parent = 0;
  int quiet = 0;

  while ((opt = getopt (argc, argv, "hnqp:")) != -1)
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
    dump_ctf (parent, pfp, quiet);

  for (name = &argv[optind]; *name; name++)
    {
      ctf_file_t *fp = read_ctf (*name);
      ctf_sect_t sect;

      if (!fp)
	continue;

      if (parent)
	ctf_import (fp, pfp);

      dump_ctf (*name, fp, quiet);

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
