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

#include <stdlib.h>

#include "pkg.h"
#include "pkg_repo.h"
#include "pkg_private.h"
#include "pkg_repo_private.h"

/*
 * Returns a new package repo
 */
struct pkg_repo *
pkg_repo_new(pkg_repo_get_pkg_callback *pkg_get,
             pkg_repo_free_callback *pfree)
{
	struct pkg_repo *repo;

	repo = malloc(sizeof(struct pkg_repo));
	if (!repo) {
		return NULL;
	}

	repo->pkg_get = pkg_get;
	repo->pkg_free = pfree;

	repo->pkg_object.data = NULL;
	repo->pkg_object.free = NULL;

	return repo;
}

struct pkg *
pkg_repo_get_pkg(struct pkg_repo *repo, const char *pkg_name)
{
	if (!repo) {
		return NULL;
	}

	if (!pkg_name) {
		return NULL;
	}

	if (!repo->pkg_get) {
		return NULL;
	}

	return repo->pkg_get(repo, pkg_name);
}

/*
 * Frees the struct pkg_repo
 */
int
pkg_repo_free(struct pkg_repo *repo)
{
	if (!repo) {
		return PKG_FAIL;
	}

	if (repo->pkg_free)
		repo->pkg_free(repo);

	free(repo);

	return PKG_OK;
}
