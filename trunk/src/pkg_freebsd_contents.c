/*
 * Copyright (C) 2005, 2007 Andrew Turner
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
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
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

/**
 * @defgroup FreebsdContents FreeBSD +CONTENTS handling
 *
 * @{
 */

/**
 * @brief All possible line types in a +CONTENTS file
 */
static const char *pkg_freebsd_contents_line_str[] = {
	"",
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
	"@display",
	NULL
};

/**
 * Reads a FreeBSD +CONTENTS file create a struct pkg_freebsd_contents
 */
struct pkg_freebsd_contents *
pkg_freebsd_contents_new(const char *contents, uint64_t length)
{
	struct pkg_freebsd_contents *cont;
	unsigned int pos;

	cont = malloc(sizeof(struct pkg_freebsd_contents));
	if (!cont)
		return NULL;

	cont->cnts_file = NULL;
	cont->cnts_prefix = NULL;

	if (contents == NULL) {
		cont->file = NULL;
		cont->line_count = 0;
		cont->line_size = 0;
		cont->lines = NULL;
	} else {
		cont->file = malloc(length + 1);
		if (!cont->file) {
			free(cont);
			return NULL;
		}
		memcpy(cont->file, contents, length);
		cont->file[length] = '\0';
		cont->lines = NULL;

		pos = 0;
		cont->line_count = 0;
		while (pos != length) {
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
		cont->lines[0].data = NULL;
		pos = 1;
		while (pos < cont->line_count) {
			cont->lines[pos].data = NULL;
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
		for(pos = 0; pos < cont->line_count; pos++) {
			char *space;

			if (cont->lines[pos].line[0] != '@') {
				cont->lines[pos].line_type = PKG_LINE_FILE;
				assert(cont->lines[pos].data == NULL);
				continue;
			} else if (!strcmp(cont->lines[pos].line, "@ignore")) {
				cont->lines[pos].line_type = PKG_LINE_IGNORE;
				assert(cont->lines[pos].data == NULL);
				continue;
			}

			space = strchr(cont->lines[pos].line, ' ');
			if (space && space[0] != '\0') {
				space[0] = '\0';
				space++;
				if (space[0] != '\0')
					cont->lines[pos].data = space;
			} else {
				/* There must be a space in the line */
				pkg_freebsd_contents_free(cont);
				return NULL;
			}

			/* Get the correct line type for the line */
			if (!strcmp(cont->lines[pos].line, "@comment")) {
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
			} else if (!strcmp(cont->lines[pos].line, "@dirrm")) {
				cont->lines[pos].line_type = PKG_LINE_DIRRM;
			} else if (!strcmp(cont->lines[pos].line, "@mtree")) {
				cont->lines[pos].line_type = PKG_LINE_MTREE;
			} else if (!strcmp(cont->lines[pos].line, "@display")) {
				cont->lines[pos].line_type = PKG_LINE_DISPLAY;
			} else {
				cont->lines[pos].line_type = PKG_LINE_UNKNOWN;
				fprintf(stderr, "Unknown line type %s\n",
				    cont->lines[pos].line);
			}
		}
	}
	return cont;
}

/**
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
		assert(contents->lines[contents->line_count].data == NULL);
	} else {
		contents->lines[contents->line_count].line =
		    strdup(pkg_freebsd_contents_line_str[type]);
		contents->lines[contents->line_count].data = strdup(data);
		assert(contents->lines[contents->line_count].data != NULL);
	}
	contents->line_count++;

	if (contents->cnts_file != NULL) {
		pkgfile_free(contents->cnts_file);
		contents->cnts_file = NULL;
	}
	return 0;
}

/**
 * Adds a dependency to a +CONTENTS file
 */
int
pkg_freebsd_contents_add_dependency(struct pkg_freebsd_contents *contents,
		struct pkg *pkg)
{
	const char *origin;

	if (contents == NULL || contents->file != NULL || pkg == NULL)
		return -1;

	if (pkg_freebsd_contents_add_line(contents, PKG_LINE_PKGDEP,
	    pkg_get_name(pkg)) != 0) {
		return -1;
	}

	origin = pkg_get_origin(pkg);
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

	if (contents->cnts_file != NULL) {
		pkgfile_free(contents->cnts_file);
		contents->cnts_file = NULL;
	}
	return -1;
}

/**
 * Add's a file the the +CONTENTS file
 */
int
pkg_freebsd_contents_add_file(struct pkg_freebsd_contents *contents,
		struct pkgfile *file)
{
	char md5[33], tmp[37];
	const char *data;

	if (contents == NULL || contents->file != NULL || file == NULL)
		return -1;

	if (pkg_freebsd_contents_add_line(contents, PKG_LINE_FILE,
	    pkgfile_get_name(file)) != 0) {
		return -1;
	}

	data = pkgfile_get_data(file);
	if (!data)
		return -1;
	MD5Data(data, pkgfile_get_size(file), md5);
	snprintf(tmp, 37, "MD5:%s", md5);
	if (pkg_freebsd_contents_add_line(contents, PKG_LINE_COMMENT, tmp)
	    != 0) {
		return -1;
	}

	if (contents->cnts_file != NULL) {
		pkgfile_free(contents->cnts_file);
		contents->cnts_file = NULL;
	}
	return 0;
}

/**
 * Gets the given line from the contents file
 */
struct pkg_freebsd_contents_line*
pkg_freebsd_contents_get_line(struct pkg_freebsd_contents *contents,
		unsigned int line)
{
	if (contents == NULL)
		return NULL;

	if (line > contents->line_count)
		return NULL;

	return &contents->lines[line];
}

int
pkg_freebsd_contents_update_prefix(struct pkg_freebsd_contents *contents,
    const char *prefix)
{
	unsigned int pos;

	if (contents == NULL)
		return -1;

	/* Find the package prefix and change it */
	for (pos = 0; pos < contents->line_count; pos++) {
		if (contents->lines[pos].line_type == PKG_LINE_CWD) {
			if (contents->cnts_prefix != NULL) {
				free(contents->cnts_prefix);
			}
			contents->cnts_prefix = strdup(prefix);
			contents->lines[pos].data = contents->cnts_prefix;
			break;
		}
	}
	if (contents->cnts_file != NULL) {
		pkgfile_free(contents->cnts_file);
		contents->cnts_file = NULL;
	}
	return 0;
}

struct pkgfile *
pkg_freebsd_contents_get_file(struct pkg_freebsd_contents *contents)
{
	unsigned int pos;

	if (contents == NULL)
		return NULL;

	if (contents->cnts_file == NULL) {
		contents->cnts_file = pkgfile_new_regular("+CONTENTS", "", 0);
		if (contents->cnts_file == NULL)
			return NULL;

		for (pos = 0; pos < contents->line_count; pos++) {
			struct pkg_freebsd_contents_line *line;
			char *data;

			line = &contents->lines[pos];
			if (line->data == NULL) {
				pkgfile_append(contents->cnts_file, line->line,
				    strlen(line->line));
				pkgfile_append(contents->cnts_file, "\n", 1);
			} else {
				asprintf(&data, "%s %s\n",
				    line->line,line->data);
				pkgfile_append(contents->cnts_file, data,
				    strlen(data));
				free(data);
			}
		}
	}
	return contents->cnts_file;
}

/**
 * Frees a contents struct
 */
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
/**
 * @}
 */
