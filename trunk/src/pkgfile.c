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
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "pkg.h"
#include "pkg_private.h"

static struct pkgfile	*pkgfile_new(const char *, pkgfile_type, pkgfile_loc);
static int		 pkgfile_open_fd(struct pkgfile *);
static int		 pkgfile_get_type(struct pkgfile *);
static const char	*pkgfile_real_name(struct pkgfile *);

static const char *pkgfile_types[] =
	{ "none", "file", "hardlink", "symlink", "directory" };

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

	file->cwd = NULL;
	file->real_name = NULL;
	file->type = type;
	file->loc = location;
	file->follow_link = 0;
	file->fd = NULL;
	file->data = NULL;
	file->length = 0;
	file->offset = 0;
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
static int
pkgfile_get_type(struct pkgfile *file)
{
	assert(file != NULL);

	/* If this is a file from a buffer it will already have a type */
	if (file->loc != pkgfile_loc_disk)
		return 0;

	/* Find the file type */
	if (file->type == pkgfile_none) {
		struct stat sb;

		if (lstat(pkgfile_real_name(file), &sb) != 0)
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
 * @brief Gets the real name of a file including it's full directory
 * @param file The file to find the name for
 * @return The file's name
 */
static const char *
pkgfile_real_name(struct pkgfile *file)
{
	assert(file != NULL);
	assert(file->name != NULL);

	if (file->real_name == NULL) {
		if (file->name[0] != '/' && file->cwd != NULL) {
			asprintf(&file->real_name, "%s/%s", file->cwd,
			    file->name);
		} else {
			return file->name;
		}
	}

	return file->real_name;
}

/**
 * @brief funopen callback used to read with a FILE pointer
 * @param pkgfile The file to read
 * @param buf The buffer to read to
 * @param len The length of data to read
 */
static int
pkgfile_fileptr_read(void *pkgfile, char *buf, int len)
{
	struct pkgfile *file;

	file = pkgfile;
	if (len <= 0 || file->offset >= file->length)
		return 0;

	/* Read in the data */
	pkgfile_get_data(file);

	/* Stop reading past the end of the file */
	if (file->offset + len > file->length)
		len = file->length - file->offset;

	/* Fill the buffer with the data */
	memcpy(buf, file->data + file->offset, len);
	file->offset += len;

	return len;
}
/**
 * @}
 */

/**
 * @defgroup PackageFile Safe file handling functions
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
	return pkgfile_real_name(file);
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
			return file->data;
		}
		break;
	case pkgfile_regular:
		if (file->loc == pkgfile_loc_disk) {
			/* Load the file to the data pointer */
			if (file->data == NULL && file->length > 0) {
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
 * @brief Creates a FILE pointer to be passed to things like fread
 * @param file The pkgfile to read from
 * @return A FILE pointer
 * @return NULL on error
 */
FILE *
pkgfile_get_fileptr(struct pkgfile *file)
{
	if (file == NULL)
		return NULL;

	if (file->loc == pkgfile_loc_disk)
		pkgfile_open_fd(file);

	assert(file->type != pkgfile_none);

	return fropen(file, pkgfile_fileptr_read);
}

/**
 * @brief Gets a string containing a description of the type of the file
 * @return A null terminated string with the name of the file type
 */
const char *
pkgfile_get_type_string(struct pkgfile *file)
{
	if (file == NULL)
		return NULL;

	pkgfile_get_type(file);
	return pkgfile_types[file->type];
}

/**
 * @brief Sets the current working directory for file access
 * @return  0 on success
 * @return -1 on failure
 */
int
pkgfile_set_cwd(struct pkgfile *file, const char *cwd)
{
	if (file == NULL)
		return -1;

	if (cwd == NULL)
		return -1;

	/* Force the next call to pkgfile_real_name to rebuild the real name */
	if (file->real_name != NULL) {
		free(file->real_name);
		file->real_name = NULL;
	}

	if (file->cwd != NULL)
		free(file->cwd);

	if (cwd == NULL) {
		file->cwd = NULL;
	} else {
		file->cwd = strdup(cwd);
		if (file->cwd == NULL)
			return -1;
	}

	return 0;
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

	if (file->loc == pkgfile_loc_mem)
		return -1;

	assert(file->loc == pkgfile_loc_disk);

	pkgfile_get_type(file);
	if (file->type == pkgfile_dir) {
		return rmdir(pkgfile_real_name(file));
	} else {
		return unlink(pkgfile_real_name(file));
	}
}

/**
 * @brief Seeks to a given position in a file
 * @return 0 on success or -1 on error
 */
int
pkgfile_seek(struct pkgfile *file, int64_t position, int whence)
{
	if (file == NULL)
		return -1;

	if (file->loc == pkgfile_loc_disk)
		pkgfile_open_fd(file);

	assert(file->type != pkgfile_none);

	if (file->type == pkgfile_regular) {
		if (file->loc == pkgfile_loc_disk) {
			if (file->fd != NULL) {
				if (fseek(file->fd, position, whence) != 0)
					return -1;
			} else {
				return -1;
			}
		} else {
			switch (whence) {
			case SEEK_SET:
				if (position >= 0) {
					file->offset = position;
				} else {
					errno = EINVAL;
					return -1;
				}
				break;
			case SEEK_CUR:
				if (position < 0 &&
				    (uint64_t)(-position) > file->offset) {
					file->offset = 0;
					errno = EINVAL;
					return -1;
				}
				file->offset += position;
				break;
			case SEEK_END:
				if (position < 0 &&
				    (uint64_t)(-position) > file->length) {
					file->offset = 0;
					errno = EINVAL;
					return -1;
				}
				file->offset = file->length + position;
				break;
			}
			if (file->offset > file->length) {
				file->offset = file->length;
			}
		}
	} else {
		return -1;
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
 * @brief Finds a given line in a file
 * @param file The file
 * @param line The line to remove
 * @return A pointer to the start of the line
 * @return NULL if the line was not found
 */
const char *
pkgfile_find_line(struct pkgfile *file, const char *line)
{
	char *buf;

	if (file == NULL || line == NULL)
		return NULL;

	pkgfile_get_type(file);
	if (file->type != pkgfile_regular)
		return NULL;

	/* Read in the file */
	pkgfile_get_data(file);

	buf = file->data;
	/** @todo Change the length of the buffer left on each iteration */
	while ((buf = memmem(buf, file->length, line, strlen(line))) != NULL) {
		/* Check the found line is complete */
		if ((buf == file->data || buf[-1] == '\n') &&
		    (buf + strlen(line) == file->data + file->length || 
		     buf[strlen(line)] == '\n')) {
			break;
		}
	}

	return buf;
}
/**
 * @brief Removes the first occurance of line from a file
 * @param file The file
 * @param line The line to remove
 * @return  1 on line not found
 * @return  0 on success
 * @return -1 on error
 */
int
pkgfile_remove_line(struct pkgfile *file, const char *line)
{
	union { const char *in; char *out; } buf;
	char *ptr;

	if (file == NULL || line == NULL)
		return -1;

	pkgfile_get_type(file);
	if (file->type != pkgfile_regular)
		return -1;

	/* Find the line in the file to remove */
	buf.in = pkgfile_find_line(file, line);

	/* Move the rest of the file */
	ptr = buf.out + strlen(line) + 1;
	memcpy(buf.out, ptr, file->length - (ptr - file->data));
	file->length -= strlen(line) + 1;

	if (file->loc == pkgfile_loc_disk) {
		fseek(file->fd, 0, SEEK_SET);
		if (fwrite(file->data, 1, file->length, file->fd) !=
		    file->length) {
			assert(0);
			return -1;
		}
		ftruncate(fileno(file->fd), file->length);
	}

	return 0;
}

/**
 * @brief Appends data to the end of a file
 * @param file The file to append data to
 * @param data The data to append
 * @param length The length of the data to append
 * @return  0 on success
 * @return -1 on error
 */
int
pkgfile_append(struct pkgfile *file, const char *data, uint64_t length)
{
	if (file == NULL)
		return -1;

	if (data == NULL && length != 0)
		return -1;

	assert(file->loc == pkgfile_loc_mem);
	if (file->type != pkgfile_regular)
		return -1;

	assert(file->length == 0 || file->data != NULL);
	if (file->data != NULL) {
		char *new_data;

		new_data = realloc(file->data, file->length + length);
		if (new_data == NULL)
			return -1;

		/* Update the internal pointer and copy the new data */
		file->data = new_data;
	} else {
		file->data = malloc(length);
		if (file->data == NULL) {
			return -1;
		}
	}
	/* Append the data to the file */
	memcpy(file->data + file->length, data, length);
	file->length += length;

	return 0;
}

/**
 * @brief Appends a null terminated string to the end of a file
 * @param file The file to append
 * @param format A printf(3) like format string
 * @return  0 on success
 * @return -1 on failure
 */
int
pkgfile_append_string(struct pkgfile *file, const char *format, ...)
{
	char *buf;
	int len, ret;
	va_list ap;

	if (file == NULL || format == NULL)
		return -1;

	/* Build a buffer from the format string */
	va_start(ap, format);
	len = vasprintf(&buf, format, ap);
	if (buf == NULL)
		return -1;
	va_end(ap);

	ret = pkgfile_append(file, buf, len);
	free(buf);

	return ret;
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
			fd = fopen(pkgfile_real_name(file), "a");
			if (fd == NULL) {
				char *dir_name;

				/*
				 * The open failed, try running mkdir -p
				 * on the dir and opening again
				 */
				dir_name = dirname(pkgfile_real_name(file));
				pkg_dir_build(dir_name, 0);
				fd = fopen(pkgfile_real_name(file), "a");
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
		if (link(file->data, file->name) != 0) {
			char *dir_name;
			if (errno != ENOENT)
				return -1;

			dir_name = dirname(pkgfile_real_name(file));
			pkg_dir_build(dir_name, 0);
			if (link(file->data, file->name) != 0)
				return -1;
		}
		break;
	case pkgfile_symlink:
		if (symlink(file->data, file->name) != 0) {
			char *dir_name;
			if (errno != ENOENT)
				return -1;

			dir_name = dirname(pkgfile_real_name(file));
			pkg_dir_build(dir_name, 0);
			if (symlink(file->data, file->name) != 0)
				return -1;
		}
		break;
	case pkgfile_dir:
		if (pkg_dir_build(pkgfile_real_name(file), file->mode) != 0)
			return -1;
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

	if (file->cwd != NULL)
		free(file->cwd);

	if (file->real_name != NULL)
		free(file->real_name);

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
