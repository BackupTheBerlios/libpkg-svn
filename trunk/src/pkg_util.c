/*-
 * Copyright (c) 1983, 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/stat.h>

#include <assert.h>
#include <err.h>
#include <errno.h>
#include <md5.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pkg.h"
#include "pkg_private.h"

static int	 pkg_cached_readfn(void *, char *, int);
static fpos_t	 pkg_cached_seekfn(void *, fpos_t, int);
static int	 pkg_cached_closefn(void *);

/**
 * @defgroup PackageUtil Miscellaneous utilities
 *
 * @{
 */

/**
 * @brief A simplified version of `mkdir -p path'
 * @return 0 on success, -1 on error
 */
/* Based off src/bin/mkdir/mkdir.c 1.32 */
int
pkg_dir_build(const char *path, mode_t mode)
{
	struct stat sb;
	int last, retval;
	char *str, *p;

	str = strdup(path);
	if (!str) {
		return -1;
	}
	p = str;
	retval = 0;
	if (p[0] == '/')		/* Skip leading '/'. */
		++p;
	for (last = 0; !last ; ++p) {
		if (p[0] == '\0')
			last = 1;
		else if (p[0] != '/')
			continue;
		*p = '\0';
		if (!last && p[1] == '\0')
			last = 1;
		if (mkdir(str,
		    (mode == 0) ? (S_IRWXU | S_IRWXG | S_IRWXO) : mode) < 0) {
			if (errno == EEXIST || errno == EISDIR) {
				if (stat(str, &sb) < 0) {
					retval = -1;
					break;
				} else if (!S_ISDIR(sb.st_mode)) {
					if (last)
						errno = EEXIST;
					else
						errno = ENOTDIR;
					retval = -1;
					break;
				}
			} else {
				retval = -1;
				break;
			}
		}
		if (!last)
		    *p = '/';
	}
	free(str);
	return (retval);
}

/**
 * @brief Creates an absolute pathname from a relative name
 *
 * Creates an absolute path, additionally removing all .'s, ..'s, and extraneous
 * /'s, as realpath() would, but without resolving symlinks.
 * @return The absolute pathname
 * @return NULL on error
 */
char *
pkg_abspath(const char *pathname)
{
	char *tmp, *tmp1, *resolved_path;
	char *cwd = NULL;
	int len;

	assert(pathname != NULL);

	if (pathname[0] != '/') {
		cwd = getcwd(NULL, MAXPATHLEN);
		asprintf(&resolved_path, "%s/%s/", cwd, pathname);
		free(cwd);    
	} else
		asprintf(&resolved_path, "%s/", pathname);

	if (resolved_path == NULL)
		return NULL;

	while ((tmp = strstr(resolved_path, "//")) != NULL)
		strcpy(tmp, tmp + 1);
 
	while ((tmp = strstr(resolved_path, "/./")) != NULL)
		strcpy(tmp, tmp + 2);
 
	while ((tmp = strstr(resolved_path, "/../")) != NULL) {
		*tmp = '\0';
		if ((tmp1 = strrchr(resolved_path, '/')) == NULL)
			tmp1 = resolved_path;
		strcpy(tmp1, tmp + 3);
	}

	len = strlen(resolved_path);
	if (len > 1 && resolved_path[len - 1] == '/')
		resolved_path[len - 1] = '\0';

	return resolved_path;
}

/**
 * @brief Executes a program
 * 
 * It will use fmt as the format to generate the execv string.
 * @return the return value from the child process
 */
int
pkg_exec(const char *fmt, ...)
{
	va_list ap;
	char *str;
	int ret;

	va_start(ap, fmt);
	vasprintf(&str, fmt, ap);
	va_end(ap);

	ret = system(str);
	free(str);

	return ret;
}

/**
 * @}
 */

/**
 * @defgroup PackageUtilCachedFileInternal File cacheing handler callbacks
 * @ingroup PackageUtilCachedFile
 * These functions are callbacks for a FILE pointer designed
 * to cache the output of a another FILE pointer.
 * This is useful when downloading with libfetch to not have
 * to download the file again.
 *
 * @{
 */

struct cached_read {
	FILE *fd;
	FILE *cache;
};

/**
 * @brief Reads from a file, caches the data and copies it to buf
 * @param c A cached_read object
 * @param buf The buffer to save the data to
 * @param len The ammount of data to read
 *
 * This is a callback to fread
 * @return The amount of data read or -1
 */
static int
pkg_cached_readfn(void *c, char *buf, int len)
{
	struct cached_read *cr;
	int ret, left, count;
	char *b;

	cr = c;

	/* Check we are not at the end of the input file */
	if (feof(cr->fd))
		return 0;

	ret = fread(buf, 1, len, cr->fd);

	if (ret == -1)
		return ret;

	/* Write the entire buffer to the file */
	left = ret;
	b = buf;
	while (left > 0) {
		count = fwrite(b, 1, left, cr->cache);
		b += count;
		left -= count;
	}
	return ret;
}

/**
 * @brief Seeks to a given point in a cached file
 * @param c A cached_file object
 * @param pos The position to move to
 * @param whence Where to make the move relative to
 *
 * This is a callback for fseek
 * @return -1 on error
 * @return 0 on success
 */
static fpos_t
pkg_cached_seekfn(void *c, fpos_t pos, int whence)
{
	struct cached_read *cr;

	cr  = c;
	return fseek(cr->fd, pos, whence);
}

/**
 * @brief Closes a cached file
 * @param c A cached_file object
 * @return 0
 */
static int
pkg_cached_closefn(void *c)
{
	struct cached_read *cr;

	cr  = c;
	fclose(cr->fd);
	fclose(cr->cache);
	free(cr);
	return 0;
}

/**
 * @}
 */

/**
 * @defgroup PackageUtilCachedFile File cacheing handler creation
 * @ingroup PackageUtil
 *
 * @{
 */

/**
 * @brief Creates a new cached FILE pointer
 * @param fd The file to cache
 * @param file The location of the file cache
 * @return A FILE pointer or NULL
 */
FILE *
pkg_cached_file(FILE *fd, const char *file)
{
	struct cached_read *cr;
	
	cr = malloc(sizeof(struct cached_read));
	if (cr == NULL)
		return NULL;

	cr->fd = fd;
	/* Create the file and write to it when caching */
	cr->cache = fopen(file, "w");
	return funopen(cr, pkg_cached_readfn, NULL, pkg_cached_seekfn,
	    pkg_cached_closefn);
}

/**
 * @}
 */
