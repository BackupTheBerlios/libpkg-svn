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

#include <sys/stat.h>
#include <sys/types.h>

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pkg.h"
#include "pkg_freebsd.h"
#include "pkg_private.h"
#include "pkg_freebsd_private.h"

#define DB_LOCATION	"/var/db/pkg"

/*
 * State transition array for the head part of a +CONTENTS file.
 * p0 is the start state, p4 and p6 are the accepting states
 */
static int pkg_states[7][12] = {
	{ -1,  1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 }, /* p0 */
	{ -1, -1,  2, -1, -1, -1, -1, -1, -1, -1, -1, -1 }, /* p1 */
	{ -1,  3, -1,  4, -1, -1, -1, -1, -1, -1, -1, -1 }, /* p2 */
	{ -1, -1, -1,  4, -1, -1, -1, -1, -1, -1, -1, -1 }, /* p3 */
	{ -1, -1, -1, -1,  5,  6, -1, -1, -1, -1, -1, -1 }, /* p4 */
	{ -1,  4, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 }, /* p5 */
	{ -1, -1, -1, -1, -1,  6, -1, -1, -1, -1, -1, -1 }  /* p6 */
};

static int freebsd_install_pkg(struct pkg_db *, struct pkg *);
static int freebsd_is_installed(struct pkg_db *, const char *);
static struct pkg_file *freebsd_build_contents(struct pkg_freebsd_contents *);
static int freebsd_do_cwd(struct pkg_db *, struct pkg *, char *ndir);
static int freebsd_check_contents(struct pkg_db *, struct pkg_freebsd_contents *);

/*
 * Opens the FreeBSD Package Database
 */
struct pkg_db*
pkg_db_open_freebsd(const char *base)
{
	struct pkg_db *db;

	db = pkg_db_open(base, freebsd_install_pkg, freebsd_is_installed);
	if (!db) {
		/* pkg_null will have the error message */
		return NULL;
	}

	return db;
}

/*
 * Installs the package pkg to the database db
 */
static int
freebsd_install_pkg(struct pkg_db *db, struct pkg *pkg)
{
	struct pkg_file	*contents_file;
	struct pkg_list *control;
	struct pkg_freebsd_contents *contents;
	char *cwd;
	char *directory, *last_file;
	int i = 0; //, state = 0;

	assert(pkg != NULL);

	control = pkg_get_control_files(pkg);
	if (!control) {
		char *str;
		str = pkg_error_string(&pkg_null);
		pkg_error_set((struct pkg_object *)db, str);
		return PKG_FAIL;
	}

	/* Find the contents file in the control files */
	contents_file = pkg_file_list_get_file(control, "+CONTENTS");
	if (!contents_file) {
		char *str;
		pkg_list_free(control);
		str = pkg_error_string(&pkg_null);
		pkg_error_set((struct pkg_object *)db, str);
		return PKG_FAIL;
	}

	contents = pkg_freebsd_contents_new(contents_file->contents);
	if (!contents) {
		char *str;
		pkg_list_free(control);
		str = pkg_error_string(&pkg_null);
		pkg_error_set((struct pkg_object *)db, str);
		return PKG_FAIL;
	}

	cwd = getcwd(NULL, 0);
	if (!cwd) {
		pkg_list_free(control);
		pkg_freebsd_contents_free(contents);
		pkg_error_set((struct pkg_object *)db, "Get the current working directory");
		return PKG_FAIL;
	}

	i = freebsd_check_contents(db, contents);
	if (i == -1) {
		pkg_list_free(control);
		pkg_freebsd_contents_free(contents);
		chdir(cwd);
		free(cwd);
		return PKG_FAIL;
	}

	directory = getcwd(NULL, 0);
	last_file = NULL;

	/* Read through the contents file and install the package */
	for (; i < contents->line_count; i++) {
		switch (contents->lines[i].line_type) {
		case PKG_LINE_COMMENT:
		case PKG_LINE_UNEXEC:
		case PKG_LINE_DIRRM:
		case PKG_LINE_MTREE:
		case PKG_LINE_IGNORE:
			break;
		case PKG_LINE_CWD:
			/* Change to the correct directory */
			if (freebsd_do_cwd(db, pkg, contents->lines[i].data)
			    == PKG_FAIL) {
				chdir(cwd);
				free(cwd);
				pkg_list_free(control);
				pkg_freebsd_contents_free(contents);
				return PKG_FAIL;
			}

			free(directory);
			directory = getcwd(NULL, 0);
			break;
		case PKG_LINE_EXEC: {
			char cmd[FILENAME_MAX];
			freebsd_format_cmd(cmd, FILENAME_MAX,
			    contents->lines[i].data, directory, last_file);
			printf("exec %s\n", cmd);
			break;
		}
		case PKG_LINE_FILE: {
			/* Install a file to the correct directory */

			struct pkg_file *file;
			char *contents_sum;
			int ret;

			/* Check the contents file is correctly formated */
			if (contents->lines[i+1].line_type != PKG_LINE_COMMENT) {
				chdir(cwd);
				free(cwd);
				pkg_error_set((struct pkg_object *)db,
				    "Incorrect line in +CONTENTS file");
				pkg_freebsd_contents_free(contents);
				return PKG_FAIL;
			} else if (strncmp("MD5:", contents->lines[i+1].data, 4)) {
				chdir(cwd);
				free(cwd);
				pkg_error_set((struct pkg_object *)db,
				    "Incorrect line in +CONTENTS file: "
				    "Checksum is incorrect");
				pkg_freebsd_contents_free(contents);
				return PKG_FAIL;
			}

			/* Read the file to install */
			if (contents->lines[i].line[0] == '+') {
				/* + Files are not fetched with pkg_get_next_file */
				file = pkg_file_list_get_file(control, contents->lines[i].line);
			} else {
				file = pkg_get_next_file(pkg);
			}

			/* Check the file name is correct */
			if (strcmp(file->filename, contents->lines[i].line)) {
				chdir(cwd);
				free(cwd);
				pkg_file_free(file);
				pkg_freebsd_contents_free(contents);
				pkg_error_set((struct pkg_object *)file,
				    "File name incorrect");
				return PKG_FAIL;
			}

			contents_sum = strchr(contents->lines[i+1].data, ':');
			contents_sum++;
			if (pkg_checksum_md5(file, contents_sum) == PKG_FAIL) {
				char *str;
				pkg_list_free(control);

				str = pkg_error_string(
				    (struct pkg_object *)file);
				pkg_error_set((struct pkg_object *)db, str);
				return PKG_FAIL;
			}

			/* Install the file */
			ret = pkg_file_write(file);
			if (ret == PKG_FAIL) {
				char *str;

				str = pkg_error_string(
				    (struct pkg_object *)file);
				pkg_error_set((struct pkg_object *)db, str);
				pkg_list_free(control);
				pkg_file_free(file);
				pkg_freebsd_contents_free(contents);
				return PKG_FAIL;
			}

			/* Remember the name if there is an "@exec" line next */
			if (last_file)
				free(last_file);
			last_file = strdup(file->filename);

			if (contents->lines[i].line[0] != '+')
				pkg_file_free(file);

			i++;
			break;
		}
		default:
			fprintf(stderr, "ERROR: Incorrect line in "
			    "+CONTENTS file \"%s %s\"\n",
			    contents->lines[i].line,
			    contents->lines[i].data);
			break;
		}
	}

	/* Create the new contents file */
	contents_file = freebsd_build_contents(contents);
	pkg_file_write(contents_file);
	pkg_file_free(contents_file);

	free(directory);
	if (last_file)
		free(last_file);
	chdir(cwd);
	free(cwd);

	pkg_freebsd_contents_free(contents);

	return PKG_NOTSUP;
}

/*
 * Returns PKG_YES if the package is installed
 *         PKG_NO on not installed
 *         PKG_FAIL on error
 */
static int
freebsd_is_installed(struct pkg_db *db, const char *package)
{
	struct stat sb;
	char *dir;

	assert(db != NULL);
	assert(package != NULL);

	asprintf(&dir, "%s" DB_LOCATION "/%s", db->db_base, package);
	if (!dir) {
		pkg_error_set((struct pkg_object *)db, "Out of Memory");
		return PKG_FAIL;
	}

	errno = 0;
	if (stat(dir, &sb)) {
		if (errno == ENOENT || errno == ENOTDIR) {
			free(dir);
			return PKG_NO;
		} else {
			pkg_error_set((struct pkg_object *)db,
			    "Error with directory %s", dir);
			free(dir);
			return PKG_FAIL;
		}
	}

	if (!S_ISDIR(sb.st_mode)) {
		pkg_error_set((struct pkg_object *)db,
		    "The file %s exists but it is not a directory", dir);
		free(dir);
		return PKG_FAIL;
	}

	free(dir);

	/* XXX Check the correct files are there */

	return PKG_NO;
}

static int
freebsd_do_cwd(struct pkg_db *db, struct pkg *pkg, char *ndir) {
	char *dir;

	assert(db != NULL);
	assert(ndir != NULL);

	/*
	 * If the dir is . it should really
	 * be the package database dir
	 */
	if (ndir[0] == '.' &&
	    ndir[1] == '\0') {
		assert(pkg != NULL); /* pkg is only needed to chdir to . */
		asprintf(&dir, "%s/var/db/pkg/%s", db->db_base, pkg->pkg_name);
		if (!dir) {
			pkg_error_set((struct pkg_object *)db,
			    "Out of Memory");
			return PKG_FAIL;
		}
		pkg_dir_build(dir);
	} else {
		/* Set dir to the correct location */
		asprintf(&dir, "%s/%s", db->db_base, ndir);
		if (!dir) {
			pkg_error_set((struct pkg_object *)db,
			    "Out of Memory");
			return PKG_FAIL;
		}
	}
	if (chdir(dir) == -1) {
		pkg_error_set((struct pkg_object *)db,
		    "Could not chdir to %s", dir);
		free(dir);
		return PKG_FAIL;
	}

	free(dir);

	return PKG_OK;
}

/*
 * Builds the new cotents file to be installed in /var/db/pkg/foo-1.2.3
 */
static struct pkg_file *
freebsd_build_contents(struct pkg_freebsd_contents *contents)
{
	uint64_t size, used;
	char *buffer, *ptr;
	int i;

	assert(contents != NULL);

	used = 0;
	size = 1024;
	buffer = malloc(size);
	ptr = buffer;
	if (!buffer) {
		return NULL;
	}
	for (i = 0; i < contents->line_count; i++) {
		int line_len, data_len;

		line_len = strlen(contents->lines[i].line);
		data_len = 0;
		if (contents->lines[i].line_type != PKG_LINE_FILE &&
		    contents->lines[i].line_type != PKG_LINE_IGNORE) {
			data_len = strlen(contents->lines[i].data);
		}
		/* if the line is @ignore we will ignore the 2 lines */
		switch (contents->lines[i].line_type) {
		case PKG_LINE_IGNORE:
			i += 2;
			break;
		case PKG_LINE_CWD:
			if (strcmp(contents->lines[i].data, ".")) {
				if (used + line_len + data_len + 2 >= size) {
					size += 1024;
					buffer = realloc(buffer, size);
					ptr = buffer + used;
				}
				sprintf(ptr, "%s %s\n",
				    contents->lines[i].line,
				    contents->lines[i].data);
				used += line_len + data_len + 2;
				ptr = buffer + used;
			}
		case PKG_LINE_MTREE:
			break;
		case PKG_LINE_FILE:
			if (used + line_len + 1 >= size) {
				size += 1024;
				buffer = realloc(buffer, size);
				ptr = buffer + used;
			}
			sprintf(ptr, "%s\n", contents->lines[i].line);
			used += line_len + 1;
			ptr = buffer + used;
			break;
		default:
			if (used + line_len + data_len + 2 >= size) {
				size += 1024;
				buffer = realloc(buffer, size);
				ptr = buffer + used;
			}
			sprintf(ptr, "%s %s\n", contents->lines[i].line,
			    contents->lines[i].data);
			used += line_len + data_len + 2;
			ptr = buffer + used;
			break;
		}
	}
	/*
	 * buffer now contains the data to write
	 * to /var/db/pkg/foo-1.2.3/+CONTENTS
	 */
	return pkg_file_new_from_buffer("+CONTENTS", used, buffer, NULL);
}

/* Unlike most functions this return -1 on error.
 * It will return the number of lines to
 * skip to get to the first file.
 */
static int
freebsd_check_contents(struct pkg_db *db, struct pkg_freebsd_contents *contents)
{
	int i, state;

	assert(db != NULL);
	assert(contents != NULL);

	state = 0;

	if (contents->lines[0].line_type != PKG_LINE_COMMENT) {
		pkg_error_set((struct pkg_object *)db, "Contents file is malformed");
		return -1;
	} else if (strcmp(contents->lines[0].data, "PKG_FORMAT_REVISION:1.1")) {
		pkg_error_set((struct pkg_object *)db, "Contents file is malformed");
		return -1;
	}

	/* Run through a NFA to check the head */
	for (i = 0; i < contents->line_count; i++) {
		int new_state = -2;
		new_state = pkg_states[state][contents->lines[i].line_type];
		if (new_state == -1) {
			break;
		}
		/* If the current line is @chdir... do it */
		if (contents->lines[i].line_type == PKG_LINE_CWD) {
			if (freebsd_do_cwd(db, NULL, contents->lines[i].data)
			    == PKG_FAIL) {
				return PKG_FAIL;
			}
		}
		state = new_state;
	}
	if (state != 4 && state != 6) {
		pkg_error_set((struct pkg_object *)db, "Contents file is malformed");
		return -1;
	}
	return i;
}
