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
#include <sys/mman.h>

#include <assert.h>
#include <errno.h>
#include <libgen.h>
#include <limits.h>
#include <md5.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "pkg.h"
#include "pkg_private.h"

static struct pkgfile	*pkgfile_new(const char *, pkgfile_type, pkgfile_loc);
static int		 pkgfile_open_fd(struct pkgfile *);
static int		 pkgfile_get_type(struct pkgfile *);

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
pkgfile_new(const char *filename, pkgfile_type type, pkgfile_loc location)
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
	file->loc = location;
	file->follow_link = 0;
	file->fd = NULL;
	file->data = NULL;
	file->length = 0;
	file->mode = 0;
	file->md5[0] = '\0';

	return file;
}

/**
 * @brief Creates a FILE pointer in the given pkgfile object
 * @return  0 On success
 * @return -1 On error
 */
static int
pkgfile_open_fd(struct pkgfile *file)
{
	/* Consistancy check */
	assert(file != NULL);
	assert(file->loc == pkgfile_loc_disk);

	if (pkgfile_get_type(file) != 0)
		return -1;

	if (file->type == pkgfile_regular) {
		/* Check if the file has already been opened */
		if (file->fd != NULL)
			return 0;

		/* Open the file read write */
		file->fd = fopen(file->name, "r+");
		if (file->fd == NULL) {
			/* Attempt to open file read only */
			file->fd = fopen(file->name, "r");
		}

		/* If we failed return -1 */
		if (file->fd == NULL) {
			return -1;
		}

	}

	return 0;
}

/**
 * @brief Gets a file's type from disk
 *
 * This is to be used when the file's type in needed but
 * the file dosn't need to be opened
 * @return  0 on success
 * @return -1 on error
 */
int
pkgfile_get_type(struct pkgfile *file)
{
	assert(file != NULL);
	assert(file->loc == pkgfile_loc_disk);

	/* Find the file type */
	if (file->type == pkgfile_none) {
		struct stat sb;

		if (lstat(file->name, &sb) != 0)
			return -1;

		if (S_ISREG(sb.st_mode) ||
		    (file->follow_link && S_ISLNK(sb.st_mode))) {
			file->type = pkgfile_regular;
			file->length = sb.st_size;
		} else if(S_ISLNK(sb.st_mode)) {
			file->type = pkgfile_symlink;
		} else if (S_ISDIR(sb.st_mode)) {
			file->type = pkgfile_dir;
		} else {
			return -1;
		}
	}
	return 0;
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
	struct pkgfile *file;

	file = pkgfile_new(filename, pkgfile_none, pkgfile_loc_disk);
	if (file == NULL)
		return NULL;

	file->follow_link = follow_link;

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

	file = pkgfile_new(name, pkgfile_regular, pkgfile_loc_mem);
	if (file == NULL)
		return NULL;

	file->length = length;
	if (file->length == 0) {
		file->data = NULL;
	} else {
		file->data = malloc(file->length);
		if (file->data == NULL) {
			pkgfile_free(file);
			return NULL;
		}
		memcpy(file->data, contents, file->length);
	}

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

	pkgfile = pkgfile_new(file, pkgfile_symlink, pkgfile_loc_mem);
	if (pkgfile == NULL)
		return NULL;

	pkgfile->length = strlen(data);
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

	pkgfile = pkgfile_new(file, pkgfile_hardlink, pkgfile_loc_mem);
	if (pkgfile == NULL)
		return NULL;

	pkgfile->length = strlen(other_file);
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

	file = pkgfile_new(dir, pkgfile_dir, pkgfile_loc_mem);
	if (file == NULL)
		return NULL;

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

	if (file->loc == pkgfile_loc_disk)
		pkgfile_open_fd(file);

	assert(file->type != pkgfile_none);

	switch (file->type) {
		case pkgfile_none:
			break;
		case pkgfile_dir:
			if (file->length == 0)
				file->length = strlen(file->name);
			return file->length;
			break;
		case pkgfile_hardlink:
			assert(file->loc == pkgfile_loc_mem);
			if (file->loc == pkgfile_loc_mem) {
				return file->length;
			}
			break;
		case pkgfile_regular:
			if (file->loc == pkgfile_loc_disk) {
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
 * @todo Change to return "const char *" and not do the strdup
 */
const char *
pkgfile_get_data(struct pkgfile *file)
{
	if (file == NULL)
		return NULL;

	if (file->loc == pkgfile_loc_disk)
		pkgfile_open_fd(file);

	assert(file->type != pkgfile_none);

	switch (file->type) {
	case pkgfile_none:
		break;
	case pkgfile_dir:
		return file->name;
	case pkgfile_hardlink:
		assert(file->loc == pkgfile_loc_mem);
		if (file->loc == pkgfile_loc_mem) {
			if (file->data == NULL)
				return NULL;
			return file->data;
		}
		break;
	case pkgfile_regular:
		if (file->loc == pkgfile_loc_disk) {
			/* Load the file to the data pointer */
			if (file->data == NULL) {
				file->data = malloc(file->length);
				if (file->data == NULL)
					return NULL;
				/*
				 * Read up to length bytes
				 * from the file to data
				 */
				/** @todo check length < size left in file */
				fread(file->data, 1, file->length, file->fd);
			}
		}
	case pkgfile_symlink:
		return file->data;
	}
	
	return NULL;
}

/**
 * @brief Sets the expected md5 of a file
 * @return  0 on success
 * @return -1 on error
 */
int
pkgfile_set_checksum_md5(struct pkgfile *file, const char *md5)
{
	if (file == NULL || md5 == NULL || strlen(md5) != 32)
		return -1;

	strlcpy(file->md5, md5, 33);
	return 0;
}

/**
 * @brief Compares a file's MD5 checksum with the version on disk
 * @return  1 if the recorded checksum is different to the disk checksum
 * @return  0 if the recorded checksum is the same as the disk checksum
 * @return -1 if there is a problem with the file object
 */
int
pkgfile_compare_checksum_md5(struct pkgfile *file)
{
	char checksum[33];

	if (file == NULL || file->md5[0] == '\0')
		return -1;

	if (file->loc == pkgfile_loc_disk)
		pkgfile_open_fd(file);

	assert(file->type != pkgfile_none);
	assert(file->type != pkgfile_dir);

	switch (file->type) {
		case pkgfile_none:
		case pkgfile_dir:
			return -1;
		case pkgfile_hardlink:
		{
			char real_file[MAXPATHLEN + 1];

			getcwd(real_file, MAXPATHLEN + 1);
			snprintf(real_file, MAXPATHLEN + 1, "%s/%s", real_file,
			    file->data);
			MD5File(real_file, checksum);
			break;
		}
		case pkgfile_symlink:
		case pkgfile_regular:
		{
			/*
			 * Make sure the data has been loaded
			 * then calculate the checksum
			 */
			pkgfile_get_data(file);
			MD5Data(file->data, file->length, checksum);

			break;
		}
	}
	if (strncmp(checksum, file->md5, 32) == 0)
		return 0;

	return 1;
}

/**
 * @brief Unlinkes the given file
 * @return  0 on success
 * @return -1 on error
 */
int
pkgfile_unlink(struct pkgfile *file)
{
	if (file == NULL)
		return -1;

	assert(file->loc == pkgfile_loc_disk);

	pkgfile_get_type(file);
	if (file->type == pkgfile_dir) {
		return rmdir(file->name);
	} else {
		return unlink(file->name);
	}
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

	if (file->loc == pkgfile_loc_disk)
		pkgfile_open_fd(file);

	assert(file->type != pkgfile_none);
	assert(file->type != pkgfile_hardlink);
	assert(file->type != pkgfile_symlink);
	assert(file->type != pkgfile_dir);

	if (file->type == pkgfile_regular) {
		assert(file->loc == pkgfile_loc_disk);
		if (file->fd != NULL) {
			if (fseek(file->fd, position, whence) != 0)
				return -1;
		} else {
			return -1;
		}
	}
	return 0;
}

/**
 * @brief Sets the given file's mode
 * @param file The file to set the mode on
 * @param mode The mode to set. 0 will unset it
 * @return  0 on success
 * @return -1 on error
 */
int
pkgfile_set_mode(struct pkgfile *file, mode_t mode)
{
	if (file == NULL)
		return -1;

	file->mode = mode & ALLPERMS;
	return 0;
}

/**
 * @brief Removes the first occurance of line from a file
 * @param file The file
 * @param file The line to remove
 * @return  1 on line not found
 * @return  0 on success
 * @return -1 on error
 */
int
pkgfile_remove_line(struct pkgfile *file, const char *line)
{
	char *buf, *ptr;

	if (file == NULL || line == NULL)
		return -1;

	assert(file->loc == pkgfile_loc_disk);

	/* Read in the file */
	pkgfile_get_data(file);
	assert(file->type == pkgfile_regular);

	buf = file->data;
	while ((buf = memmem(buf, file->length, line, strlen(line))) != NULL) {
		/* Check the found line is complete */
		if ((buf == file->data || buf[-1] == '\n') &&
		    (buf + strlen(line) == file->data + file->length || 
		     buf[strlen(line)] == '\n')) {
			break;
		}
	}
	if (buf == NULL)
		return 1;

	/* Move the rest of the file */
	ptr = buf + strlen(line) + 1;
	memcpy(buf, ptr, file->length - (ptr - file->data));
	file->length -= strlen(line) + 1;
	fseek(file->fd, 0, SEEK_SET);
	if (fwrite(file->data, 1, file->length, file->fd) != file->length) {
		assert(0);
		return -1;
	}
	ftruncate(fileno(file->fd), file->length);

	return 0;
}

/**
 * @brief Writes a pkgfile to disk
 * @return  0 on success
 * @return -1 on error
 */
int
pkgfile_write(struct pkgfile *file)
{
	if (file == NULL)
		return -1;

	if (file->loc == pkgfile_loc_disk)
		pkgfile_open_fd(file);

	assert(file->type != pkgfile_none);

	switch (file->type) {
	case pkgfile_none:
		return -1;
	case pkgfile_regular:
		if (file->loc == pkgfile_loc_mem) {
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
			}
			if (fstat(fileno(fd), &sb) != 0) {
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
			if (file->data != NULL) {
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
			}

			if (file->mode != 0)
				fchmod(fileno(fd), file->mode);

			fclose(fd);
		}
		break;
	case pkgfile_hardlink:
		if (link(file->data, file->name) != 0)
			return -1;
		break;
	case pkgfile_symlink:
		if (symlink(file->data, file->name) != 0)
			return -1;
		break;
	case pkgfile_dir:
#define DEF_MODE (S_IRWXU|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH)
		if (mkdir(file->name,
		    (file->mode == 0 ? DEF_MODE : file->mode)) != 0) {
			return -1;
		}
#undef DEF_MODE
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
