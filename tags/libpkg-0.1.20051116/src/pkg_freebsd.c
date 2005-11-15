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

	struct pkg_file **control;
	struct pkg_freebsd_contents	*contents;

	/* If not null contains the next file in
	 * the archive after the control files */
	struct pkg_file	*next;
};

/* Callbacks */
static struct pkg_file		**freebsd_get_control_files(struct pkg *);
static struct pkg_file		*freebsd_get_next_file(struct pkg *);
static struct pkg		**freebsd_get_deps(struct pkg *);
static int			 freebsd_free(struct pkg *);

/* Internal functions */
static struct freebsd_package	*freebsd_get_package(FILE *);
static struct pkg_file		*freebsd_get_next_entry(struct archive *);
static char 			*freebsd_get_pkg_name(const char *);
static int			 freebsd_free_package(struct freebsd_package *);

struct pkg *
pkg_new_freebsd(FILE *fd)
{
	struct pkg *pkg;
	struct freebsd_package *f_pkg;
	char *pkg_name;

	f_pkg = freebsd_get_package(fd);

	/* Find the package name */
	pkg_name = freebsd_get_pkg_name(f_pkg->control[0]->contents);

	pkg = pkg_new(pkg_name, freebsd_get_control_files,
		freebsd_get_next_file, freebsd_get_deps, freebsd_free);
	free(pkg_name);

	if (pkg == NULL)
		return NULL;

	pkg->data = f_pkg;

	return pkg;
}

/* XXX Make this a callback */
struct pkg *
pkg_make_freebsd(struct pkg *pkg, FILE *fd)
{
	struct freebsd_package *f_pkg;

	pkg_set_callbacks(pkg, freebsd_get_control_files,
	    freebsd_get_next_file, freebsd_get_deps, freebsd_free);
	f_pkg = freebsd_get_package(fd);
	pkg->data = f_pkg;
	return pkg;
}

/*
 * Returns a pointer to be placed into the data of the Package object
 */
static struct freebsd_package *
freebsd_get_package(FILE *fd)
{
	struct freebsd_package *f_pkg;
	struct pkg_file *file;
	size_t control_size;
	unsigned int control_pos;

	f_pkg = malloc(sizeof(struct freebsd_package));
	if (!f_pkg) {
		return NULL;
	}

	/* Init the struct */
	f_pkg->next = NULL;
	f_pkg->control = NULL;
	f_pkg->contents = NULL;
	f_pkg->fd = fd;

	/* We only need to read from gzip and bzip2 as they
	 * are the only posible file types for FreeBSD packages
	 */
	f_pkg->archive = archive_read_new();
	archive_read_support_compression_bzip2(f_pkg->archive);
	archive_read_support_compression_gzip(f_pkg->archive);
	archive_read_support_format_tar(f_pkg->archive);

	if (archive_read_open_stream(f_pkg->archive, fd, 10240)
	    != ARCHIVE_OK) {
		freebsd_free_package(f_pkg);
		return NULL;
	}

	/* Read the first file and check it has the correct name */
	file = freebsd_get_next_entry(f_pkg->archive);
	if (!file) {
		freebsd_free_package(f_pkg);
		return NULL;
	} else if (strcmp(file->filename, "+CONTENTS")) {
		/* Package error */
		pkg_file_free(file);
		freebsd_free_package(f_pkg);
		return NULL;
	}
	/* Set the control files array to be big enough for
	 * the +CONTENTS file and a null terminator
	 */
	f_pkg->contents = pkg_freebsd_contents_new(file->contents);
	control_size = sizeof(struct pkg_file *) * 2;
	f_pkg->control = malloc(control_size);
	f_pkg->control[0] = file;
	f_pkg->control[1] = NULL;
	control_pos = 1;

	/* Add all the control files to the control array */
	while (1) {
		file = freebsd_get_next_entry(f_pkg->archive);
		if (file == NULL) {
			break;
		} else if (file->filename[0] != '+') {
			f_pkg->next = file;
			break;
		} else {
			control_size += sizeof(struct pkg_file *);
			f_pkg->control = realloc(f_pkg->control, control_size);
			f_pkg->control[control_pos] = file;
			control_pos++;
			f_pkg->control[control_pos] = NULL;
		}
	}

	return f_pkg;
}

/* Return the array of control files */
static struct pkg_file **
freebsd_get_control_files(struct pkg *pkg)
{
	struct freebsd_package *f_pkg;

	assert(pkg != NULL);

	f_pkg = pkg->data;

	return f_pkg->control;
}

/* Get the next file in the package */
static struct pkg_file *
freebsd_get_next_file(struct pkg *pkg)
{
	struct freebsd_package *f_pkg;

	assert(pkg != NULL);

	f_pkg = pkg->data;

	if (f_pkg->next) {
		struct pkg_file *ret;

		ret = f_pkg->next;
		f_pkg->next = NULL;

		return ret;
	}

	return freebsd_get_next_entry(f_pkg->archive);
}

/*
 * Find all the packages that depend on this package
 * and return an array of empty package objects
 */
static struct pkg **
freebsd_get_deps(struct pkg *pkg)
{
	int line;
	struct pkg_freebsd_contents *contents;
	struct pkg **pkgs;
	unsigned int pkg_count;
	size_t pkg_size;

	assert(pkg != NULL);

	/* If this is null there was an error that should have been checked */
	contents = ((struct freebsd_package *)pkg->data)->contents;
	assert(contents != NULL);

	pkg_count = 0;
	pkg_size = sizeof(struct pkg *);
	pkgs = malloc(pkg_size);
	pkgs[0] = NULL;
	for (line = 0; line < contents->line_count; line++) {
		if (contents->lines[line].line_type == PKG_LINE_PKGDEP) {
			pkg_size += sizeof(struct pkg *);
			pkgs = realloc(pkgs, pkg_size);
			pkgs[pkg_count] = pkg_new(contents->lines[line].data,
			    NULL, NULL, NULL, NULL);
			pkg_count++;
			pkgs[pkg_count] = NULL;
		}
	}
	return pkgs;
}

/* Free the package */
static int
freebsd_free(struct pkg *pkg)
{
	assert(pkg != NULL);

	freebsd_free_package(pkg->data);

	return 0;
}

/* Return a pointer to the next file in the archive `a' */
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
		return NULL;
	}

	/* Allocate enough space for the file and copy it to the string */
	length = archive_entry_size(entry);
	str = malloc(length+1);
	if (!str) {
		return NULL;
	}
	archive_read_data_into_buffer(a, str, length);
	str[length] = '\0';

	/* Get the needed struct stat from the archive */
	sb = archive_entry_stat(entry);

	/* Create the pkg_file and return it */
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
		return NULL;
	}
	str = strchr((const char *)str, ' ');
	if (!str) {
		return NULL;
	}
	str++;
	if (str[0] == '\0') {
		return NULL;
	}

	/* Copy the rest of the line to pkg_name */
	ptr = strchr((const char *)str, '\n');
	if (!ptr) {
		return NULL;
	}

	len = ptr-str;
	pkg_name = malloc(len+1);
	if (!pkg_name) {
		return NULL;
	}

	strlcpy(pkg_name, str, len+1);

	return pkg_name;
}

/* Free the struct freebsd_package */
static int
freebsd_free_package(struct freebsd_package *f_pkg)
{
	unsigned int pos;
	
	assert(f_pkg != NULL);

	if (f_pkg->archive) {
		archive_read_finish(f_pkg->archive);
		f_pkg->archive = NULL;
	}

	if (f_pkg->fd)
		fclose(f_pkg->fd);

	f_pkg->fd = NULL;

	for (pos=0; f_pkg->control[pos] != NULL; pos++)
		pkg_file_free(f_pkg->control[pos]);
	free(f_pkg->control);
	f_pkg->control = NULL;

	if (f_pkg->next)
		pkg_file_free(f_pkg->next);
	pkg_freebsd_contents_free(f_pkg->contents);

	free(f_pkg);

	return 0;
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
