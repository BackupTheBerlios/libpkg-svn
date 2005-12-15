/*
 * Copyright (C) 2005, Andrew Turner, All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name(s) of the author(s) may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <sys/param.h>
#include <sys/types.h>

#include <assert.h>
#include <md5.h>
#include <stdlib.h>
#include <string.h>

#include "pkg.h"
#include "pkg_freebsd.h"
#include "pkg_private.h"
#include "pkg_freebsd_private.h"

const char *pkg_freebsd_contents_line_str[] = {
	"",
	"@comment",
	"@name",
	"@cwd",
	"@pkgdep",
	"@conflicts",
	"@exec",
	"@unexec",
	"@ignore",
	"@dirrm",
	"@mtree",
	NULL
};

struct pkg_freebsd_contents *
pkg_freebsd_contents_new(const char *contents)
{
	struct pkg_freebsd_contents *cont;
	unsigned int pos;

	cont = malloc(sizeof(struct pkg_freebsd_contents));
	if (!cont)
		return NULL;

	if (contents == NULL) {
		cont->file = NULL;
		cont->line_count = 0;
		cont->line_size = 0;
		cont->lines = NULL;
	} else {
		cont->file = strdup(contents);
		if (!cont->file) {
			free(cont);
			return NULL;
		}
		cont->lines = NULL;

		pos = 0;
		cont->line_count = 0;
		while (cont->file[pos] != '\0') {
			if (cont->file[pos] == '\n')
				cont->line_count++;
			pos++;
		}
		/* Check the last line contains data */
		if (pos > 0 && cont->file[pos-1] != '\n')
			cont->line_count++;

		if (cont->line_count == 0) {
			pkg_freebsd_contents_free(cont);
			return NULL;
		}
		cont->lines = malloc(sizeof(struct pkg_freebsd_contents_line) *
		    cont->line_count);
		if (!cont->lines) {
			pkg_freebsd_contents_free(cont);
			return NULL;
		}

		/*
		 * Make each line in cont->lines point to the start of it's line
		 * and be a valid string
		 */
		cont->lines[0].line = cont->file;
		pos = 1;
		while (pos < cont->line_count) {
			cont->lines[pos].line = strchr(cont->lines[pos-1].line, '\n');
			if (cont->lines[pos].line) {
				/* Terminate the last line */
				cont->lines[pos].line[0] = '\0';
				cont->lines[pos].line++;
			} else
				break;
			pos++;
		}
		/* The last line may need to be terminated at the correct place */
		pos = strlen(cont->lines[cont->line_count-1].line);
		if (cont->lines[cont->line_count-1].line[--pos] == '\n') {
			cont->lines[cont->line_count-1].line[pos] = '\0';
		}

		/*
	         * Set the data part of the line. ie not the control word 
	         * Set the line_type
	         */
		pos = 0;
		while (pos < cont->line_count) {
			char *space;

			space = strchr(cont->lines[pos].line, ' ');
			if (space && space[0] != '\0') {
				space[0] = '\0';
				space++;
				if (space[0] != '\0')
					cont->lines[pos].data = space;
			}

			/* Get the correct line type for the line */
			if (cont->lines[pos].line[0] != '@') {
				cont->lines[pos].line_type = PKG_LINE_FILE;
			} else if (!strcmp(cont->lines[pos].line, "@comment")) {
				cont->lines[pos].line_type = PKG_LINE_COMMENT;
			} else if (!strcmp(cont->lines[pos].line, "@name")) {
				cont->lines[pos].line_type = PKG_LINE_NAME;
			} else if (!strcmp(cont->lines[pos].line, "@cwd")) {
				cont->lines[pos].line_type = PKG_LINE_CWD;
			} else if (!strcmp(cont->lines[pos].line, "@pkgdep")) {
				cont->lines[pos].line_type = PKG_LINE_PKGDEP;
			} else if (!strcmp(cont->lines[pos].line, "@conflicts"))
			    {
				cont->lines[pos].line_type = PKG_LINE_CONFLICTS;
			} else if (!strcmp(cont->lines[pos].line, "@exec")) {
				cont->lines[pos].line_type = PKG_LINE_EXEC;
			} else if (!strcmp(cont->lines[pos].line, "@unexec")) {
				cont->lines[pos].line_type = PKG_LINE_UNEXEC;
			} else if (!strcmp(cont->lines[pos].line, "@ignore")) {
				cont->lines[pos].line_type = PKG_LINE_IGNORE;
			} else if (!strcmp(cont->lines[pos].line, "@dirrm")) {
				cont->lines[pos].line_type = PKG_LINE_DIRRM;
			} else if (!strcmp(cont->lines[pos].line, "@mtree")) {
				cont->lines[pos].line_type = PKG_LINE_MTREE;
			} else {
				cont->lines[pos].line_type = PKG_LINE_UNKNOWN;
				fprintf(stderr, "Unknown line type %s\n",
				    cont->lines[pos].line);
			}
			pos++;
		}
	}
	return cont;
}

/*
 * Adds a line of type with the value of data the fiven contents file
 */
int
pkg_freebsd_contents_add_line(struct pkg_freebsd_contents *contents, int type,
	const char *data)
{
	if (!contents || !data)
		return -1;

	if (!(type > 0 && type <= PKG_LINE_FILE)) {
		return -1;
	}

	/* Add the lines to the +CONTENTS file */
	contents->line_size += sizeof(struct pkg_freebsd_contents_line);
	if (contents->lines == NULL)
		contents->lines = malloc(contents->line_size);
	else
		contents->lines = realloc(contents->lines, contents->line_size);

	/* Init the values */
	contents->lines[contents->line_count].data = NULL;
	contents->lines[contents->line_count].line = NULL;
	contents->lines[contents->line_count].line_type = type;

	/*
	 * If the line is a file then the line will be the filename,
	 * the data will be NULL
	 */
	if (type == PKG_LINE_FILE) {
		contents->lines[contents->line_count].line = strdup(data);
		assert(contents->lines[contents->line_count].line != NULL);
	} else {
		contents->lines[contents->line_count].line =
		    strdup(pkg_freebsd_contents_line_str[type]);
		contents->lines[contents->line_count].data = strdup(data);
		assert(contents->lines[contents->line_count].data != NULL);
	}
	contents->line_count++;

	return 0;
}

int
pkg_freebsd_contents_add_dependency(struct pkg_freebsd_contents *contents,
		struct pkg *pkg)
{
	char *origin;

	if (contents == NULL || contents->file != NULL || pkg == NULL)
		return -1;

	if (pkg_freebsd_contents_add_line(contents, PKG_LINE_PKGDEP,
	    pkg_get_name(pkg)) != 0) {
		return -1;
	}

	origin = pkg_freebsd_get_origin(pkg);
	if (origin != NULL) {
		char *data;

		asprintf(&data, "ORIGIN:%s", origin);
		if (pkg_freebsd_contents_add_line(contents, PKG_LINE_COMMENT,
		    data) != 0) {
			free(data);
			return -1;
		}
		free(data);
	}	

	return -1;
}

/*
 * Add's a file the the +CONTENTS file
 */
int
pkg_freebsd_contents_add_file(struct pkg_freebsd_contents *contents,
		struct pkg_file *file)
{
	char md5[33], tmp[37];

	if (contents == NULL || contents->file != NULL || file == NULL)
		return -1;

	if (pkg_freebsd_contents_add_line(contents, PKG_LINE_FILE,
	    file->filename) != 0) {
		return -1;
	}

	MD5Data(file->contents, file->len, md5);
	snprintf(tmp, 37, "MD5:%s", md5);
	if (pkg_freebsd_contents_add_line(contents, PKG_LINE_COMMENT, tmp)
	    != 0) {
		return -1;
	}

	return 0;
}

int
pkg_freebsd_contents_free(struct pkg_freebsd_contents *contents)
{
	if (!contents) {
		return -1;
	}

	if (contents->file)
		free(contents->file);

	if (contents->lines)
		free(contents->lines);

	free(contents);

	return 0;
}
