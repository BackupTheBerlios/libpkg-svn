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

#include <assert.h>
#include <errno.h>
#include <libgen.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pkg.h"
#include "pkg_private.h"

static struct pkgfile*  pkgfile_new(const char *, pkgfile_type);

/**
 * @defgroup PackageFileInternal Internal file functions
 * Internal functions in the package module
 * @ingroup PackageFile
 * @{
 */

/**
 * @brief Creates an empry pkgfile object
 * @return A new pkgfile object or NULL
 */
static struct pkgfile*
pkgfile_new(const char *filename, pkgfile_type type)
{
	struct pkgfile *file;

	file = malloc(sizeof(struct pkgfile));
	if (file == NULL)
		return NULL;

	file->name = strdup(filename);
	if (file->name == NULL) {
		pkgfile_free(file);
		return NULL;
	}

	file->type = type;
	file->fd = NULL;
	file->data = NULL;
	file->length = 0;

	return file;
}

/**
 * @}
 */

/**
 * @defgroup PackageFile Safe file handeling functions
 * @{
 */

/**
 * @brief Creates a new pkgfile object from the given file in the filesystem
 * @return A new pkgfile object or NULL
 */
struct pkgfile *
pkgfile_new_from_disk(const char *filename, int follow_link)
{
	struct stat sb;
	struct pkgfile *file;
	int i;

	errno = 0;
	i = lstat(filename, &sb);
	if (i != 0)
		return NULL;

	file = pkgfile_new(filename, pkgfile_none);
	if (file == NULL)
		return NULL;
	
	if (S_ISREG(sb.st_mode) || (follow_link && S_ISLNK(sb.st_mode))) {
		file->type = pkgfile_regular;
		/* Attempt to open file read/write */
		file->fd = fopen(file->name, "r+");
		if (file->fd == NULL) {
			/* Attempt to open file read only */
			file->fd = fopen(file->name, "r");
			if (file->fd == NULL) {
				pkgfile_free(file);
				return NULL;
			}
		}
	} else if(S_ISLNK(sb.st_mode)) {
		file->type = pkgfile_symlink;
	} else if (S_ISDIR(sb.st_mode)) {
		file->type = pkgfile_dir;
	} else {
		pkgfile_free(file);
		return NULL;
	}

	return file;
}

/**
 * @brief Creates a new regular file from a buffer
 * @return A new pkgfile object or NULL
 */
struct pkgfile*
pkgfile_new_regular(const char *name, const char *contents, uint64_t length)
{
	struct pkgfile *file;

	if (name == NULL || (contents == NULL && length > 0))
		return NULL;

	file = pkgfile_new(name, pkgfile_regular);
	if (file == NULL)
		return NULL;

	file->length = length;
	file->data = malloc(file->length);
	if (file->data == NULL) {
		pkgfile_free(file);
		return NULL;
	}
	memcpy(file->data, contents, file->length);

	return file;
	
}

/**
 * @brief Creates a new symlink pkgfile object containing the given data
 * @return A new pkgfile object or NULL
 */
struct pkgfile*
pkgfile_new_symlink(const char *file, const char *data)
{
	struct pkgfile *pkgfile;

	if (file == NULL || data == NULL)
		return NULL;

	pkgfile = pkgfile_new(file, pkgfile_symlink);
	if (pkgfile == NULL)
		return NULL;

	pkgfile->data = strdup(data);
	if (pkgfile->data == NULL) {
		pkgfile_free(pkgfile);
		return NULL;
	}

	return pkgfile;
}

/**
 * @brief Creates a new hardlink pkgfile object pointing to another file
 * @return A new pkgfile object or NULL
 */
struct pkgfile*
pkgfile_new_hardlink(const char *file, const char *other_file)
{
	struct pkgfile *pkgfile;

	if (file == NULL || other_file == NULL)
		return NULL;

	pkgfile = pkgfile_new(file, pkgfile_hardlink);
	if (pkgfile == NULL)
		return NULL;

	pkgfile->data = strdup(other_file);
	if (pkgfile->data == NULL) {
		pkgfile_free(pkgfile);
		return NULL;
	}

	return pkgfile;
}

/**
 * @brief Creates a new directory pkgfile object
 * @return A new pkgfile object or NULL
 */
struct pkgfile*
pkgfile_new_directory(const char *dir)
{
	struct pkgfile *file;

	if (dir == NULL)
		return NULL;

	file = pkgfile_new(dir, pkgfile_dir);
	if (file == NULL)
		return NULL;

	file->data = strdup(dir);
	if (file->data == NULL) {
		pkgfile_free(file);
		return NULL;
	}

	return file;
}

/**
 * @brief Retrieves the name of a file
 * @return A null-terminated string with the filename or NULL
 */
const char *
pkgfile_get_name(struct pkgfile *file)
{
	if (file == NULL)
		return NULL;
	return file->name;
}

/**
 * @brief Get the size of a file
 * @return The file size. 0 is used for an empty file or error
 */
uint64_t
pkgfile_get_size(struct pkgfile *file)
{
	if (file == NULL)
		return 0;

	assert(file->type != pkgfile_none);
	assert(file->type != pkgfile_hardlink);
	assert(file->type != pkgfile_dir);

	switch (file->type) {
		case pkgfile_none:
		case pkgfile_dir:
		case pkgfile_hardlink:
			break;
		case pkgfile_regular:
			if (file->fd != NULL) {
				struct stat sb;
				fstat(fileno(file->fd), &sb);
				return sb.st_size;
			} else if (file->data != NULL) {
				return file->length;
			}
			break;
		case pkgfile_symlink:
			if (file->data != NULL) {
				return strlen(file->data);
			}
			break;
	}

	return 0;
}

/**
 * @brief Reads up to length bytes from a file
 * @return A string containing the data or NULL
 */
char *
pkgfile_get_data(struct pkgfile *file, uint64_t length)
{
	char *data;
	data = NULL;
	if (file == NULL)
		return NULL;

	assert(file->type != pkgfile_none);
	assert(file->type != pkgfile_hardlink);
	assert(file->type != pkgfile_dir);

	switch (file->type) {
		case pkgfile_none:
		case pkgfile_hardlink:
		case pkgfile_dir:
			break;
		case pkgfile_regular:
			/** @todo check length < size left in file */
			data = malloc(length);
			if (data == NULL)
				return NULL;
			if (file->fd != NULL) {
				/*
				 * Read up to length bytes
				 * from the file to data
				 */
				size_t len;

				len = fread(data, 1, length, file->fd);
			} else if (file->data != NULL) {
				memcpy(data, file->data, length);
			}
			break;
		case pkgfile_symlink:
			if (file->data == NULL)
				return NULL;
			data = strdup(file->data);
	}
	
	return data;
}

/**
 * @brief Reads the entire contents of a file
 * @return A string containing the entire file or NULL
 */
char*
pkgfile_get_data_all(struct pkgfile *file)
{
	uint64_t size;

	if (file == NULL)
		return NULL;

	size = pkgfile_get_size(file);
	return pkgfile_get_data(file, size);
}

/**
 * @brief Seeks to a given position in a file
 * @return 0 on success or -1 on error
 */
int
pkgfile_seek(struct pkgfile *file, uint64_t position, int whence)
{
	if (file == NULL)
		return -1;

	assert(file->type != pkgfile_none);
	assert(file->type != pkgfile_hardlink);
	assert(file->type != pkgfile_symlink);
	assert(file->type != pkgfile_dir);

	if (file->type == pkgfile_regular) {
		assert(file->fd != NULL);
		if (file->fd != NULL) {
			if (fseek(file->fd, position, whence) != 0)
				return -1;
		}
	}
	return 0;
}

/**
 * @brief Writes a pkgfile to disk
 * @return 0 on success or -1 on error
 */
int
pkgfile_write(struct pkgfile *file)
{
	if (file == NULL)
		return -1;

	assert(file->type != pkgfile_none);
	assert(file->type != pkgfile_hardlink);
	assert(file->type != pkgfile_dir);

	switch (file->type) {
	case pkgfile_none:
		break;
	case pkgfile_regular:
		if (file->data != NULL) {
			uint64_t length;
			struct stat sb;
			size_t write_size;
			char *buf;
			FILE *fd;

			assert(file->fd == NULL);
			fd = fopen(file->name, "a");
			if (fd == NULL) {
				char *dir_name;

				/*
				 * The open failed, try running mkdir -p
				 * on the dir and opening again
				 */
				dir_name = dirname(file->name);
				pkg_dir_build(dir_name);
				fd = fopen(file->name, "a");
				if (fd == NULL) {
					return -1;
				}
				/* We created the file */
			} else if (fstat(fileno(fd), &sb) != 0) {
				/* And fstat can find it */
				fclose(fd);
				return -1;
			} else if (!S_ISREG(sb.st_mode)) {
				/* And it is regular */
				fclose(fd);
				return -1;
			} else if (sb.st_size > 0) {
				/* And the file is empty */
				fclose(fd);
				return -1;
			} else if (sb.st_nlink != 1) {
				/* And there are no hardlinks to it */
				fclose(fd);
				return -1;
			}
			/* We can now write to the file */
			buf = file->data;

			length = file->length;
			while (length > 0) {
				write_size = fwrite(buf, 1, length, fd);
				length -= write_size;
				buf += write_size;
				if (write_size == 0) {
					assert(0);
					break;
				}
			}
			fclose(fd);
		}
		break;
	case pkgfile_hardlink:
		break;
	case pkgfile_symlink:
		if (symlink(file->name, file->data) != 0)
			return -1;
		break;
	case pkgfile_dir:
		break;
	}

	return 0;
}

/**
 * @brief Frees a pkgfile object
 * @return 0 on success or -1 on error
 */
int
pkgfile_free(struct pkgfile *file)
{
	if (file == NULL)
		return -1;

	if (file->name != NULL)
		free(file->name);

	if (file->fd != NULL)
		fclose(file->fd);

	if (file->data != NULL)
		free(file->data);
	
	free(file);

	return 0;
}

/**
 * @}
 */
