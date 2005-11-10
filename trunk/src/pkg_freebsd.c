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

#include <archive.h>
#include <archive_entry.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "pkg.h"
#include "pkg_freebsd.h"
#include "pkg_private.h"
#include "pkg_freebsd_private.h"

struct freebsd_package {
	struct archive	*archive;
	FILE		*fd;

	struct pkg_list *control;

	/* If not null contains the next file in the archive */
	struct pkg_file	*next;
};

/* Callbacks */
static struct pkg_list	*freebsd_get_control_files(struct pkg *);
static struct pkg_file		*freebsd_get_next_file(struct pkg *);
static int			 freebsd_free(struct pkg *);

/* Internal functions */
static struct pkg_file		*freebsd_get_next_entry(struct archive *);
static char 			*freebsd_get_pkg_name(const char *);
static int			 freebsd_free_package(struct freebsd_package *);

struct pkg *
pkg_new_freebsd(FILE *fd)
{
	struct pkg * pkg;
	struct freebsd_package *f_pkg;
	char *pkg_name;
	struct pkg_file *file;

	f_pkg = malloc(sizeof(struct freebsd_package));
	if (!f_pkg) {
		pkg_error_set(&pkg_null, "Out of Memory");
		return NULL;
	}

	f_pkg->next = NULL;
	f_pkg->control = NULL;
	f_pkg->fd = fd;

	f_pkg->archive = archive_read_new();
	archive_read_support_compression_bzip2(f_pkg->archive);
	archive_read_support_compression_gzip(f_pkg->archive);
	archive_read_support_format_tar(f_pkg->archive);

	if (archive_read_open_stream(f_pkg->archive, fd, 10240)
	    != ARCHIVE_OK) {
		freebsd_free_package(f_pkg);
		pkg_error_set(&pkg_null, "Could not open archive");
		return NULL;
	}

	/* Read the first file and check it has the correct name */
	file = freebsd_get_next_entry(f_pkg->archive);
	if (!file) {
		freebsd_free_package(f_pkg);
		/* Error message is in pkg_null */
		return NULL;
	} else if (strcmp(file->filename, "+CONTENTS")) {
		/* Package error */
		pkg_file_free(file);
		freebsd_free_package(f_pkg);
		pkg_error_set(&pkg_null, "Package does not start with a +CONTENTS file");
		return NULL;
	}
	f_pkg->control = pkg_file_list_add(f_pkg->control, file);

	/* Find the package name */
	pkg_name = freebsd_get_pkg_name(file->contents);

	/* Add all the control files to the control pkg_files_list */
	while (1) {
		file = freebsd_get_next_entry(f_pkg->archive);
		if (file == NULL) {
			break;
		} else if (file->filename[0] != '+') {
			f_pkg->next = file;
			break;
		} else {
			f_pkg->control = pkg_file_list_add(f_pkg->control, file);
		}
	}

	pkg = pkg_new(pkg_name, freebsd_get_control_files,
		freebsd_get_next_file, freebsd_free);

	free(pkg_name);

	if (pkg)
		pkg->pkg_object.data = f_pkg;

	return pkg;
}

static struct pkg_list *
freebsd_get_control_files(struct pkg *pkg)
{
	struct freebsd_package *f_pkg;

	assert(pkg != NULL);

	f_pkg = pkg->pkg_object.data;

	return f_pkg->control;
}

static struct pkg_file	*
freebsd_get_next_file(struct pkg *pkg)
{
	struct freebsd_package *f_pkg;

	assert(pkg != NULL);

	f_pkg = pkg->pkg_object.data;

	if (f_pkg->next) {
		struct pkg_file *ret;

		ret = f_pkg->next;
		f_pkg->next = NULL;

		return ret;
	}

	return freebsd_get_next_entry(f_pkg->archive);
}

static int
freebsd_free(struct pkg *pkg)
{
	assert(pkg != NULL);

	freebsd_free_package(pkg->pkg_object.data);

	return PKG_OK;
}

static struct pkg_file *
freebsd_get_next_entry(struct archive *a)
{
	uint64_t length;
	char *str;
	struct archive_entry *entry;
	const struct stat *sb;

	assert(a != NULL);

	/* Read the next entry to a buffer. */
	if (archive_read_next_header(a, &entry) != ARCHIVE_OK) {
		pkg_error_set(&pkg_null, "Could not read next header");
		return NULL;
	}

	length = archive_entry_size(entry);
	str = malloc(length+1);
	if (!str) {
		pkg_error_set(&pkg_null, "Out of Memory");
		return NULL;
	}
	archive_read_data_into_buffer(a, str, length);
	str[length] = '\0';

	sb = archive_entry_stat(entry);
	return pkg_file_new_from_buffer(archive_entry_pathname(entry),
		length, str, sb);
}

/* Returns the package name from a +CONTENTS file */
static char *
freebsd_get_pkg_name(const char *buffer)
{
	unsigned int len;
	char *pkg_name, *str, *ptr;

	assert(buffer != NULL);

	/* Find the character after the first space on the second line */
	str = strchr(buffer, '\n');
	if (!str) {
		pkg_error_set(&pkg_null, "Malformed +CONTROL file");
		return NULL;
	}
	str = strchr((const char *)str, ' ');
	if (!str) {
		pkg_error_set(&pkg_null, "Malformed +CONTROL file");
		return NULL;
	}
	str++;
	if (str[0] == '\0') {
		pkg_error_set(&pkg_null, "Malformed +CONTROL file");
		return NULL;
	}

	/* Copy the rest of the line to pkg_name */
	ptr = strchr((const char *)str, '\n');
	if (!ptr) {
		pkg_error_set(&pkg_null, "Malformed +CONTROL file");
		return NULL;
	}

	len = ptr-str;
	pkg_name = malloc(len+1);
	if (!pkg_name) {
		pkg_error_set(&pkg_null, "Out of Memory");
		return NULL;
	}

	strlcpy(pkg_name, str, len+1);

	return pkg_name;
}

/* Free the struct freebsd_package */
static int
freebsd_free_package(struct freebsd_package *f_pkg)
{
	assert(f_pkg != NULL);

	if (f_pkg->archive) {
		archive_read_finish(f_pkg->archive);
		f_pkg->archive = NULL;
	}

	if (f_pkg->fd)
		fclose(f_pkg->fd);

	f_pkg->fd = NULL;

	pkg_list_free(f_pkg->control);
	f_pkg->control = NULL;

	free(f_pkg);

	return PKG_OK;
}

/*
 * Using fmt, replace all instances of:
 *
 * %F   With the parameter "name"
 * %D   With the parameter "dir"
 * %B   Return the directory part ("base") of %D/%F
 * %f   Return the filename part of %D/%F
 *
 * Does not check for overflow - caution!
 *
 */
void
freebsd_format_cmd(char *buf, int max, const char *fmt, const char *dir,
	const char *name)
{
	char *cp, scratch[FILENAME_MAX * 2];
	int l;

	assert(buf != NULL);
	assert(max >= 0);
	assert(fmt != NULL);
	assert(dir != NULL);
	assert(name != NULL);

	while (*fmt && max > 0) {
		if (*fmt == '%') {
			switch (*++fmt) {
				case 'F':
					strncpy(buf, name, max);
					l = strlen(name);
					buf += l, max -= l;
					break;

				case 'D':
					strncpy(buf, dir, max);
					l = strlen(dir);
					buf += l, max -= l;
					break;

				case 'B':
					snprintf(scratch, FILENAME_MAX * 2,
					    "%s/%s", dir, name);
					cp = &scratch[strlen(scratch) - 1];
					while (cp != scratch && *cp != '/')
						--cp;
					*cp = '\0';
					strncpy(buf, scratch, max);
					l = strlen(scratch);
					buf += l, max -= l;
					break;

				case 'f':
					snprintf(scratch, FILENAME_MAX * 2,
					    "%s/%s", dir, name);
					cp = &scratch[strlen(scratch) - 1];
					while (cp != scratch && *(cp - 1) != '/')
						--cp;
					strncpy(buf, cp, max);
					l = strlen(cp);
					buf += l, max -= l;
					break;

				default:
					*buf++ = *fmt;
					--max;
					break;
			}
			++fmt;
		} else {
			*buf++ = *fmt++;
			--max;
		}
	}
	*buf = '\0';
}
