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

#include <sys/types.h>

#include <archive.h>
#include <archive_entry.h>
#include <assert.h>
#include <dirent.h>
#include <err.h>
#include <libgen.h>
#include <stdlib.h>
#include <string.h>

#include "pkg.h"
#include "pkg_freebsd.h"
#include "pkg_private.h"
#include "pkg_freebsd_private.h"

struct freebsd_package {
	struct archive	 *archive;
	FILE		 *fd;

	struct pkg_file **control;
	struct pkg_freebsd_contents *contents;
	char		 *origin;

	/* If not null contains the next file in
	 * the archive after the control files */
	struct pkg_file	 *next;

	struct pkg_file **all_files;
	unsigned int	  all_files_size;
	unsigned int	  all_files_pos;
};

/* Callbacks */
pkg_static int			  freebsd_add_depend(struct pkg *,
					struct pkg *);
pkg_static int			  freebsd_add_file(struct pkg *,
					struct pkg_file *);
pkg_static struct pkg_file	**freebsd_get_control_files(struct pkg *);
pkg_static struct pkg_file	 *freebsd_get_control_file(struct pkg *,
					const char *);
pkg_static struct pkg_file	 *freebsd_get_next_file(struct pkg *);
pkg_static struct pkg		**freebsd_get_deps(struct pkg *);
pkg_static int			  freebsd_free(struct pkg *);

pkg_static const char		 *freebsd_get_version(struct pkg *);
pkg_static const char		 *freebsd_get_origin(struct pkg *);

/* Internal functions */
pkg_static struct pkg_file			**freebsd_open_control_files(const char *);
pkg_static struct freebsd_package	*freebsd_get_package(FILE *,
					struct pkg_file **);
pkg_static struct pkg_file		*freebsd_get_next_entry(struct archive *);
pkg_static char 			*freebsd_get_pkg_name(const char *);
pkg_static int			 freebsd_free_package(struct freebsd_package *);

#define FREE_CONTENTS(c) \
	{ \
		int i; \
		for (i=0; c[i] != NULL; i++) { \
			pkg_file_free(c[i]); \
		} \
		free(c); \
	}

/**
 * @defgroup FreebsdPackage FreeBSD Package
 * @ingroup pkg
 *
 * @{
 */

/**
 * @brief Creates a new FreeBSD package from a FILE pointer
 * @param fd A pointer to a FILE object containing a FreeBSD Package
 *
 * This creates a pkg object from a given file pointer.
 * It is able to then manipulate the package and install the it to the pkg_db.
 * @return A new package object or NULL
 */
struct pkg *
pkg_new_freebsd_from_file(FILE *fd)
{
	struct pkg *pkg;
	struct freebsd_package *f_pkg;
	char *pkg_name;
	unsigned int line;

	if (fd == NULL)
		return NULL;
	
	f_pkg = freebsd_get_package(fd, NULL);

	/* Find the package name */
	pkg_name = freebsd_get_pkg_name(pkg_file_get(f_pkg->control[0]));

	pkg = pkg_new(pkg_name, freebsd_get_control_files,
	    freebsd_get_control_file, freebsd_get_deps, freebsd_free);
	free(pkg_name);

	if (pkg == NULL)
		return NULL;
	pkg_add_callbacks_install(pkg, freebsd_get_next_file);
	pkg_add_callbacks_data(pkg, freebsd_get_version, freebsd_get_origin);

	pkg->data = f_pkg;
	
	assert(f_pkg->contents != NULL);
	for (line = 0; line < f_pkg->contents->line_count; line++) {
		if (f_pkg->contents->lines[line].line_type == PKG_LINE_COMMENT)
		    {
			if (strncmp("ORIGIN:",
			    f_pkg->contents->lines[line].data, 7) == 0) {
				f_pkg->origin = strdup(
				    f_pkg->contents->lines[line].data + 7);
				break;
			}
		}
	}

	return pkg;
}

/**
 * @brief Creates a new FreeBSD package from one installed on a system
 * @param pkg_name The name of the package to retrieve
 * @param pkg_db_dir The directory in the database the package is registered in
 * @todo Make this work through a pkg_db callback
 * @todo Remove the need for pkg_db_dir by using a struct pkg_repo
 *
 * This creates a package object from an installed package.
 * It can be used to retrieve information from the pkg_db and deintall
 * the package.
 * @return A pkg object or NULL
 */
struct pkg *
pkg_new_freebsd_installed(const char *pkg_name, const char *pkg_db_dir)
{
	struct pkg *pkg;
	struct freebsd_package *f_pkg;
	struct pkg_file **control;
	unsigned int line;


	if (!pkg_name || ! pkg_db_dir)
		return NULL;

	/*
	 * This section until the closedir takes too long in pkg_info.
	 * It needs to be optimised to just read the required data
	 */
	control = freebsd_open_control_files(pkg_db_dir);

	/* Only the get_deps and free callbacks will work */
	pkg = pkg_new(pkg_name, freebsd_get_control_files,
	    freebsd_get_control_file, freebsd_get_deps, freebsd_free);
	if (pkg == NULL) {
		FREE_CONTENTS(control);
		return NULL;
	}
	pkg_add_callbacks_data(pkg, freebsd_get_version, freebsd_get_origin);

	f_pkg = freebsd_get_package(NULL, control);
	if (f_pkg == NULL) {
		pkg_free(pkg);
		return NULL;
	}
	pkg->data = f_pkg;

	assert(f_pkg->contents != NULL);
	for (line = 0; line < f_pkg->contents->line_count; line++) {
		if (f_pkg->contents->lines[line].line_type == PKG_LINE_COMMENT)
		    {
			if (strncmp("ORIGIN:",
			    f_pkg->contents->lines[line].data, 7) == 0) {
				f_pkg->origin = strdup(
				    f_pkg->contents->lines[line].data + 7);
				break;
			}
		}
	}

	return pkg;
}

/**
 * @brief Creates an empty FreeBSD package to add files to
 * @param pkg_name The name of the package
 *
 * This creates an empty FreeBSD Package.
 * It can then have files added to it, eg. in pkg_create(1)
 * @return A package object or NULL
 */
struct pkg *
pkg_new_freebsd_empty(const char *pkg_name)
{
	struct pkg *pkg;
	struct freebsd_package *f_pkg;

	if (pkg_name == NULL)
		return NULL;

	pkg = pkg_new(pkg_name, NULL, NULL, NULL, freebsd_free);
	if (pkg == NULL)
		return NULL;
	pkg_add_callbacks_empty(pkg, freebsd_add_depend, freebsd_add_file);

	f_pkg = freebsd_get_package(NULL, NULL);
	pkg->data = f_pkg;
	if (f_pkg == NULL) {
		pkg_free(pkg);
		return NULL;
	}
	f_pkg->all_files_size = sizeof(struct pkg_file *);
	f_pkg->all_files_pos = 0;
	f_pkg->all_files = malloc(f_pkg->all_files_size);
	if (f_pkg->all_files == NULL) {
		pkg_free(pkg);
		return NULL;
	}
	f_pkg->all_files[0] = NULL;

	/* Setup the +CONTENTS file */
	assert(f_pkg->contents != NULL);
	pkg_freebsd_contents_add_line(f_pkg->contents, PKG_LINE_COMMENT,
	    "PKG_FORMAT_REVISION:1.1");
	pkg_freebsd_contents_add_line(f_pkg->contents, PKG_LINE_NAME, pkg_name);

	return pkg;
}

/**
 * @brief Gets the contents struct from a package
 * @todo Remove the need for this function to be exported
 * @return The contents struct
 */
struct pkg_freebsd_contents *
pkg_freebsd_get_contents(struct pkg *pkg)
{
	struct freebsd_package *f_pkg;
	if (pkg == NULL)
		return NULL;

	f_pkg = (struct freebsd_package *)pkg->data;
	assert(f_pkg != NULL);

	/* Load the +CONTENTS file if it is NULL */
	assert(f_pkg->contents != NULL);
	return f_pkg->contents;
}

/**
 * @}
 */

/**
 * @defgroup FreebsdPackageCallbacks FreeBSD Package callbacks
 * @ingroup FreebsdPackage
 * @brief FreeBSD Package callback functions
 *
 * These are the callbacks the @link pkg package object @endlink uses
 * when pkg_new_freebsd_from_file(), pkg_new_freebsd_installed() or
 * pkg_new_freebsd_empty() is used to create the object
 *
 * @{
 */

/**
 * @brief Callback for pkg_get_version()
 *
 * @return A string containing the package version. Do not Free.
 */
static const char *
freebsd_get_version(struct pkg *pkg)
{
	struct freebsd_package *f_pkg;
	char *s;

	assert(pkg != NULL);
	assert(pkg->data != NULL);

	f_pkg = pkg->data;

	/* Check the package struct is correct
	 * If any fail it means there is a bug in the library
	 */
	assert(f_pkg->contents != NULL);
	assert(f_pkg->contents->lines != NULL);
	assert(f_pkg->contents->lines[0].data != NULL);
	assert(f_pkg->contents->lines[0].line_type == PKG_LINE_COMMENT);
	assert(strcmp("PKG_FORMAT_REVISION:1.1", f_pkg->contents->lines[0].data) == 0);
	s = strchr(f_pkg->contents->lines[0].data, ':');
	if (s == NULL)
		return NULL;
	s++;
	if (s[0] == '\0')
		return NULL;

	return s;
}

/**
 * @brief Callback for pkg_get_origin()
 * 
 * @return A string containing the origin of the Package. Do not free.
 */
static const char *
freebsd_get_origin(struct pkg *pkg)
{
	struct freebsd_package *f_pkg;

	assert(pkg != NULL);
	assert(pkg->data != NULL);

	f_pkg = pkg->data;
	return f_pkg->origin;
}

/**
 * @brief Callback for pkg_add_dependency()
 * @todo write
 * @return -1
 */
static int
freebsd_add_depend(struct pkg *pkg __unused, struct pkg *depend __unused)
{
	return -1;
}

/**
 * @brief Callback for pkg_add_file()
 * @todo rewrite to work
 * @return 0 on success or -1 on error
 */
static int
freebsd_add_file(struct pkg *pkg, struct pkg_file *file)
{
	struct freebsd_package *f_pkg;

	assert(pkg != NULL);
	assert(file != NULL);
	assert(pkg->data != NULL);

	f_pkg = pkg->data;

	assert(f_pkg->all_files != NULL);

	f_pkg->all_files_size += sizeof(struct pkg_file *);
	f_pkg->all_files = realloc(f_pkg->all_files, f_pkg->all_files_size);
	f_pkg->all_files[f_pkg->all_files_pos] = file;
	f_pkg->all_files_pos++;
	f_pkg->all_files[f_pkg->all_files_pos] = NULL;

	assert(f_pkg->contents != NULL);
	pkg_freebsd_contents_add_file(f_pkg->contents, file);
	
	return 0;
}

/**
 * @brief Callback for pkg_get_control_files()
 * @return An array of pkg_file or NULL
 */
static struct pkg_file **
freebsd_get_control_files(struct pkg *pkg)
{
	struct freebsd_package *f_pkg;

	assert(pkg != NULL);

	f_pkg = pkg->data;

	assert(f_pkg != NULL);

	return f_pkg->control;
}

/**
 * @brief for pkg_get_control_file()
 * @return The named pkg_file or NULL
 */
static struct pkg_file *
freebsd_get_control_file(struct pkg *pkg, const char *file)
{
	struct freebsd_package *f_pkg;
	unsigned int pos;

	assert(pkg != NULL);
	assert(file != NULL);

	f_pkg = pkg->data;
	assert(f_pkg != NULL);

	for (pos = 0; f_pkg->control[pos] != NULL; pos++)
		if (strcmp(basename(f_pkg->control[pos]->filename), file) == 0)
			return f_pkg->control[pos];

	return NULL;
}

/**
 * @brief Callback for pkg_get_next_file()
 * @return The next non-control pkg_file or NULL
 */
static struct pkg_file *
freebsd_get_next_file(struct pkg *pkg)
{
	struct freebsd_package *f_pkg;

	assert(pkg != NULL);

	f_pkg = pkg->data;
	assert(f_pkg != NULL);
	assert(f_pkg->archive != NULL);

	if (f_pkg->next) {
		struct pkg_file *ret;

		ret = f_pkg->next;
		f_pkg->next = NULL;

		return ret;
	}

	return freebsd_get_next_entry(f_pkg->archive);
}

/**
 * @brief Callback for pkg_get_dependencies()
 * @return An array of empty package objects, or NULL
 */
static struct pkg **
freebsd_get_deps(struct pkg *pkg)
{
	unsigned int line;
	struct pkg_freebsd_contents *contents;
	struct pkg **pkgs;
	unsigned int pkg_count;
	size_t pkg_size;

	assert(pkg != NULL);
	assert(pkg->data != NULL);

	/* If this is null there was an error that should have been checked */
	contents = ((struct freebsd_package *)pkg->data)->contents;
	assert(contents != NULL);

	pkg_count = 0;
	pkg_size = sizeof(struct pkg *);
	pkgs = malloc(pkg_size);
	if (!pkgs)
		return NULL;
	pkgs[0] = NULL;
	for (line = 0; line < contents->line_count; line++) {
		if (contents->lines[line].line_type == PKG_LINE_PKGDEP) {
			pkg_size += sizeof(struct pkg *);
			pkgs = realloc(pkgs, pkg_size);
			pkgs[pkg_count] = pkg_new_empty
			    (contents->lines[line].data);
			pkg_count++;
			pkgs[pkg_count] = NULL;
		}
	}
	return pkgs;
}

/**
 * @brief Callback for pkg_free()
 * @return 0
 */
static int
freebsd_free(struct pkg *pkg)
{
	assert(pkg != NULL);

	if (pkg->data)
		freebsd_free_package(pkg->data);

	return 0;
}

/**
 * @}
 */

/**
 * @defgroup FreebsdPackageInternals FreeBSD Package internal functions
 * @ingroup FreebsdPackage
 * @brief Internal package functions
 *
 * These are the internal FreeBSD package functions.
 * They are all declared static.
 * They are documented to help others understand the internals of a
 * package class callback system.
 *
 * @{
 */

/**
 * @brief Opens all the control files in a directory dir
 * @return An array of files or NULL
 */
static struct pkg_file **
freebsd_open_control_files(const char *pkg_db_dir)
{
	struct pkg_file **control;
	unsigned int control_size, control_count;
	DIR *d;
	struct dirent *de;
	

	d = opendir(pkg_db_dir);
	if (d == NULL)
		return NULL;

	/* Load all the + files into control */
	control_size = sizeof(struct pkg_file **);
	control = malloc(control_size);
	control[0] = NULL;
	control_count = 0;
	while ((de = readdir(d)) != NULL) {
		char *file;
	
		if (de->d_name[0] == '.') {
			continue;
		} else if (de->d_type != DT_REG) {
			closedir(d);
			FREE_CONTENTS(control);
			return NULL;
		} else if (de->d_name[0] != '+') {
			/* All files must begin with + */
			closedir(d);
			FREE_CONTENTS(control);
			return NULL;
		}
		asprintf(&file, "%s/%s", pkg_db_dir, de->d_name);
		if (!file) {
			closedir(d);
			FREE_CONTENTS(control);
			return NULL;
		}
		control_size += sizeof(struct pkg_file **);
		control = realloc(control, control_size);
		control[control_count] = pkg_file_new(file);
		control_count++;
		control[control_count] = NULL;
		free(file);
	}
	closedir(d);

	return control;
}

/**
 * @brief Retrieves a pointer to be placed into the data of the Package object
 *
 * @return A newley created freebsd_package or NULL
 */
static struct freebsd_package *
freebsd_get_package(FILE *fd, struct pkg_file **control)
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
	f_pkg->archive = NULL;
	f_pkg->next = NULL;
	f_pkg->all_files = NULL;
	f_pkg->all_files_size = 0;
	f_pkg->all_files_pos = 0;
	f_pkg->control = control;
	f_pkg->contents = NULL;
	f_pkg->origin = NULL;
	f_pkg->fd = fd;

	if (control != NULL) {
		unsigned int pos;
		char *ptr;

		/* Find the +CONTENTS file */
		for (pos = 0; control[pos] != NULL; pos++) {
			ptr = basename(control[pos]->filename);
			if (!strcmp(ptr, "+CONTENTS")) {
				break;
			}
		}
		if (control[pos] == NULL) {
			free(f_pkg);
			return NULL;
		}

		f_pkg->contents = pkg_freebsd_contents_new(
		    pkg_file_get(control[pos]));
	} else if (fd != NULL) {
		/*
		 * We only need to read from gzip and bzip2 as they
		 * are the only posible file types for FreeBSD packages
		 */
		f_pkg->archive = archive_read_new();
		if (!f_pkg->archive) {
			freebsd_free_package(f_pkg);
			return NULL;
		}
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
		/*
		 * Set the control files array to be big enough for
		 * the +CONTENTS file and a null terminator
		 */
		f_pkg->contents = pkg_freebsd_contents_new(pkg_file_get(file));

		control_size = sizeof(struct pkg_file *) * 2;
		f_pkg->control = malloc(control_size);
		f_pkg->control[0] = file;
		f_pkg->control[1] = NULL;
		control_pos = 1;

		assert(f_pkg->archive != NULL);
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
				f_pkg->control = realloc(f_pkg->control,
				    control_size);
				f_pkg->control[control_pos] = file;
				control_pos++;
				f_pkg->control[control_pos] = NULL;
			}
		}
	}

	return f_pkg;
}

/**
 * @brief Retrieves a pointer to the next file in an archive
 * @param a A libarchive(3) archive object
 * @return A the next file in the archive or NULL
 */
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

	/* Get the needed struct stat from the archive */
	sb = archive_entry_stat(entry);

	if (S_ISREG(sb->st_mode)) {
		/* Allocate enough space for the file and copy it to the string */
		length = archive_entry_size(entry);
		str = malloc(length+1);
		if (!str) {
			return NULL;
		}
		archive_read_data_into_buffer(a, str, length);
		str[length] = '\0';

		/* Create the pkg_file and return it */
		return pkg_file_new_from_buffer(archive_entry_pathname(entry),
			length, str, sb);
	} else if (S_ISLNK(sb->st_mode)) {
		str = strdup(archive_entry_symlink(entry));
		if (!str)
			return NULL;

		return pkg_file_new_symlink(archive_entry_pathname(entry),
		    str, sb);
	}
	errx(1, "File is not regular or symbolic link");
	return NULL;
}

/**
 * @brief Retrieves the package name from a +CONTENTS file
 * @param buffer A NULL terminated string containing the +CONTENTS file
 * @return The name of the package the file is from
 */
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

/**
 * @brief Function to free the internal package data struct
 * @return 0
 */
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

	if (f_pkg->control) {
		for (pos = 0; f_pkg->control[pos] != NULL; pos++)
			pkg_file_free(f_pkg->control[pos]);
		free(f_pkg->control);
		f_pkg->control = NULL;
	}

	if (f_pkg->next)
		pkg_file_free(f_pkg->next);
	pkg_freebsd_contents_free(f_pkg->contents);

	if (f_pkg->all_files) {
		for (pos = 0; f_pkg->all_files[pos] != NULL; pos++)
			pkg_file_free(f_pkg->all_files[pos]);

		free(f_pkg->all_files);
	}

	if (f_pkg->origin)
		free(f_pkg->origin);
	
	free(f_pkg);

	return 0;
}

/**
 * @brief Creates a string containing the command to run using printf
 * like substitutions
 *
 * @verbatim
 * Using fmt, replace all instances of:
 *
 * %F   With the parameter "name"
 * %D   With the parameter "dir"
 * %B   Return the directory part ("base") of %D/%F
 * %f   Return the filename part of %D/%F
 * @endverbatim
 *
 * @bug Does not check for overflow - caution!
 * @todo Move to the correct file
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

/**
 * @}
 */

