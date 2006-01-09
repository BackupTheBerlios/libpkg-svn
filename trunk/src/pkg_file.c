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
#include <libgen.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pkg.h"
#include "pkg_private.h"

/*
 * Creates a new pkg_file from a file on the file system
 */
struct pkg_file *
pkg_file_new(const char *filename)
{
	struct pkg_file *file;
	FILE *fd;
	struct stat sb;
	uint64_t length;

	if (!filename)
		return NULL;

	fd = fopen(filename, "r");
	if (fd == NULL) {
		return NULL;
	}
	fstat(fileno(fd), &sb);
	length = sb.st_size;

	file = pkg_file_new_from_buffer(filename, length, NULL, &sb);
	if (!file) {
		fclose(fd);
		return NULL;
	}
	file->fd = fd;
	return file;
}

/*
 * Creates a new pkg_file from a buffer
 */
struct pkg_file *
pkg_file_new_from_buffer(const char *filename, uint64_t length, char *buffer,
		const struct stat *sb)
{
	struct pkg_file *file;

	if (!filename)
		return NULL;
	
	file = malloc(sizeof(struct pkg_file));
	if (!file) {
		return NULL;
	}

	file->filename = strdup(filename);
	if (!file->filename) {
		free(file);
		return NULL;
	}
	if (sb == NULL) {
		file->stat = NULL;
	} else {
		file->stat = malloc(sizeof(struct stat));
		if (!file->stat) {
			free(file->filename);
			free(file);
			return NULL;
		}
		memcpy(file->stat, sb, sizeof(struct stat));
	}
	file->len = length;
	file->contents = buffer;
	file->fd = NULL;

	return file;
}

/*
 * Frees a pkg_file
 */
int
pkg_file_free(struct pkg_file *file)
{
	if (!file) {
		return -1;
	}

	if (file->filename)
		free(file->filename);

	if (file->contents)
		free(file->contents);

	if (file->stat)
		free(file->stat);

	if (file->fd)
		fclose(file->fd);

	free(file);

	return 0;
}

/*
 * Writes a file to the filesystem
 */
int
pkg_file_write(struct pkg_file *file)
{
	/* Install a file to the correct directory */
	FILE *fd;
	struct stat sb;

	if (!file) {
		return -1;
	}

	if (file->stat) {
		/* Check the file to be written is regular */
		if (!S_ISREG(file->stat->st_mode)) {
			return -1;
		}
	}

	/* Open the file to append to it */
	fd = fopen(file->filename, "a");
	if (fd == NULL) {
		char *dir_name;

		/*
		 * The open failed, try running mkdir -p
		 * on the dir and opening again
		 */
		dir_name = dirname(file->filename);
		pkg_dir_build(dir_name);
		fd = fopen(file->filename, "a");
		if (fd == NULL) {
			return -1;
		}
	}
	/* Check the file we just created is a regular file */
	fstat(fileno(fd), &sb);
	if (!S_ISREG(sb.st_mode)) {
		fclose(fd);
		return -1;
	} else if (sb.st_size > 0) {
		/* And the file is empty */
		fclose(fd);
		return -1;
	}

	/* Write the file to disk */
	fwrite(file->contents, file->len, 1, fd);

	if (file->stat) {
		/* Set the correct permission for the file */
		fchmod(fileno(fd), file->stat->st_mode);
	}

	fclose(fd);

	return 0;
}

/*
 * Gets to contents of file
 */
char *
pkg_file_get(struct pkg_file *file)
{
	if (file == NULL || (file->contents == NULL && file->fd == NULL))
		return NULL;

	if (file->contents == NULL) {
		file->contents = malloc(file->len + 1);
		if (file->contents == NULL) {
			return NULL;
		}

		/* 
		 * XXX fread can only handle up to SIZE_T_MAX so fail
		 * if the file is bigger until a better file reader is written
		 */
		assert(file->len <= SIZE_T_MAX);
		fread(file->contents, 1, file->len, file->fd);
		file->contents[file->len] = '\0';
	}

	return file->contents;
}
