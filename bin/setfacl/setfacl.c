/*
 * Copyright (c) 2001 Chris D. Faulhaber
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR THE VOICES IN HIS HEAD BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/acl.h>
#include <sys/queue.h>

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

#include "setfacl.h"

static void   add_filename(const char *filename);
static acl_t *get_file_acls(const char *filename);
static void   usage(void);

static void
add_filename(const char *filename)
{
	struct sf_file *file;

	if (strlen(filename) > PATH_MAX - 1) {
		warn("illegal filename");
		return;
	}
	file = zmalloc(sizeof(struct sf_file));
	file->filename = filename;
	STAILQ_INSERT_TAIL(&filelist, file, next);
}

static acl_t *
get_file_acls(const char *filename)
{
	acl_t *acl;
	struct stat sb;

	if (stat(filename, &sb) == -1) {
		warn("stat() of %s failed", filename);
		return NULL;
	}

	acl = zmalloc(sizeof(acl_t) * 2);
	acl[0] = acl_get_file(filename, ACL_TYPE_ACCESS);
	if (!acl[0])
		err(EX_OSERR, "acl_get_file() failed");
	if (S_ISDIR(sb.st_mode)) {
		acl[1] = acl_get_file(filename, ACL_TYPE_DEFAULT);
		if (!acl[1])
			err(EX_OSERR, "acl_get_file() failed");
	} else
		acl[1] = NULL;

	return acl;
}

static void
usage(void)
{

	fprintf(stderr, "usage: setfacl [-bdknv] [-m entries] [-M file1] "
	    "[-x entries] [-X file2] [file ...]\n");
	exit(EX_USAGE);
}

int
main(int argc, char *argv[])
{
	acl_t *acl, final_acl;
	char filename[PATH_MAX];
	int local_error, carried_error, ch, i;
	struct sf_file *file;
	struct sf_entry *entry;

	acl_type = ACL_TYPE_ACCESS;
	carried_error = local_error = 0;
	have_mask = have_stdin = n_flag = need_mask = 0;

	STAILQ_INIT(&entrylist);
	STAILQ_INIT(&filelist);

	while ((ch = getopt(argc, argv, "M:X:bdkm:nx:")) != -1)
		switch(ch) {
		case 'M':
			entry = zmalloc(sizeof(struct sf_entry));
			entry->acl = get_acl_from_file(optarg);
			if (!entry->acl)
				err(EX_OSERR, "get_acl_from_file() failed");
			entry->op = OP_MERGE_ACL;
			STAILQ_INSERT_TAIL(&entrylist, entry, next);
			break;
		case 'X':
			entry = zmalloc(sizeof(struct sf_entry));
			entry->acl = get_acl_from_file(optarg);
			entry->op = OP_REMOVE_ACL;
			STAILQ_INSERT_TAIL(&entrylist, entry, next);
			break;
		case 'b':
			entry = zmalloc(sizeof(struct sf_entry));
			entry->op = OP_REMOVE_EXT;
			STAILQ_INSERT_TAIL(&entrylist, entry, next);
			break;
		case 'd':
			acl_type = ACL_TYPE_DEFAULT;
			break;
		case 'k':
			entry = zmalloc(sizeof(struct sf_entry));
			entry->op = OP_REMOVE_DEF;
			STAILQ_INSERT_TAIL(&entrylist, entry, next);
			break;
		case 'm':
			entry = zmalloc(sizeof(struct sf_entry));
			entry->acl = acl_from_text(optarg);
			if (!entry->acl)
				err(EX_USAGE, "acl_from_text() failed");
			entry->op = OP_MERGE_ACL;
			STAILQ_INSERT_TAIL(&entrylist, entry, next);
			break;
		case 'n':
			n_flag++;
			break;
		case 'x':
			entry = zmalloc(sizeof(struct sf_entry));
			entry->acl = acl_from_text(optarg);
			if (!entry->acl)
				err(EX_USAGE, "acl_from_text() failed");
			entry->op = OP_REMOVE_ACL;
			STAILQ_INSERT_TAIL(&entrylist, entry, next);
			break;
		default:
			usage();
			break;
		}
	argc -= optind;
	argv += optind;

	if (STAILQ_EMPTY(&entrylist))
		usage();

	/* take list of files from stdin */
	if (argc == 0 || !strcmp(argv[0], "-")) {
		if (have_stdin)
			err(EX_USAGE, "cannot have more than one stdin");
		have_stdin = 1;
		bzero(&filename, sizeof(filename));
		while (fgets(filename, sizeof(filename), stdin)) {
			/* remove the \n */
			filename[strlen(filename) - 1] = '\0';
			add_filename(filename);
		}
	} else
		for (i = 0; i < argc; i++)
			add_filename(argv[i]);

	/* cycle through each file */
	STAILQ_FOREACH(file, &filelist, next) {
		/* get our initial access and default ACL's */
		acl = get_file_acls(file->filename);
		if (!acl)
			continue;
		if ((acl_type == ACL_TYPE_DEFAULT) && !acl[1]) {
			warnx("Default ACL not valid for %s", file->filename);
			continue;
		}

		local_error = 0;

		/* cycle through each option */
		STAILQ_FOREACH(entry, &entrylist, next) {
			if (local_error)
				continue;

			switch(entry->op) {
			case OP_MERGE_ACL:
				local_error += merge_acl(entry->acl, acl);
				need_mask = 1;
				break;
			case OP_REMOVE_EXT:
				remove_ext(acl);
				need_mask = 0;
				break;
			case OP_REMOVE_DEF:
				if (acl_delete_def_file(file->filename) == -1) {
					warn("acl_delete_def_file() failed");
					local_error++;
				}
				local_error += remove_default(acl);
				need_mask = 0;
				break;
			case OP_REMOVE_ACL:
				local_error += remove_acl(entry->acl, acl);
				need_mask = 1;
				break;
			/* NOTREACHED */
			}
		}

		/* don't bother setting the ACL if something is broken */
		if (local_error) {
			carried_error++;
			continue;
		}

		if (acl_type == ACL_TYPE_ACCESS)
			final_acl = acl[0];
		else
			final_acl = acl[1];

		if (need_mask && (set_acl_mask(final_acl) == -1)) {
			warnx("failed to set ACL mask on %s", file->filename);
			carried_error++;
		} else if (acl_set_file(file->filename, acl_type,
		    final_acl) == -1) {
			carried_error++;
			warn("acl_set_file() failed for %s", file->filename);
		}

		acl_free(acl[0]);
		acl_free(acl[1]);
		free(acl);
	}

	return carried_error;
}
