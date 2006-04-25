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

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pkg.h"
#include "pkg_db.h"
#include "pkg_freebsd.h"
#include "pkg_private.h"
#include "pkg_db_private.h"
#include "pkg_freebsd_private.h"

#define DB_LOCATION	"/var/db/pkg"

struct pkg_install_data {
	int fake;
	struct pkg_db *db;
	char last_file[FILENAME_MAX];
	char directory[MAXPATHLEN];
};

/*
 * State transition array for the head part of a +CONTENTS file.
 * p0 is the start state, p4 and p6 are the accepting states
 */
static const int pkg_states[7][12] = {
	{ -1,  1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 }, /* p0 */
	{ -1, -1,  2, -1, -1, -1, -1, -1, -1, -1, -1, -1 }, /* p1 */
	{ -1,  3, -1,  4, -1, -1, -1, -1, -1, -1, -1, -1 }, /* p2 */
	{ -1, -1, -1,  4, -1, -1, -1, -1, -1, -1, -1, -1 }, /* p3 */
	{ -1, -1, -1, -1,  5,  6, -1, -1, -1, -1, -1, -1 }, /* p4 */
	{ -1,  4, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 }, /* p5 */
	{ -1, -1, -1, -1, -1,  6, -1, -1, -1, -1, -1, -1 }  /* p6 */
};

pkg_static int		  freebsd_install_pkg_action(struct pkg_db *,
				struct pkg *, int, int, pkg_db_action *);
pkg_static int		  freebsd_is_installed(struct pkg_db *, struct pkg *);
pkg_static struct pkg	**freebsd_get_installed_match(struct pkg_db *,
				pkg_db_match *, const void *);
pkg_static struct pkg	 *freebsd_get_package(struct pkg_db *, const char *);	

/* pkg_install callbacks */
pkg_static int	freebsd_do_chdir(struct pkg *, pkg_db_action *, void *,
				const char *);
pkg_static int	freebsd_install_file(struct pkg *, pkg_db_action *, void *,
				struct pkg_file *);
pkg_static int	freebsd_do_exec(struct pkg *, pkg_db_action *, void *,
				const char *);
pkg_static int	freebsd_register(struct pkg *, pkg_db_action *, void *,
				struct pkg_file **);
/* Internal */
pkg_static void			 freebsd_format_cmd(char *, int, const char *,
				const char *, const char *);

/**
 * @defgroup PackageDBFreebsd FreeBSD Package Database handeling
 * @ingroup PackageDB
 *
 * @{
 */

/**
 * @brief Opens the FreeBSD Package Database
 * @return A package database that will install FreeBSD packages
 */
struct pkg_db*
pkg_db_open_freebsd(const char *base)
{
	return pkg_db_open(base, freebsd_install_pkg_action,
	    freebsd_is_installed, freebsd_get_installed_match,
	    freebsd_get_package);
}

/**
 * @}
 */

/**
 * @defgroup PackageDBFreebsdCallback FreeBSD package database callbacks
 * @ingroup PackageDBFreebsd
 * @brief FreeBSD package database callback functions.
 *
 * @{
 */

/**
 * @brief Callback for pkg_db_install_pkg_action()
 * @param db The database to install to
 * @param pkg The package to install
 * @param pkg_action A function to call when an action takes place
 * @param fake Should we actually install the package or just report what would have happened
 * @todo Run mtree
 * @todo Register the reverse dependencies
 * @bug When the install fails part way through remove some files are left.
 *     Remove these.
 * @return 0 on success, -1 on error
 */
static int
freebsd_install_pkg_action(struct pkg_db *db, struct pkg *pkg, int reg,
    int fake, pkg_db_action *pkg_action)
{
	struct pkg_install_data install_data;
	char cwd[MAXPATHLEN];

	assert(db != NULL);
	assert(pkg != NULL);
	assert(pkg_action != NULL);

	if (getwd(cwd) == NULL)
		return -1;

	pkg_action(PKG_DB_PACKAGE, "Package name is %s", pkg_get_name(pkg));

	/* Run +REQUIRE */
	pkg_action(PKG_DB_INFO, "Running ... for %s..",
	    pkg_get_name(pkg));

	if (!fake) {
		/** @todo Check if the force flag is set */
		if (pkg_run_script(pkg, pkg_script_require) != 0) {
			return -1;
		}
	}

	/* Run Pre-install */
	pkg_action(PKG_DB_INFO, "Running pre-install for %s..",
	    pkg_get_name(pkg));

	if (!fake)
		pkg_run_script(pkg, pkg_script_pre);

	/* Do the Install */
	install_data.db = db;
	install_data.fake = fake;
	install_data.last_file[0] = '\0';
	install_data.directory[0] = '\0';
	pkg_install(pkg, reg, pkg_action, &install_data, freebsd_do_chdir,
	    freebsd_install_file, freebsd_do_exec, freebsd_register);

	/* Extract the +MTREE */
	pkg_action(PKG_DB_INFO, "Running mtree for %s..", pkg_get_name(pkg));

	if (!fake)
		pkg_run_script(pkg, pkg_script_mtree);

	/* Run post-install */
	pkg_action(PKG_DB_INFO, "Running post-install for %s..",
	    pkg_get_name(pkg));

	if (!fake)
		pkg_run_script(pkg, pkg_script_post);

	/** @todo Display contents of @display */

	chdir(cwd);
	return 0;
}

/**
 * @brief Callback for pkg_db_is_installed()
 * @returns 0 on of the package is installed, -1 otherwise
 */
static int
freebsd_is_installed(struct pkg_db *db, struct pkg *pkg)
{
	struct stat sb;
	char *dir;
	struct pkg **pkgs;
	int is_installed;

	assert(db != NULL);
	assert(pkg != NULL);

	asprintf(&dir, "%s" DB_LOCATION "/%s", db->db_base, pkg_get_name(pkg));
	if (!dir) {
		return -1;
	}

	is_installed = -1;

	/* Does the package repo directory exist */
	if (stat(dir, &sb) == 0 && S_ISDIR(sb.st_mode) != 0) {
		/* The passed package is installed */
		free(dir);
		return 0;
	}
	free(dir);

	/* Does the package have an origin and if so is that origin installed */
	if (pkg_get_origin(pkg) != NULL) {
		pkgs = freebsd_get_installed_match(db, pkg_match_by_origin,
		    (const void *)pkg_get_origin(pkg));
		if (pkgs[0] != NULL)
			is_installed = 0;
		pkg_list_free(pkgs);
	}
	return is_installed;
}

/**
 * @brief Callback for pkg_db_get_installed_match()
 * @return A null-terminated array of packages that when passed to the match
 *     function it returns 0. NULL on error
 */
static struct pkg **
freebsd_get_installed_match(struct pkg_db *db, pkg_db_match *match, const void *data)
{
	DIR *d;
	struct dirent *de;
	char *dir;
	struct pkg **packages;
	unsigned int packages_size;
	unsigned int packages_pos;
	
	assert(db != NULL);
	assert(db->db_base != NULL);

	asprintf(&dir, "%s" DB_LOCATION, db->db_base);
	if (!dir)
		return NULL;
	d = opendir(dir);
	free(dir);
	if (!d)
		return NULL;

	packages_size = sizeof(char *);
	packages = malloc(packages_size);
	if (!packages) {
		closedir(d);
		return NULL;
	}
	packages[0] = NULL;
	packages_pos = 0;
	while((de = readdir(d)) != NULL) {
		struct pkg *pkg;

		if (de->d_name[0] == '.' || de->d_type != DT_DIR)
			continue;
		asprintf(&dir, "%s" DB_LOCATION "/%s",
		    db->db_base, de->d_name);

		pkg = pkg_new_freebsd_installed(de->d_name, dir);
		if (match(pkg, data) == 0) {
			packages_size += sizeof(char *);
			packages = realloc(packages, packages_size);
			packages[packages_pos] = pkg;
			packages_pos++;
			packages[packages_pos] = NULL;
		} else
			pkg_free(pkg);
		free(dir);
	}
	closedir(d);
	return packages;
}

/**
 * @brief Callback for pkg_db_get_package()
 * @return The named package or NULL
 */
static struct pkg *
freebsd_get_package(struct pkg_db *db, const char *pkg_name)
{
	char dir[MAXPATHLEN + 1];

	snprintf(dir, MAXPATHLEN, "%s/var/db/pkg/%s", db->db_base, pkg_name);
	return pkg_new_freebsd_installed(pkg_name, dir);
}

/**
 * @}
 */

/**
 * @defgroup PackageDBFreebsdInternal FreeBSD package database internal functions
 * @ingroup PackageDBFreebsd
 * @brief Functions to help the FreeBSD package database callbacks
 *
 * @{
 */

static int
freebsd_do_chdir(struct pkg *pkg, pkg_db_action *pkg_action, void *data,
		const char *dir)
{
	struct pkg_install_data *install_data;
	struct pkg_db *db;

	assert(pkg != NULL);
	assert(pkg_action != NULL);
	assert(data != NULL);
	assert(dir != NULL);
	
	install_data = data;
	db = install_data->db;
	assert(db != NULL);
	assert(db->db_base != NULL);

	if (strcmp(dir, ".") == 0) {
		snprintf(install_data->directory, MAXPATHLEN,
		    "%s" DB_LOCATION "/%s", db->db_base, pkg_get_name(pkg));
	} else {
		snprintf(install_data->directory, MAXPATHLEN, "%s/%s",
		    db->db_base, dir);
	}

	pkg_action(PKG_DB_PACKAGE, "CWD to %s", install_data->directory);
	if (!install_data->fake) {
		pkg_dir_build(install_data->directory);
		return chdir(install_data->directory);
	}

	return 0;
}

static int
freebsd_install_file(struct pkg *pkg, pkg_db_action *pkg_action __unused,
		void *data, struct pkg_file *file)
{
	struct pkg_install_data *install_data;

	assert(pkg != NULL);
	assert(data != NULL);
	assert(file != NULL);

	install_data = data;

	snprintf(install_data->last_file, FILENAME_MAX, "%s",
	    pkg_file_get_name(file));

	pkg_action(PKG_DB_PACKAGE, "%s/%s", install_data->directory,
	    pkg_file_get_name(file));
	if (!install_data->fake)
		return pkg_file_write(file);
	return 0;
}

static int
freebsd_do_exec(struct pkg *pkg, pkg_db_action *pkg_action, void *data,
		const char *cmd)
{
	char the_cmd[FILENAME_MAX];
	struct pkg_install_data *install_data;

	assert(pkg != NULL);
	assert(pkg_action != NULL);
	assert(data != NULL);
	assert(cmd != NULL);

	install_data = data;

	freebsd_format_cmd(the_cmd, FILENAME_MAX, cmd, install_data->directory,
	    install_data->last_file);

	pkg_action(PKG_DB_PACKAGE, "execute '%s'", the_cmd);
	if (!install_data->fake)
		return pkg_exec(the_cmd);

	return 0;
}

static int
freebsd_register(struct pkg *pkg, pkg_db_action *pkg_action, void *data,
		struct pkg_file **control)
{
	unsigned int pos;
	struct pkg_install_data *install_data;
	struct pkg_db *db;
	struct pkg **deps;

	assert(pkg != NULL);
	assert(pkg_action != NULL);
	assert(data != NULL);
	assert(control != NULL);

	install_data = data;
	assert(install_data->db);
	db = install_data->db;

	pkg_action(PKG_DB_INFO,
	    "Attempting to record package into " DB_LOCATION "/%s..",
	    pkg_get_name(pkg));
	for (pos = 0; control[pos] != NULL; pos++) {
		freebsd_install_file(pkg, pkg_action, data, control[pos]);
	}

	/* Register reverse dependency */
	deps = pkg_get_dependencies(pkg);
	for (pos=0; deps[pos] != NULL; pos++) {
		char required_by[FILENAME_MAX];
		const char *name;
		FILE *fd;

		snprintf(required_by, FILENAME_MAX, "%s" DB_LOCATION
		    "/%s/+REQUIRED_BY", db->db_base, pkg_get_name(deps[pos]));

		/** @todo Make pkg_file work to properly to create the file */
		fd = fopen(required_by, "a");
		name = pkg_get_name(pkg);
		fwrite(name, strlen(name), 1, fd);
		fwrite("\n", 1, 1, fd);
		fclose(fd);
	}
	pkg_list_free(deps);
	
	pkg_action(PKG_DB_INFO, "Package %s registered in %s" DB_LOCATION "/%s",
	    pkg_get_name(pkg), db->db_base, pkg_get_name(pkg));
	return -1;
}

#ifdef DEAD
/**
 * @brief Checks the start of a contents file
 * @return The number of lines to skip to get to the first file or -1 on error
 */
static int
freebsd_check_contents(struct pkg_db *db, struct pkg_freebsd_contents *contents,
		int fake)
{
	unsigned int i;
	int state;

	assert(db != NULL);
	assert(contents != NULL);

	state = 0;

	if (contents->lines[0].line_type != PKG_LINE_COMMENT) {
		return -1;
	} else if (strcmp(contents->lines[0].data, "PKG_FORMAT_REVISION:1.1")) {
		return -1;
	}

	/* Run through a NFA to check the head */
	for (i = 0; i < contents->line_count; i++) {
		int new_state = -2;
		new_state = pkg_states[state][contents->lines[i].line_type];
		if (new_state == -1) {
			break;
		}
		/* If the current line is @chdir... do it */
		if (contents->lines[i].line_type == PKG_LINE_CWD) {
			if (freebsd_do_cwd(db, NULL, contents->lines[i].data,
			    fake) != 0) {
				return -1;
			}
		}
		state = new_state;
	}
	if (state != 4 && state != 6) {
		return -1;
	}
	return i;
}
#endif

/**
 * @brief Creates a string containing the command to run using printf
 * like substitutions
 *
 * @verbatim
 * Using fmt, replace all instances of:
 *
 * %F   With the parameter "name"
 * %D   With the parameter "dir"
 * %B   Return the directory part ("base") of %D/%F
 * %f   Return the filename part of %D/%F
 * @endverbatim
 *
 * @bug Does not check for overflow - caution!
 */
static void
freebsd_format_cmd(char *buf, int max, const char *fmt, const char *dir,
	const char *name)
{
	char *cp, scratch[FILENAME_MAX * 2];
	int l;

	assert(buf != NULL);
	assert(max >= 0);
	assert(fmt != NULL);
	assert(dir != NULL);
	assert(name != NULL);

	while (*fmt && max > 0) {
		if (*fmt == '%') {
			switch (*++fmt) {
				case 'F':
					strncpy(buf, name, max);
					l = strlen(name);
					buf += l, max -= l;
					break;

				case 'D':
					strncpy(buf, dir, max);
					l = strlen(dir);
					buf += l, max -= l;
					break;

				case 'B':
					snprintf(scratch, FILENAME_MAX * 2,
					    "%s/%s", dir, name);
					cp = &scratch[strlen(scratch) - 1];
					while (cp != scratch && *cp != '/')
						--cp;
					*cp = '\0';
					strncpy(buf, scratch, max);
					l = strlen(scratch);
					buf += l, max -= l;
					break;

				case 'f':
					snprintf(scratch, FILENAME_MAX * 2,
					    "%s/%s", dir, name);
					cp = &scratch[strlen(scratch) - 1];
					while (cp != scratch && *(cp - 1) != '/')
						--cp;
					strncpy(buf, cp, max);
					l = strlen(cp);
					buf += l, max -= l;
					break;

				default:
					*buf++ = *fmt;
					--max;
					break;
			}
			++fmt;
		} else {
			*buf++ = *fmt++;
			--max;
		}
	}
	*buf = '\0';
}

/**
 * @}
 */
