/*
 * Copyright (C) 2005, 2006 Andrew Turner, All rights reserved.
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
 * Opens a Package Database
 */
struct pkg_db*
pkg_db_open(const char *base, pkg_db_install_pkg_callback *install_pkg,
		pkg_db_is_installed_callback *is_installed,
		pkg_db_get_installed_match_callback *get_installed_match,
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
	}

	/* Check the directory exists and is a directory */
	if (stat(db->db_base, &sb) == -1) {
		pkg_db_free(db);
		return NULL;
	} else if (!S_ISDIR(sb.st_mode)) {
		pkg_db_free(db);
		return NULL;
	}

	/* Add the callbacks */
	db->pkg_install = install_pkg;
	db->pkg_is_installed = is_installed;
	db->pkg_get_installed_match = get_installed_match;
	db->pkg_get_package = get_package;

	db->data = NULL;

	return db;
}

/*
 * Install a given package to the database
 */
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

/*
 * Check to se if a package is installed
 */
int
pkg_db_is_installed(struct pkg_db *db, struct pkg *pkg)
{
	if (!db) {
		return -1;
	}

	if (!db->pkg_is_installed) {
		return -1;
	}

	return db->pkg_is_installed(db, pkg);
}

/*
 * Get a NULL terminated array of installed packages
 */
struct pkg **
pkg_db_get_installed(struct pkg_db *db)
{
	return pkg_db_get_installed_match(db, NULL, NULL);
}

/*
 * Get a NULL terminated array of installed packages that match accepts
 */
struct pkg **
pkg_db_get_installed_match(struct pkg_db *db, pkg_db_match *match, const void *data)
{
	if (!db)
		return NULL;

	if (match == NULL)
		match = pkg_match_all;

	if (db->pkg_get_installed_match)
		return db->pkg_get_installed_match(db, match, data);

	return NULL;
}

/*
 * Gets the package with the given name
 */
struct pkg *
pkg_db_get_package(struct pkg_db *db, const char *pkg_name)
{
	if (!db || !pkg_name)
		return NULL;

	if (db->pkg_get_package)
		return db->pkg_get_package(db, pkg_name);

	return NULL;
}

/*
 * Frees the database
 */
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

/*
 * Matches all packages.
 * This is here because it is used with pkg_db_get_installed_match
 */
int
pkg_match_all(struct pkg *pkg __unused, const void *data __unused)
{
	return 0;
}

/*
 * Matches all packages by origin. The origin is a string pointed to by data
 */
int
pkg_match_by_origin(struct pkg *pkg, const void *data)
{
	return strcmp(pkg_get_origin(pkg), (const char *)data);
}
