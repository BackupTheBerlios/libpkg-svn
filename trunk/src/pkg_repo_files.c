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
#include "pkg_repo.h"
#include "pkg_private.h"
#include "pkg_repo_private.h"

static struct pkg *file_repo_get_pkg(struct pkg_repo *, const char *);

/**
 * @defgroup PackageRepoFiles Local file repository
 * @ingroup PackageRepo
 *
 * @{
 */

/**
 * @brief Creates a new package repository where local files can be added
 * @return A new pkg_repo object
 */
struct pkg_repo *
pkg_repo_new_files()
{
	return pkg_repo_new(file_repo_get_pkg, NULL);
}

/**
 * @}
 */

/**
 * @defgroup PackageRepoFilesCallbacks Local file repository callbacks
 * @ingroup PackageRepoFiles
 *
 * @{
 */

/**
 * @brief Callback for pkg_repo_get_pkg()
 * @param repo The repo creates with pkg_repo_new_files()
 * @param pkg_name The file to create a package from
 * @return A pkg object or NULL
 */
static struct pkg *
file_repo_get_pkg(struct pkg_repo *repo, const char *pkg_name)
{
	struct pkg *pkg;
	FILE *fd;

	assert(repo != NULL);
	assert(pkg_name != NULL);

	/* Open the package file */
	fd = fopen(pkg_name, "r");
	if (!fd) {
		return NULL;
	}

	/* Create the package */
	/* XXX auto detect package type */
	pkg = pkg_new_freebsd_from_file(fd);
	if (!pkg) {
		fclose(fd);
		return NULL;
	}

	return pkg;
}

/**
 * @}
 */
