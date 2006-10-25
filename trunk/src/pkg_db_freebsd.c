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

static int		  freebsd_install_pkg_action(struct pkg_db *,
				struct pkg *, const char *, int, int, int,
				pkg_db_action *);
static int		  freebsd_is_installed(struct pkg_db *, struct pkg *);
static struct pkg	**freebsd_get_installed_match(struct pkg_db *,
				pkg_db_match *, unsigned int, const void *);
static struct pkg	 *freebsd_get_package(struct pkg_db *, const char *);
static int		  freebsd_deinstall_pkg(struct pkg_db *, struct pkg *,
				int, int, pkg_db_action *);

/* pkg_(install|deinstall) callbacks */
static int	freebsd_do_chdir(struct pkg *, pkg_db_action *, void *,
				const char *);
static int	freebsd_install_file(struct pkg *, pkg_db_action *, void *,
				struct pkgfile *);
static int	freebsd_deinstall_file(struct pkg *, pkg_db_action *, void *,
				struct pkgfile *);
static int	freebsd_do_exec(struct pkg *, pkg_db_action *, void *,
				const char *);
static int	freebsd_register(struct pkg *, pkg_db_action *, void *,
				struct pkgfile **);
static int	freebsd_deregister(struct pkg *, pkg_db_action *, void *,
				struct pkgfile **);
/* Internal */
static void			 freebsd_format_cmd(char *, int, const char *,
				const char *, const char *);

/**
 * @defgroup PackageDBFreebsd FreeBSD Package Database handling
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
	    freebsd_get_package, freebsd_deinstall_pkg);
}

/**
 * @}
 */

/**
 * @defgroup PackageDBFreebsdMatch FreeBSD package matching functions
 * @ingroup PackageDBFreebsd
 * @brief FreeBSD specific functions to be passed to
 * 	pkg_db_get_installed_match()
 *
 * @{
 */

/**
 * @brief A function to match all FreeBSD package that a given pakage depend on
 * @param pkg The package to test
 * @param pkg_name The name of the package to find the dependencies of
 * @return 0 if pkg_name depends on pkg
 * @return Non zero otherwise
 */
int
pkg_db_freebsd_match_rdep(struct pkg *pkg, const void *pkg_name)
{
	struct pkgfile *file;

	assert(pkg != NULL);
	assert(pkg_name != NULL);

	file = pkg_get_control_file(pkg, "+REQUIRED_BY");
	if (file == NULL)
		return 1;

	if (pkgfile_find_line(file, (const char *)pkg_name) != NULL)
		return 0;

	printf("--> %s\n", pkg_get_name(pkg));

	return 1;
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
 * @param prefix If non-NULL this will override the packages prefix
 * @param reg If true register the package in the database
 * @param scripts If true will run the packafes scripts
 * @param fake Should we actually install the package or
 *     just report what would have happened
 * @param pkg_action A function to call when an action takes place
 * @bug When the install fails part way through remove some files are left.
 *     Remove these.
 * @return 0 on success, -1 on error
 */
static int
freebsd_install_pkg_action(struct pkg_db *db, struct pkg *pkg,
    const char *prefix, int reg, int scripts, int fake,
    pkg_db_action *pkg_action)
{
	struct pkg_install_data install_data;
	char cwd[MAXPATHLEN];

	assert(db != NULL);
	assert(pkg != NULL);
	assert(pkg_action != NULL);

	if (getwd(cwd) == NULL)
		return -1;

	/* Set the package environment */
	if (prefix == NULL) {
		const char *pkg_prefix = pkg_get_prefix(pkg);
		if (pkg_prefix == NULL)
			setenv("PKG_PREFIX", "/usr/local", 1);
		else
			setenv("PKG_PREFIX", pkg_prefix, 1);
	} else
		setenv("PKG_PREFIX", prefix, 1);

	pkg_action(PKG_DB_PACKAGE, "Package name is %s", pkg_get_name(pkg));

	/* Run +REQUIRE */
	pkg_action(PKG_DB_INFO, "Running ... for %s..",
	    pkg_get_name(pkg));

	if (!fake) {
		/** @todo Check if the force flag is set */
		if (pkg_run_script(pkg, prefix, pkg_script_require) != 0) {
			return -1;
		}
	}

	/* Run Pre-install */
	pkg_action(PKG_DB_INFO, "Running pre-install for %s..",
	    pkg_get_name(pkg));

	if (!fake && scripts)
		pkg_run_script(pkg, prefix, pkg_script_pre);

	/* Do the Install */
	install_data.db = db;
	install_data.fake = fake;
	install_data.last_file[0] = '\0';
	install_data.directory[0] = '\0';
	if (pkg_install(pkg, prefix, reg, pkg_action, &install_data,
	    freebsd_do_chdir, freebsd_install_file, freebsd_do_exec,
	    freebsd_register) != 0) {
		return -1;
	}

	/* Extract the +MTREE */
	pkg_action(PKG_DB_INFO, "Running mtree for %s..", pkg_get_name(pkg));

	if (!fake)
		pkg_run_script(pkg, prefix, pkg_script_mtree);

	/* Run post-install */
	pkg_action(PKG_DB_INFO, "Running post-install for %s..",
	    pkg_get_name(pkg));

	if (!fake && scripts)
		pkg_run_script(pkg, prefix, pkg_script_post);

	/** @todo Display contents of \@display */

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
	pkg_remove_extra_slashes(dir);

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
		    0, (const void *)pkg_get_origin(pkg));
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
freebsd_get_installed_match(struct pkg_db *db, pkg_db_match *match,
    unsigned int count, const void *data)
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
	pkg_remove_extra_slashes(dir);
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
		pkg_remove_extra_slashes(dir);

		pkg = pkg_new_freebsd_installed(de->d_name, dir);
		if (match(pkg, data) == 0) {
			packages_size += sizeof(char *);
			packages = realloc(packages, packages_size);
			packages[packages_pos] = pkg;
			packages_pos++;
			packages[packages_pos] = NULL;

			/* Stop after count packages */
			if (count != 0 && packages_pos == count + 1)
				break;
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
	pkg_remove_extra_slashes(dir);
	return pkg_new_freebsd_installed(pkg_name, dir);
}

/**
 * @brief Callback for pkg_db_deinstall_package()
 * @return  0 on success
 * @return -1 on fatal error
 */
static int
freebsd_deinstall_pkg(struct pkg_db *db, struct pkg *the_pkg, int scripts __unused, int fake,
	pkg_db_action *pkg_action)
{
	struct pkg_install_data deinstall_data;
	struct pkg *real_pkg;
	struct pkg **deps;

	assert(db != NULL);
	assert(the_pkg != NULL);

	/* Get the real package. The one supplyed may be an empty one */
	/** @todo Check if the package suplyed is a valid package or not */
	real_pkg = freebsd_get_package(db, pkg_get_name(the_pkg));
	/* Check if the package is installed */
	if (real_pkg == NULL) {
		pkg_action(PKG_DB_INFO, "No such package '%s' installed",
		    pkg_get_name(the_pkg));
		return -1;
	}

	/** @todo Check if package is dependended on */
	deps = pkg_get_reverse_dependencies(real_pkg);
	if (deps == NULL) {
		return -1;
	} else if (deps[0] != NULL) {
		unsigned int pos, buf_size, buf_used;
		char *buf;
		/* XXX */
		buf_used = 0;
		buf_size = 1024;
		buf = malloc(buf_size);
		if (buf == NULL) {
			pkg_action(PKG_DB_INFO,
			    "package '%s' is required by other packages and "
			    "may not be deinstalled however an error occured "
			    "while retrieving the list of packages",
			    pkg_get_name(real_pkg));
			return -1;
		}
		/* Load the names of the packages into a buffer */
		for (pos = 0; deps[pos] != NULL; pos++) {
			size_t len;

			len = strlen(pkg_get_name(deps[pos]));
			if (buf_used + len >= buf_size) {
				buf_size += 1024;
				buf = realloc(buf, buf_size);
			}
			strlcat(buf, pkg_get_name(deps[pos]), buf_size);
			strlcat(buf, "\n", buf_size);
			buf_used += len + 1;
		}
		pkg_action(PKG_DB_INFO,
		    "package '%s' is required by these other packages "
		    "and may not be deinstalled:\n%s",
		    pkg_get_name(real_pkg), buf);
		free(buf);
		return -1;
	}

	if (pkg_run_script(real_pkg, NULL, pkg_script_require_deinstall) != 0) {
		/* XXX */
		return -1;
	}

	if (pkg_run_script(real_pkg, NULL, pkg_script_pre_deinstall) != 0) {
		/* XXX */
		return -1;
	}


	/* Remove the reverse dependencies */
	deps = pkg_db_get_installed_match(db, pkg_db_freebsd_match_rdep,
	    pkg_get_name(real_pkg));
	if (deps != NULL) {
		unsigned int pos;
		for (pos = 0; deps[pos] != NULL; pos++) {
			struct pkgfile *file;

			file = pkg_get_control_file(deps[pos], "+REQUIRED_BY");
			pkgfile_remove_line(file, pkg_get_name(real_pkg));
		}
	}

	/* Do the deinstall */
	deinstall_data.db = db;
	deinstall_data.fake = fake;
	deinstall_data.last_file[0] = '\0';
	deinstall_data.directory[0] = '\0';
	if (pkg_deinstall(real_pkg, pkg_action, &deinstall_data,
	    freebsd_do_chdir, freebsd_deinstall_file,
	    freebsd_do_exec, freebsd_deregister) != 0) {
		return -1;
	}

	/** @todo Run +POST-DEINSTALL <pkg-name>/+DEINSTALL <pkg-name> POST-DEINSTALL */

	return -1;
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

/**
 * @brief The db_chdir callback of pkg_install() for the FreeBSD package
 *     database
 * @return 0 on success or -1 on error
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
	pkg_remove_extra_slashes(install_data->directory);

	pkg_action(PKG_DB_PACKAGE, "CWD to %s", install_data->directory);

	if (!install_data->fake) {
		pkg_dir_build(install_data->directory, 0);
		return chdir(install_data->directory);
	}

	return 0;
}

/**
 * @brief The install_file callback of pkg_install() for the FreeBSD package
 *     database
 * @return  0 on success
 * @return -1 on error
 */
static int
freebsd_install_file(struct pkg *pkg, pkg_db_action *pkg_action, void *data,
	struct pkgfile *file)
{
	struct pkg_install_data *install_data;

	assert(pkg != NULL);
	assert(data != NULL);
	assert(file != NULL);

	install_data = data;

	snprintf(install_data->last_file, FILENAME_MAX, "%s",
	    pkgfile_get_name(file));

	pkg_action(PKG_DB_PACKAGE, "%s/%s", install_data->directory,
	    pkgfile_get_name(file));
	if (!install_data->fake)
		return pkgfile_write(file);
	return 0;
}

/**
 * @brief The deinstall_file callback of pkg_deinstall() for the FreeBSD
 *     package database
 * @return  0 on success
 * @return -1 on error
 */
static int
freebsd_deinstall_file(struct pkg *pkg __unused, pkg_db_action *pkg_action __unused, void *data __unused,
	struct pkgfile *file)
{
	assert(file != NULL);
	return pkgfile_unlink(file);
}
/**
 * @brief The do_chdir callback of pkg_install() for the FreeBSD package
 *     database
 * @return 0 on success or -1 on error
 */
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
	if (!install_data->fake) {
		return pkg_exec(the_cmd);
	}

	return 0;
}

/**
 * @brief The pkg_register callback of pkg_install() for the FreeBSD package
 *     database
 * @return 0 on success or -1 on error
 */
static int
freebsd_register(struct pkg *pkg, pkg_db_action *pkg_action, void *data,
		struct pkgfile **control)
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
		pkg_remove_extra_slashes(required_by);

		/** @todo Make pkgfile work to properly to create the file */
		fd = fopen(required_by, "a");
		name = pkg_get_name(pkg);
		fseek(fd, 0, SEEK_END);
		fwrite(name, strlen(name), 1, fd);
		fwrite("\n", 1, 1, fd);
		fclose(fd);
	}
	pkg_list_free(deps);
	
	pkg_action(PKG_DB_INFO, "Package %s registered in %s" DB_LOCATION "/%s",
	    pkg_get_name(pkg), db->db_base, pkg_get_name(pkg));
	return -1;
}

/**
 * @brief The pkg_deregister callback of pkg_deinstall() for the FreeBSD
 *     package database
 * @return  0 on success
 * @return -1 on error
 */
static int
freebsd_deregister(struct pkg *pkg, pkg_db_action *pkg_action __unused, void *data,
		struct pkgfile **control)
{
	unsigned int pos;
	struct pkg_install_data *install_data;
	struct pkgfile *dir;
	char db_dir[FILENAME_MAX];

	install_data = data;
	assert(install_data->db != NULL);

	assert(control != NULL);
	assert(control[0] != NULL);
	/* Remove the control files */
	for (pos = 0; control[pos] != NULL; pos++) {
		pkgfile_unlink(control[pos]);
	}

	snprintf(db_dir, FILENAME_MAX, "%s" DB_LOCATION "/%s/",
	    install_data->db->db_base, pkg_get_name(pkg));
	pkg_remove_extra_slashes(db_dir);
	dir = pkgfile_new_from_disk(db_dir, 0);
	if (dir == NULL)
		return -1;
	return pkgfile_unlink(dir);
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
					pkg_remove_extra_slashes(scratch);
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
					pkg_remove_extra_slashes(scratch);
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
