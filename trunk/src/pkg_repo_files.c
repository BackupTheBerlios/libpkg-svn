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

#include <sys/stat.h>

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "pkg.h"
#include "pkg_private.h"

static struct pkg *file_get_pkg(struct pkg_repo *, const char *);

/*
 * A repo where local files can be added to be installed
 */
struct pkg_repo *
pkg_repo_new_files()
{
	return pkg_repo_new(file_get_pkg, NULL);
}

static struct pkg *
file_get_pkg(struct pkg_repo *repo, const char *pkg_name)
{
	struct pkg *pkg;
	FILE *fd;

	assert(repo != NULL);
	assert(pkg_name != NULL);

	/* Open the package file */
	fd = fopen(pkg_name, "r");
	if (!fd) {
		pkg_error_set(&pkg_null, "Could not open %s", pkg_name);
		return NULL;
	}

	/* Create the package */
	/* XXX auto detect package type */
	pkg = pkg_new_freebsd(fd);
	if (!pkg) {
		char *str;
		fclose(fd);
		str = pkg_error_string(&pkg_null);
		pkg_error_set((struct pkg_object *)repo, str);
		return NULL;
	}

	return pkg;
}
