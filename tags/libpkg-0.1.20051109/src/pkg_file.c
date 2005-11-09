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

#include <libgen.h>
#include <stdlib.h>
#include <string.h>

#include "pkg.h"
#include "pkg_private.h"

/*
 * Creates a new pkg_file from a buffer
 */
struct pkg_file *
pkg_file_new_from_buffer(const char *filename, uint64_t length, char *buffer,
		const struct stat *sb)
{
	struct pkg_file *file;

	file = malloc(sizeof(struct pkg_file));
	if (!file) {
		pkg_error_set(&pkg_null, "Out of Memory");
		return NULL;
	}

	file->filename = strdup(filename);
	if (!file->filename) {
		free(file);
		pkg_error_set(&pkg_null, "Out of Memory");
		return NULL;
	}
	if (sb == NULL) {
		file->stat = NULL;
	} else {
		file->stat = malloc(sizeof(struct stat));
		if (!file->stat) {
			free(file->filename);
			free(file);
			pkg_error_set(&pkg_null, "Out of Memory");
			return NULL;
		}
		memcpy(file->stat, sb, sizeof(struct stat));
	}
	file->len = length;
	file->contents = buffer;

	file->pkg_object.error_str = NULL;

	return file;
}

/*
 * Frees a pkg_file
 */
int
pkg_file_free(struct pkg_file *file)
{
	if (!file) {
		pkg_error_set(&pkg_null, "No file specified");
		return PKG_FAIL;
	}

	if (file->filename)
		free(file->filename);

	if (file->contents)
		free(file->contents);

	if (file->stat)
		free(file->stat);

	free(file);

	return PKG_OK;
}

/* Writes a file to the filesystem */
int
pkg_file_write(struct pkg_file *file)
{
	/* Install a file to the correct directory */
	FILE *fd;
	struct stat sb;

	if (!file) {
		pkg_error_set(&pkg_null, "No file specified");
		return PKG_FAIL;
	}

	if (file->stat) {
		/* Check the file to be written is regular */
		if (!S_ISREG(file->stat->st_mode)) {
			pkg_error_set((struct pkg_object *)file,
				    "Can't write a non-regular file");
			return PKG_FAIL;
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
			pkg_error_set((struct pkg_object *)file,
			    "Could not create file %s", file->filename);
			return PKG_FAIL;
		}
	}
	/* Check the file we just created is a regular file */
	fstat(fileno(fd), &sb);
	if (!S_ISREG(sb.st_mode)) {
		fclose(fd);
		pkg_error_set((struct pkg_object *)file, "Not a regular file");
		return PKG_FAIL;
	} else if (sb.st_size > 0) {
		/* And the file is empty */
		fclose(fd);
		pkg_error_set((struct pkg_object *)file, "File already exists");
		return PKG_FAIL;
	}

	/* Write the file to disk */
	fwrite(file->contents, file->len, 1, fd);

	if (file->stat) {
		/* Set the correct permission for the file */
		fchmod(fileno(fd), file->stat->st_mode);
	}

	fclose(fd);

	return PKG_OK;
}

/*
 * Adds a file to the head of a list
 */
struct pkg_file_list *
pkg_file_list_add(struct pkg_file_list *list, struct pkg_file *file)
{
	struct pkg_file_list *new;

	new = malloc(sizeof(struct pkg_file_list));
	if (!new) {
		pkg_error_set(&pkg_null, "Out of Memory");
		return NULL;
	}

	new->next = list;
	new->file = file;

	new->pkg_object.error_str = NULL;

	return new;
}

/*
 * Finds a file in a list
 */
struct pkg_file *
pkg_file_list_get_file(struct pkg_file_list *list, const char *name)
{
	struct pkg_file_list *cur;

	if (!list) {
		pkg_error_set(&pkg_null, "No file list specified");
		return NULL;
	}

	if (!name) {
		pkg_error_set((struct pkg_object *)list, "No name specified");
		return NULL;
	}

	cur = list;

	while (cur) {
		if (!strcmp(cur->file->filename, name))
			return cur->file;

		cur = cur->next;
	}
	return NULL;
}

/*
 * Frees a file list
 */
int
pkg_file_list_free(struct pkg_file_list *list)
{
	struct pkg_file_list *current;

	current = list;

	while(current) {
		struct pkg_file_list *next;

		next = current->next;
		pkg_file_free(current->file);
		if (current->pkg_object.error_str) {
			free(current->pkg_object.error_str);
			current->pkg_object.error_str = NULL;
		}
		free(current);

		current = next;
	}

	return PKG_OK;
}
