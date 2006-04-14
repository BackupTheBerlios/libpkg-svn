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

/* Internal */
pkg_static struct pkg_file	*freebsd_build_contents(
				struct pkg_freebsd_contents *);
pkg_static int			 freebsd_do_cwd(struct pkg_db *, struct pkg *,
				char *, int);
pkg_static int			 freebsd_check_contents(struct pkg_db *,
				struct pkg_freebsd_contents *, int);
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
freebsd_install_pkg_action(struct pkg_db *db, struct pkg *pkg, int reg __unused,
    int fake, pkg_db_action *pkg_action)
{
	struct pkg_file	*contents_file;
	struct pkg_file **control;
	struct pkg_freebsd_contents *contents;
	char *cwd;
	char *directory, *last_file;
	int i;
	unsigned int pos, line;

	assert(pkg != NULL);

	control = pkg_get_control_files(pkg);
	if (!control) {
		return -1;
	}

	/* Find the contents file in the control files */
	for (pos = 0; control[pos] != NULL; pos++)
		if (!strcmp(control[pos]->filename, "+CONTENTS"))
			break;
	contents_file = control[pos];
	if (!contents_file) {
		return -1;
	}

	contents = pkg_freebsd_contents_new(pkg_file_get(contents_file));
	if (!contents) {
		return -1;
	}

	if (pkg_action != NULL)
		pkg_action(PKG_DB_PACKAGE, "Package name is %s",
		    pkg_get_name(pkg));

	cwd = getcwd(NULL, 0);
	if (!cwd) {
		pkg_freebsd_contents_free(contents);
		return -1;
	}

	i = freebsd_check_contents(db, contents, fake);
	if (i < 0) {
		pkg_freebsd_contents_free(contents);
		chdir(cwd);
		free(cwd);
		return -1;
	} else
		line = i;

	/* directory is used int the processing of +CONTENTS files */
	directory = getcwd(NULL, 0);
	pkg_set_prefix(pkg, directory);
	last_file = NULL;

	/** @todo pkg_action the pre script */
	pkg_run_script(pkg, pkg_script_pre);

	/* Read through the contents file and install the package */
	for (; line < contents->line_count; line++) {
		switch (contents->lines[line].line_type) {
		case PKG_LINE_COMMENT:
		case PKG_LINE_UNEXEC:
		case PKG_LINE_DIRRM:
		case PKG_LINE_MTREE:
		case PKG_LINE_IGNORE:
			break;
		case PKG_LINE_CWD:
			/* Change to the correct directory */
			if (!fake) {
				if (reg || (!reg &&
				    strcmp(contents->lines[line].data,".")!=0)){
					free(directory);
					if (freebsd_do_cwd(db, pkg,
					    contents->lines[line].data, fake) != 0) {
						chdir(cwd);
						free(cwd);
						pkg_freebsd_contents_free(contents);
						return -1;
					}
					directory = getcwd(NULL, 0);
				}
			}
			if (pkg_action != NULL)
				pkg_action(PKG_DB_PACKAGE, "CWD to %s",
				    contents->lines[line].data);
			break;
		case PKG_LINE_EXEC: {
			char cmd[FILENAME_MAX];
			freebsd_format_cmd(cmd, FILENAME_MAX,
			    contents->lines[line].data, directory, last_file);
			if (!fake)
				pkg_exec(cmd);
			if (pkg_action != NULL)
				pkg_action(PKG_DB_PACKAGE, "execute '%s'", cmd);
			break;
		}
		case PKG_LINE_FILE: {
			/* Install a file to the correct directory */

			struct pkg_file *file;
			char *contents_sum;
			int ret;

			/* Check the contents file is correctly formated */
			if (contents->lines[line+1].line_type !=
			    PKG_LINE_COMMENT) {
				chdir(cwd);
				free(cwd);
				free(directory);
				pkg_freebsd_contents_free(contents);
				return -1;
			} else if (strncmp("MD5:", contents->lines[line+1].data,
			    4)) {
				chdir(cwd);
				free(cwd);
				free(directory);
				pkg_freebsd_contents_free(contents);
				return -1;
			}

			/* Read the file to install */
			if (contents->lines[line].line[0] == '+') {
				/*
				 * + Files are not fetched with
				 * pkg_get_next_file
				 */
				for (pos = 0; control[pos] != NULL;
				    pos++) {
					if (!strcmp(control[pos]->filename,
					    contents->lines[line].line))
						break;
				}
				file = control[pos];
			} else {
				file = pkg_get_next_file(pkg);

				/* Check the file name is correct */
				if (strcmp(file->filename,
				    contents->lines[line].line)) {
					chdir(cwd);
					free(cwd);
					free(directory);
					pkg_file_free(file);
					pkg_freebsd_contents_free(contents);
					return -1;
				}
				if (pkg_action != NULL)
					pkg_action(PKG_DB_PACKAGE, "%s/%s",
					    directory, pkg_file_get_name(file));
			}

			contents_sum = strchr(contents->lines[line+1].data,
			    ':');
			contents_sum++;
			if (S_ISREG(file->stat->st_mode) &&
			    pkg_checksum_md5(file, contents_sum) != 0) {
				chdir(cwd);
				free(cwd);
				free(directory);
				pkg_file_free(file);
				pkg_freebsd_contents_free(contents);
				return -1;
			}

			/* Install the file */
			if (!fake) {
				if (reg ||
				    (!reg && pkg_file_get_name(file)[0] !='+')){
					ret = pkg_file_write(file);
					if (ret != 0) {
						chdir(cwd);
						free(cwd);
						free(directory);
						pkg_file_free(file);
						pkg_freebsd_contents_free(contents);
						return -1;
					}
				}
			}

			/* Remember the name if there is an "@exec" line next */
			if (last_file)
				free(last_file);
			last_file = strdup(file->filename);

			if (contents->lines[line].line[0] != '+')
				pkg_file_free(file);

			line++;
			break;
		}
		default:
			fprintf(stderr, "ERROR: Incorrect line in "
			    "+CONTENTS file \"%s %s\"\n",
			    contents->lines[line].line,
			    contents->lines[line].data);
			break;
		}
	}


	if (pkg_action != NULL)
		pkg_action(PKG_DB_INFO, "Running mtree for %s..",
		    pkg_get_name(pkg));

	if (!fake)
		pkg_run_script(pkg, pkg_script_mtree);

	/** @todo pkg_action the post script */
	pkg_run_script(pkg, pkg_script_post);

	if (reg) {
		if (pkg_action != NULL)
			pkg_action(PKG_DB_INFO,
			    "Attempting to record package into " DB_LOCATION
			    "/%s..", pkg_get_name(pkg));

		/* Create the new contents file */
		if (!fake) {
			contents_file = freebsd_build_contents(contents);
			pkg_file_write(contents_file);
			pkg_file_free(contents_file);
		}
		/** @todo Register the reverse dependencies */
		if (pkg_action != NULL)
			pkg_action(PKG_DB_INFO,
			    "Package %s registered in " DB_LOCATION "/%s",
			    pkg_get_name(pkg), pkg_get_name(pkg));
	}

	free(directory);
	if (last_file != NULL)
		free(last_file);
	chdir(cwd);
	free(cwd);

	pkg_freebsd_contents_free(contents);

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

/**
 * @brief Internal function to to the correct thing for an \@cwd line
 * @return 0 if successful or -1 on error
 */
static int
freebsd_do_cwd(struct pkg_db *db, struct pkg *pkg, char *ndir, int fake) {
	char *dir;

	assert(db != NULL);
	assert(ndir != NULL);

	/*
	 * If the dir is . it should really
	 * be the package database dir
	 */
	if (ndir[0] == '.' &&
	    ndir[1] == '\0') {
		assert(pkg != NULL); /* pkg is only needed to chdir to . */

		/* When faking it don't create the database dir */
		if (fake)
			return 0;

		asprintf(&dir, "%s/var/db/pkg/%s", db->db_base,
		    pkg_get_name(pkg));
		if (!dir) {
			return -1;
		}
		pkg_dir_build(dir);
	} else {
		/* Set dir to the correct location */
		asprintf(&dir, "%s/%s", db->db_base, ndir);
		if (!dir) {
			return -1;
		}
	}
	if (chdir(dir) == -1) {
		free(dir);
		return -1;
	}

	free(dir);

	return 0;
}

/**
 * @brief Builds a new cotents file
 * @param contents The contents data to build the file from
 *
 * The file can be installed in /var/db/pkg/foo-1.2,3
 * @return The new contents file or NULL
 */
static struct pkg_file *
freebsd_build_contents(struct pkg_freebsd_contents *contents)
{
	uint64_t size, used;
	char *buffer, *ptr;
	unsigned int i;

	assert(contents != NULL);

	used = 0;
	size = 1024;
	buffer = malloc(size);
	ptr = buffer;
	if (!buffer) {
		return NULL;
	}
	for (i = 0; i < contents->line_count; i++) {
		int line_len, data_len;

		line_len = strlen(contents->lines[i].line);
		data_len = 0;
		if (contents->lines[i].line_type != PKG_LINE_FILE &&
		    contents->lines[i].line_type != PKG_LINE_IGNORE) {
			data_len = strlen(contents->lines[i].data);
		}
		/* if the line is @ignore we will ignore the 2 lines */
		switch (contents->lines[i].line_type) {
		case PKG_LINE_IGNORE:
			i += 2;
			break;
		case PKG_LINE_CWD:
			if (strcmp(contents->lines[i].data, ".")) {
				if (used + line_len + data_len + 2 >= size) {
					size += 1024;
					buffer = realloc(buffer, size);
					ptr = buffer + used;
				}
				sprintf(ptr, "%s %s\n",
				    contents->lines[i].line,
				    contents->lines[i].data);
				used += line_len + data_len + 2;
				ptr = buffer + used;
			}
		case PKG_LINE_MTREE:
			break;
		case PKG_LINE_FILE:
			if (used + line_len + 1 >= size) {
				size += 1024;
				buffer = realloc(buffer, size);
				ptr = buffer + used;
			}
			sprintf(ptr, "%s\n", contents->lines[i].line);
			used += line_len + 1;
			ptr = buffer + used;
			break;
		default:
			if (used + line_len + data_len + 2 >= size) {
				size += 1024;
				buffer = realloc(buffer, size);
				ptr = buffer + used;
			}
			sprintf(ptr, "%s %s\n", contents->lines[i].line,
			    contents->lines[i].data);
			used += line_len + data_len + 2;
			ptr = buffer + used;
			break;
		}
	}
	/*
	 * buffer now contains the data to write
	 * to /var/db/pkg/foo-1.2.3/+CONTENTS
	 */
	return pkg_file_new_from_buffer("+CONTENTS", used, buffer, NULL);
}

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
