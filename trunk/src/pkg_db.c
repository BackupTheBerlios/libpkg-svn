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
#include <string.h>

#include "pkg.h"
#include "pkg_private.h"

/*
 * Opens the FreeBSD Package Database
 */
struct pkg_db*
pkg_db_open(const char *base, pkg_db_install_pkg_callback *install_pkg,
		pkg_db_is_installed_callback *is_installed)
{
	struct pkg_db *db;

	db = malloc(sizeof(struct pkg_db));
	if (!db) {
		pkg_error_set(&pkg_null, "Out of Memory");
		return NULL;
	}

	/* Make a relative path into an absolute path */
	if (base[0] != '/') {
		char *cwd;

		cwd = getcwd(NULL, 0);
		asprintf(&db->db_base, "%s/%s", cwd, base);
		free(cwd);
	} else {
		db->db_base = strdup(base);
	}

	if (!db->db_base) {
		free(db);
		pkg_error_set(&pkg_null, "Out of Memory");
		return NULL;
	}

	db->pkg_install = install_pkg;
	db->pkg_is_installed = is_installed;

	db->pkg_object.data = NULL;
	db->pkg_object.error_str = NULL;
	db->pkg_object.free = NULL;

	return db;
}

int
pkg_db_install_pkg(struct pkg_db *db, struct pkg *pkg)
{
	if (!db) {
		pkg_error_set(&pkg_null, "No package database specified");
		return PKG_FAIL;
	}

	if (!pkg) {
		pkg_error_set((struct pkg_object *)db, "No package specified");
		return PKG_FAIL;
	}

	if (!db->pkg_install) {
		pkg_error_set((struct pkg_object *)db, "No package install callback");
		return PKG_NOTSUP;
	}
	if (db->pkg_object.error_str) {
		free(db->pkg_object.error_str);
		db->pkg_object.error_str = NULL;
	}

	return db->pkg_install(db, pkg);
}

int
pkg_db_is_installed(struct pkg_db *db, const char *package)
{
	if (!db) {
		pkg_error_set(&pkg_null, "No package database specified");
		return PKG_FAIL;
	}

	if (!db->pkg_is_installed) {
		pkg_error_set((struct pkg_object *)db, "No is installed callback");
		return PKG_NOTSUP;
	}

	return db->pkg_is_installed(db, package);
}

int
pkg_db_free(struct pkg_db *db)
{
	if (!db) {
		pkg_error_set(&pkg_null, "No package database specified");
		return PKG_FAIL;
	}

	if (db->db_base)
		free(db->db_base);

	free(db);

	return PKG_OK;
}
