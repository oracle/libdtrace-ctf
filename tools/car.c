/* CTF archiver.
 *
 * Only extraction for now.
 *
 * Copyright 2017 Oracle, Inc.
 *
 * Licensed under the GNU General Public License (GPL), version 2. See the file
 * COPYING in the top level of this tree.
 */

#define _GNU_SOURCE 1
#include <sys/ctf_api.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ctf_impl.h>

static void
usage(int argc, char *argv[])
{
	fprintf(stderr, "Syntax: %s {-x|-t} [-v] -i parent-ctf] archive...\n\n", argv[0]);
	fprintf(stderr, "-x: Extract archive contents.\n");
	fprintf(stderr, "-t: List archive contents without extraction (default).\n");
	fprintf(stderr, "-v: List archive contents while extracting.\n");
}

static int extraction = 0;
static int listing_explicit = 0;
static int quiet = 0;

struct visit_data {
	const char *name;
	ctf_file_t *fp;
	int printed_header;
	int colsize;
};

/*
 * Compute the size of column needed to print the names of all archive members.
 */
static int compute_colsize(ctf_file_t *fp, const char *name, void *data)
{
	struct visit_data *d = data;

	if ((name != NULL) && (strlen(name) > d->colsize))
	    d->colsize = strlen(name);

	return(0);
}
const char *
ctf_strraw(ctf_file_t *fp, uint_t name)
{
	ctf_strs_t *ctsp = &fp->ctf_str[CTF_NAME_STID(name)];

	if (ctsp->cts_strs != NULL && CTF_NAME_OFFSET(name) < ctsp->cts_len)
		return (ctsp->cts_strs + CTF_NAME_OFFSET(name));

	/* string table not loaded or corrupt offset */
	return (NULL);
}

const char *
ctf_strptr(ctf_file_t *fp, uint_t name)
{
	const char *s = ctf_strraw(fp, name);
	return (s != NULL ? s : "(?)");
}

static int print_extract_ctf(ctf_file_t *fp, const char *name, void *data)
{
	struct visit_data *d = data;
	char fn[PATH_MAX];

	if (!quiet) {
		if (!d->printed_header) {
			printf("\n%s:\n\n", d->name);
			printf("%-*s %-10s %-8s %-8s\n\n",
			    d->colsize, "Name", "Size", "Types", "Vars");
			d->printed_header = 1;
		}
		printf("%-*s %-10zi %-8zi %-8zi\n", d->colsize, name, fp->ctf_size,
		    fp->ctf_typemax, fp->ctf_nvars);
	}
	snprintf(fn, sizeof(fn), "%s.ctf", name);

	if (extraction) {
		int fd;

		if ((fd = open(fn, O_WRONLY | O_CREAT | O_TRUNC |
			    O_CLOEXEC, 0666)) < 0) {
			fprintf(stderr, "Cannot open %s: %s\n", fn,
			    strerror(errno));
			exit(1);
		}
		if (ctf_compress_write(fp, fd) < 0) {
			fprintf(stderr, "Cannot write to %s: %s\n",
			    fn, ctf_errmsg(ctf_errno(fp)));
			exit(1);
		}
		close(fd);
	}
	return(0);
}

int
main(int argc, char *argv[])
{
	char **name;
	char opt;

	while ((opt = getopt(argc, argv, "hxtvi:")) != -1) {
		switch (opt) {
		case 'h':
			usage(argc, argv);
			exit(1);
		case 'x':
			if (listing_explicit) {
				fprintf(stderr, "Cannot specify both -x and -t.\n");
				exit(1);
			}
			extraction = 1;
			quiet = 1;
			break;
		case 't':
			if (extraction) {
				fprintf(stderr, "Cannot specify both -x and -t.\n");
				exit(1);
			}
			listing_explicit = 1;
			break;
		case 'v':
			quiet = 0;
		}
	}

	for (name = &argv[optind]; *name; name++) {
		int err;
		ctf_archive_t *arc;
		struct visit_data visit_data;

		memset(&visit_data, 0, sizeof(struct visit_data));
		visit_data.name = *name;
		visit_data.colsize = 0;

		arc = ctf_arc_open(*name, &err);
		if (!arc) {
			fprintf(stderr, "Cannot open %s: %s\n", *name,
			    ctf_errmsg(err));
			continue;
		}
		if ((err = ctf_archive_iter(arc, compute_colsize, &visit_data)) < 0) {
			fprintf(stderr, "Error reading archive %s for colsize "
			    "computation: %s\n", *name, ctf_errmsg(err));
			exit(1);
		}
		visit_data.colsize += 2;

		if ((err = ctf_archive_iter(arc, print_extract_ctf, &visit_data)) < 0) {
			fprintf(stderr, "Error reading archive %s: %s\n", *name,
			    ctf_errmsg(err));
			exit(1);
		}
		ctf_arc_close(arc);
	}

	return 0;
}
