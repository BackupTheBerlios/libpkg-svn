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

#include <sys/stat.h>

#include <err.h>
#include <errno.h>
#include <md5.h>
#include <stdlib.h>
#include <string.h>

#include "pkg.h"
#include "pkg_private.h"

/*
 * This is a simplified version of `mkdir -p path'
 * Is has been simplified to just take in a path to create
 */
/* Based off src/bin/mkdir/mkdir.c 1.32 */
int
pkg_dir_build(const char *path)
{
	struct stat sb;
	int last, retval;
	char *str, *p;

	str = strdup(path);
	if (!str) {
		pkg_error_set(&pkg_null, "Out of Memory");
		return PKG_FAIL;
	}
	p = str;
	retval = PKG_OK;
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
		if (mkdir(str, S_IRWXU | S_IRWXG | S_IRWXO) < 0) {
			if (errno == EEXIST || errno == EISDIR) {
				if (stat(str, &sb) < 0) {
					retval = PKG_FAIL;
					pkg_error_set(&pkg_null,
					    "Could not stat %s", str);
					break;
				} else if (!S_ISDIR(sb.st_mode)) {
					if (last)
						errno = EEXIST;
					else
						errno = ENOTDIR;
					retval = PKG_FAIL;
					pkg_error_set(&pkg_null,
					    "%s is not a directory", str);
					break;
				}
			} else {
				retval = PKG_FAIL;
				pkg_error_set(&pkg_null, "Could not create %s",
				    str);
				break;
			}
		}
		if (!last)
		    *p = '/';
	}
	free(str);
	return (retval);
}

/* Checks a file against a given md5 checksum */
int
pkg_checksum_md5(struct pkg_file *file, char *chk_sum)
{
	char sum[33];

	if (!file) {
		pkg_error_set(&pkg_null, "No file specified");
		return PKG_FAIL;
	}

	if (!sum) {
		pkg_error_set((struct pkg_object *)file, "No checksum specified");
		return PKG_FAIL;
	}

	/* Perform a checksum on the file to install */
	MD5Data(file->contents, file->len, sum);
	if (strcmp(sum, chk_sum)) {
		pkg_error_set((struct pkg_object *)file,
		    "File checksum incorrect");
		return PKG_FAIL;
	}
	return PKG_OK;
}
