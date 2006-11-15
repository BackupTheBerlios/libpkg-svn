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

#ifndef __LIBPKG_PKG_DB_H__
#define __LIBPKG_PKG_DB_H__

/*
 * A place to install packages to and uninstall packages from
 */
struct pkg_db;

/*
 * Definition of the match function to be passed to pkg_get_installed_match.
 * It will take a package and some user specified data in.
 * Returns 0 if the package matches or -1 otherwise.
 */
typedef		  int pkg_db_match(struct pkg *, const void *);

enum pkg_action_level {
	PKG_DB_ERROR,	/* Error messages */
	PKG_DB_INFO,	/* General info */
	PKG_DB_PACKAGE,	/* Specific info from the package */
};
typedef		  int pkg_db_action(enum pkg_action_level, int, const char *,				...);

struct pkg_db	 *pkg_db_open_freebsd(const char *);
int		  pkg_db_install_pkg_action(struct pkg_db *, struct pkg *,
			const char *, int, int, int, pkg_db_action *);
int		  pkg_db_is_installed(struct pkg_db *, struct pkg *);
struct pkg	**pkg_db_get_installed(struct pkg_db *);
struct pkg	**pkg_db_get_installed_match(struct pkg_db *, pkg_db_match *,
			const void *);
struct pkg	**pkg_db_get_installed_match_count(struct pkg_db *,
			pkg_db_match *, unsigned int, const void *);
struct pkg	 *pkg_db_get_package(struct pkg_db *, const char *);
int		  pkg_db_delete_package_action(struct pkg_db *, struct pkg *,
			int, int, int, pkg_db_action *);
int		  pkg_db_free(struct pkg_db *);

/* Helper functions that use an internal callback for pkg_db_get_installed_match() */
typedef enum {
	PKG_DB_MATCH_ALL,
	PKG_DB_MATCH_EXACT,
	PKG_DB_MATCH_GLOB,
	PKG_DB_MATCH_EREGEX,
	PKG_DB_MATCH_REGEX
} pkg_db_match_t;

struct pkg	**pkg_db_match_by_type(struct pkg_db *, const char **,
			pkg_db_match_t);
struct pkg	**pkg_db_match_all(struct pkg_db *, const char **, int);
struct pkg	**pkg_db_match_name(struct pkg_db *, const char **, int);
struct pkg	**pkg_db_match_regex(struct pkg_db *, const char **, int);
struct pkg	**pkg_db_match_glob(struct pkg_db *, const char **, int);

int		  pkg_action_null(enum pkg_action_level, int, const char *,...);

/* Functions to be passed to pkg_db_get_installed_match() */
int		  pkg_match_all(struct pkg *, const void *);
int		  pkg_match_by_origin(struct pkg *, const void *);
int		  pkg_match_by_file(struct pkg *, const void *);
int		  pkg_db_freebsd_match_rdep(struct pkg *, const void *);

#endif /* __LIBPKG_PKG_DB_H__ */
