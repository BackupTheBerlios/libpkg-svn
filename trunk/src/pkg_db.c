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
#include "pkg_db.h"
#include "pkg_private.h"
#include "pkg_db_private.h"

/*
 * Opens the FreeBSD Package Database
 */
struct pkg_db*
pkg_db_open(const char *base, pkg_db_install_pkg_callback *install_pkg,
		pkg_db_is_installed_callback *is_installed,
		pkg_db_get_installed_callback *get_installed,
		pkg_db_get_package_callback *get_package)
{
	struct pkg_db *db;
	struct stat sb;

	db = malloc(sizeof(struct pkg_db));
	if (!db) {
		return NULL;
	}

	/* Make a relative path into an absolute path */
	if (base == NULL) {
		db->db_base = strdup("/");
	} else if (base[0] != '/') {
		char *cwd;

		cwd = getcwd(NULL, 0);
		asprintf(&db->db_base, "%s/%s", cwd, base);
		free(cwd);
	} else {
		db->db_base = strdup(base);
	}

	if (!db->db_base) {
		free(db);
		return NULL;
	} else if (stat(db->db_base, &sb) == -1) {
		free(db->db_base);
		free(db);
		return NULL;
	} else if (!S_ISDIR(sb.st_mode)) {
		free(db->db_base);
		free(db);
		return NULL;
	}
	
	db->pkg_install = install_pkg;
	db->pkg_is_installed = is_installed;
	db->pkg_get_installed = get_installed;
	db->pkg_get_package = get_package;

	db->data = NULL;

	return db;
}

int
pkg_db_install_pkg(struct pkg_db *db, struct pkg *pkg)
{
	if (!db) {
		return -1;
	}

	if (!pkg) {
		return -1;
	}

	if (!db->pkg_install) {
		return -1;
	}

	return db->pkg_install(db, pkg);
}

int
pkg_db_is_installed(struct pkg_db *db, const char *package)
{
	if (!db) {
		return -1;
	}

	if (!db->pkg_is_installed) {
		return -1;
	}

	return db->pkg_is_installed(db, package);
}

struct pkg **
pkg_db_get_installed(struct pkg_db *db)
{
	if (!db)
		return NULL;

	if (!db->pkg_get_installed)
		return NULL;

	return db->pkg_get_installed(db);
}

struct pkg *
pkg_db_get_package(struct pkg_db *db, const char *name)
{
	if (!db || !name)
		return NULL;

	if (db->pkg_get_package)
		return db->pkg_get_package(db, name);

	return NULL;
}

int
pkg_db_free(struct pkg_db *db)
{
	if (!db) {
		return -1;
	}

	if (db->db_base)
		free(db->db_base);

	free(db);

	return 0;
}
