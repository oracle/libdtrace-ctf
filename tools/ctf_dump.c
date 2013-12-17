/*
 * A simple CTF dumper.
 *
 * Copyright 2012, 2013 Oracle, Inc.
 *
 * Licensed under the GNU General Public License (GPL), version 2. See the file
 * COPYING in the top level of this tree.
 */

#define _GNU_SOURCE 1
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <sys/ctf_api.h>
#include <zlib.h>

#define GZCHUNKSIZE (1024*512)		    /* gzip uncompression chunk size */

static int
ctf_type_print(ctf_id_t id, void *state);

static ctf_sect_t
ctf_uncompress(const char *name)
{
	gzFile f;
	size_t chunklen;
	char buf[8192];
	const char *errstr;
	int err;

	size_t len = 0;
	char *result = NULL;
	ctf_sect_t sect = {0};

	f = gzopen(name, "r");
	if (!f)
		return sect;

	while ((chunklen = gzread(f, buf, 8192)) > 0) {
		result = realloc(result, len + chunklen);
		if (result == NULL) {
			fprintf(stderr, "Cannot reallocate: OOM\n");
			exit(1);
		}
		memcpy(result + len, buf, chunklen);
		len += chunklen;
	}

	errstr = gzerror(f, &err);
	if ((err != Z_OK) && (err != Z_STREAM_END)) {
		fprintf(stderr, "zlib error: %s\n", errstr);
		exit(2);
	}

	gzclose(f);

	sect.cts_data = result;
	sect.cts_size = len;

	return sect;
}

static int
ctf_member_print(const char *name, ctf_id_t id, ulong_t offset, int depth,
    void *state)
{
	ctf_file_t **fp = state;
	char buf[512];
	int i;

	printf("           ");
	for (i = 1; i < depth; i++)
		printf("    ");

	ctf_type_lname(*fp, id, buf, sizeof (buf));
	printf("    [%lx] (ID %lx) %s %s (aligned at %lx)\n", offset, id, buf,
	    name, ctf_type_align(*fp, id));

	return 0;
}

static int
ctf_type_print(ctf_id_t id, void *state)
{
	ctf_file_t **fp = state;
	char buf[512];

	ctf_type_lname(*fp, id, buf, sizeof (buf));
	printf("    ID %lx: %s\n", id, buf);

	ctf_type_visit(*fp, id, ctf_member_print, state);

	return 0;
}

static int
ctf_var_print(const char *name, ctf_id_t id, void *state)
{
	ctf_file_t **fp = state;
	char buf[512];

	ctf_type_lname(*fp, id, buf, sizeof (buf));
	printf("    %s -> ID %lx: %s\n", name, id, buf);
	return 0;
}

static ctf_file_t *
read_ctf(const char *file)
{
        ctf_sect_t sect = ctf_uncompress(file);
        ctf_file_t *ctfp;
        int err = 0;

        if (sect.cts_data == NULL) {
                fprintf(stderr, "%s open failure\n", file);
                exit(1);
        }

	/*
	 * Skip 'CTF' files with no CTF data in them (there to placate the
	 * kernel's build system).
	 */
	if ((sect.cts_size == 0) || (sect.cts_size == 1))
		return (NULL);

        ctfp = ctf_bufopen(&sect, NULL, NULL, &err);

        if (err != 0) {
                fprintf(stderr, "%s bufopen failure: %s\n",
                    file, ctf_errmsg(err));
                exit(1);
        }

        return (ctfp);
}

static void
dump_ctf(const char *file, ctf_file_t *fp)
{
        const char *errmsg;
        int err;

        printf("\nCTF file: %s\n", file);

        printf ("\n  Types: \n");
        err = ctf_type_iter(fp, ctf_type_print, &fp);

        if (err != 0) {
                errmsg = "type";
                goto err;
        }

        printf ("\n  Variables: \n");
        err = ctf_variable_iter(fp, ctf_var_print, &fp);

        if (err != 0) {
                errmsg = "variable";
                goto err;
        }

        return;

err:
        fflush(stdout);
        fprintf(stderr, "%s %s iteration failed: %s\n", file, errmsg,
            ctf_errmsg(err));
        exit(1);
}

static void
usage(int argc, char *argv[])
{
        printf("Syntax: %s [-p parent-ctf] ctf...\n\n", argv[0]);
        printf("-p is mandatory if any CTF files have parents.\n");
        printf("If any CTF file has parents, all CTF files must have the "
            "same parent.\n");
}

int
main(int argc, char *argv[])
{
        const char *parent = NULL;
        ctf_file_t *pfp = NULL;
        char opt;

        while ((opt = getopt(argc, argv, "hp:")) != -1) {
                switch (opt) {
                case 'h':
                        usage(argc, argv);
                        exit(1);
                case 'p':
                        parent = optarg;
                        pfp = read_ctf(parent);
                }
        }

	char **name;

        if (pfp)
                dump_ctf(parent, pfp);

        for (name = &argv[optind]; *name; name++) {
		ctf_file_t *fp = read_ctf(*name);

		if (!fp)
		    continue;

                if (parent)
                        ctf_import(fp, pfp);

                dump_ctf(*name, fp);

		ctf_close(fp);
	}

        if (pfp)
                ctf_close(pfp);

        return 0;
}
