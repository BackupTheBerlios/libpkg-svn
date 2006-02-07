/*
 * Copyright (C) 2006, Andrew Turner, All rights reserved.
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

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "pkg.h"
#include "pkg_repo.h"
#include "pkg_private.h"
#include "pkg_repo_private.h"

static struct pkg *file_get_pkg(struct pkg_repo *, const char *);

/**
 * The contructor.
 * Creates a package repo where the packages are stored on a local filesystem.
 * If the package name contains a '/' it will assume it is a path and attempt
 * to open the package from there.
 * Otherwise it will search in '.' then /usr/ports/distfiles.
 * @return 
 */
struct pkg_repo *
pkg_repo_new_local_freebsd()
{
	return pkg_repo_new(file_get_pkg, NULL);
}

static struct pkg *
file_get_pkg(struct pkg_repo *repo, const char *pkg_name)
{
	char dir[MAXPATHLEN + 1];
	struct pkg *pkg;
	FILE *fd;
	assert(repo != NULL);
	assert(pkg_name != NULL);

	/* XXX Check the file is a package file after every attempt to open it */
	snprintf(dir, MAXPATHLEN + 1,"%s.tbz", pkg_name);
	fd = fopen(dir, "r");
	if (!fd) {
		snprintf(dir, MAXPATHLEN + 1,
		    "/usr/ports/packages/All/%s.tbz", pkg_name);
		fd = fopen(dir, "r");
	}
	if (!fd) {
		fd = fopen(pkg_name, "r");
	}
	if (!fd) {
		snprintf(dir, MAXPATHLEN + 1,
		    "/usr/ports/packages/All/%s", pkg_name);
		fd = fopen(dir, "r");
	}
	if (!fd)
		return NULL;

	pkg = pkg_new_freebsd_from_file(fd);
	if (!pkg) {
		fclose(fd);
		return NULL;
	}

	return pkg;
}
