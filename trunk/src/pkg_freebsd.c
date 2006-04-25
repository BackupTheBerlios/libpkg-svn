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

#include "pkg.h"
#include "pkg_private.h"
#include "pkg_freebsd.h"
#include "pkg_freebsd_private.h"

#include <sys/types.h>

#include <assert.h>
#include <archive.h>
#include <archive_entry.h>
#include <dirent.h>
#include <err.h>
#include <libgen.h>
#include <stdlib.h>
#include <string.h>

/* Callbacks */
pkg_static const char		 *freebsd_get_version(struct pkg *);
pkg_static const char		 *freebsd_get_origin(struct pkg *);
#ifdef NOT_YET
pkg_static int			  freebsd_add_depend(struct pkg *,struct pkg *);
pkg_static int			  freebsd_add_file(struct pkg *,
					struct pkg_file *);
#endif
pkg_static struct pkg_file	**freebsd_get_control_files(struct pkg *);
pkg_static struct pkg_file	 *freebsd_get_control_file(struct pkg *,
					const char *);
pkg_static struct pkg_file	 *freebsd_get_next_file(struct pkg *);
pkg_static int			  freebsd_install(struct pkg *, int,
					pkg_db_action *, void *, pkg_db_chdir *,
				       	pkg_db_install_file *, pkg_db_exec *,
					pkg_db_register *);
pkg_static struct pkg		**freebsd_get_deps(struct pkg *);
pkg_static int			  freebsd_run_script(struct pkg *, pkg_script);
pkg_static int			  freebsd_free(struct pkg *);

/* Internal functions */
pkg_static struct freebsd_package *freebsd_package_new(void);
pkg_static int			  freebsd_open_control_files(
					struct freebsd_package *);
pkg_static struct pkg_file	 *freebsd_get_next_entry(struct archive *);
pkg_static int			  freebsd_parse_contents(
					struct freebsd_package *);

typedef enum {
	fpkg_unknown,
	fpkg_from_file,
	fpkg_from_installed,
	fpkg_from_empty
} freebsd_type;

struct freebsd_package {
	FILE *fd;
	struct archive *archive;
	char *db_dir;
	const char *version;
	const char *origin;
	struct pkg_file **control;
	struct pkg_freebsd_contents *contents;
	struct pkg_file *next_file;
	freebsd_type pkg_type;
};

/**
 * @defgroup FreebsdPackage FreeBSD Package
 * @ingroup pkg
 *
 * @{
 */

/**
 * @brief Creates a new FreeBSD package from a FILE pointer
 * @param fd A pointer to a FILE object containing a FreeBSD Package
 *
 * This creates a pkg object from a given file pointer.
 * It is able to then manipulate the package and install the it to the pkg_db.
 * @todo Write
 * @return A new package object or NULL
 */
struct pkg *
pkg_new_freebsd_from_file(FILE *fd)
{
	struct pkg *pkg;
	struct freebsd_package *fpkg;
	const char *pkg_name;

	if (fd == NULL)
		return NULL;

	/* Create the new package data object */
	fpkg = freebsd_package_new();
	if (fpkg == NULL)
		return NULL;

	fpkg->fd = fd;
	fpkg->pkg_type = fpkg_from_file;
	fpkg->archive = archive_read_new();
	archive_read_support_compression_bzip2(fpkg->archive);
	archive_read_support_compression_gzip(fpkg->archive);
	archive_read_support_format_tar(fpkg->archive);
	archive_read_open_stream(fpkg->archive, fd, 10240);
	
	/*
	 * Get the +CONTENTS file.
	 * We can't use the callbacks as we need the
	 * package name to use with pkg_new
	 */
	freebsd_open_control_files(fpkg);
	assert(fpkg->control != NULL);

	freebsd_parse_contents(fpkg);
	assert(fpkg->contents != NULL);
	if (fpkg->contents->lines[1].line_type != PKG_LINE_NAME) {
		/** @todo cleanup */
		return NULL;
	}

	pkg_name = fpkg->contents->lines[1].data;
	pkg = pkg_new(pkg_name, freebsd_get_control_files,
	    freebsd_get_control_file, freebsd_get_deps, freebsd_free);
	if (pkg == NULL) {
		/** @todo cleanup */
		return NULL;
	}
	pkg_add_callbacks_data(pkg, freebsd_get_version, freebsd_get_origin);
	pkg_add_callbacks_install(pkg, freebsd_install, freebsd_get_next_file,
	    freebsd_run_script);
	pkg->data = fpkg;

	return pkg;
}

/**
 * @brief Creates a new FreeBSD package from one installed on a system
 * @param pkg_name The name of the package to retrieve
 * @param pkg_db_dir The directory in the database the package is registered in
 * @todo Make this work through a pkg_db callback
 * @todo Remove the need for pkg_db_dir by using a struct pkg_repo
 * @todo move the freebsd_package creation to an internal function
 *
 * This creates a package object from an installed package.
 * It can be used to retrieve information from the pkg_db and deintall
 * the package.
 * @return A pkg object or NULL
 */
struct pkg *
pkg_new_freebsd_installed(const char *pkg_name, const char *pkg_db_dir)
{
	struct pkg *pkg;
	struct freebsd_package *fpkg;
	struct stat sb;

	/* check the directory exists and is a directory */
	if (lstat(pkg_db_dir, &sb) == -1)
		return NULL;
	if (!S_ISDIR(sb.st_mode))
		return NULL;

	pkg = pkg_new(pkg_name, freebsd_get_control_files,
	    freebsd_get_control_file, NULL, freebsd_free);
	if (pkg == NULL)
		return NULL;
	pkg_add_callbacks_data(pkg, freebsd_get_version, freebsd_get_origin);

	fpkg = freebsd_package_new();
	if (fpkg == NULL) {
		pkg_free(pkg);
		return NULL;
	}
	pkg->data = fpkg;

	fpkg->pkg_type = fpkg_from_installed;

	fpkg->db_dir = strdup(pkg_db_dir);
	if (fpkg->db_dir == NULL) {
		pkg_free(pkg);
		return NULL;
	}
	return pkg;
}

/**
 * @brief Creates an empty FreeBSD package to add files to
 * @param pkg_name The name of the package
 *
 * This creates an empty FreeBSD Package.
 * It can then have files added to it, eg. in pkg_create(1)
 * @todo Write
 * @return A package object or NULL
 */
struct pkg *
pkg_new_freebsd_empty(const char *pkg_name __unused)
{
	assert(0);
	return NULL;
}

/**
 * @brief Gets the contents struct from a package
 * @todo Write
 * @return The contents struct
 */
struct pkg_freebsd_contents *
pkg_freebsd_get_contents(struct pkg *pkg __unused)
{
	assert(0);
	return NULL;
}

/**
 * @}
 */

/**
 * @defgroup FreebsdPackageCallbacks FreeBSD Package callbacks
 * @ingroup FreebsdPackage
 * @brief FreeBSD Package callback functions
 *
 * These are the callbacks the @link pkg package object @endlink uses
 * when pkg_new_freebsd_from_file(), pkg_new_freebsd_installed() or
 * pkg_new_freebsd_empty() is used to create the object
 *
 * @{
 */

/**
 * @brief Callback for pkg_get_version()
 *
 * @todo Do proper checks of line 0
 * @return A string containing the package version. Do not Free.
 */
static const char *
freebsd_get_version(struct pkg *pkg)
{
	struct freebsd_package *fpkg;
	char *s;

	assert(pkg != NULL);

	fpkg = pkg->data;
	assert(fpkg != NULL);
	assert(fpkg->pkg_type != fpkg_unknown);
	assert(fpkg->pkg_type != fpkg_from_empty);

	if (fpkg->version == NULL) {
		freebsd_parse_contents(fpkg);
		assert(fpkg->contents != NULL);
		assert(fpkg->contents->lines != NULL);
		/** @todo Make a +CONTENTS structure check */
		assert(fpkg->contents->lines[0].data != NULL);
		assert(fpkg->contents->lines[0].line_type == PKG_LINE_COMMENT);
		assert(strcmp("PKG_FORMAT_REVISION:1.1", fpkg->contents->lines[0].data) == 0);
		s = strchr(fpkg->contents->lines[0].data, ':');
		if (s == NULL)
			return NULL;
		s++;
		if (s[0] == '\0')
			return NULL;
		fpkg->version = s;
	}
	
	return fpkg->version;
}

/**
 * @brief Callback for pkg_get_origin()
 * 
 * @return A string containing the origin of the Package. Do not free.
 */
static const char *
freebsd_get_origin(struct pkg *pkg)
{
	struct freebsd_package *fpkg;

	assert(pkg != NULL);

	fpkg = pkg->data;
	assert(fpkg != NULL);
	assert(fpkg->pkg_type != fpkg_unknown);
	assert(fpkg->pkg_type != fpkg_from_empty);

	/* Find the origin line and cache it */
	if (fpkg->origin == NULL) {
		unsigned int line;

		/* Load the contents file */
		freebsd_parse_contents(fpkg);
		if (fpkg->contents == NULL)
			return NULL;
		assert(fpkg->contents->lines != NULL);

		/* Find the line with the origin */
		for (line = 0; line < fpkg->contents->line_count; line++) {
			if (fpkg->contents->lines[line].line_type ==
			    PKG_LINE_COMMENT)
			    {
				if (strncmp("ORIGIN:",
				    fpkg->contents->lines[line].data, 7) == 0) {
					fpkg->origin =
					    fpkg->contents->lines[line].data +7;
					break;
				}
			}
		}
	}
	return fpkg->origin;
}

#ifdef NOT_YET
/**
 * @brief Callback for pkg_add_dependency()
 * @todo write
 * @return -1
 */
static int
freebsd_add_depend(struct pkg *pkg __unused, struct pkg *depend __unused)
{
	assert(0);
	return -1;
}

/**
 * @brief Callback for pkg_add_file()
 * @todo Write
 * @return -1
 */
static int
freebsd_add_file(struct pkg *pkg __unused, struct pkg_file *file __unused)
{
	assert(0);
	return -1;
}
#endif

/**
 * @brief Callback for pkg_get_control_files()
 * @return An array of pkg_file or NULL
 */
static struct pkg_file **
freebsd_get_control_files(struct pkg *pkg)
{
	struct freebsd_package *fpkg;
	assert(pkg != NULL);

	fpkg = pkg->data;
	assert(fpkg != NULL);
	assert(fpkg->pkg_type != fpkg_unknown);
	assert(fpkg->pkg_type != fpkg_from_empty);

	freebsd_open_control_files(fpkg);

	assert(fpkg->control != NULL);
	return fpkg->control;
}

/**
 * @brief for pkg_get_control_file()
 * @return The named pkg_file or NULL
 */
static struct pkg_file *
freebsd_get_control_file(struct pkg *pkg, const char *filename)
{
	struct freebsd_package *fpkg;
	unsigned int pos;
	assert(pkg != NULL);

	fpkg = pkg->data;
	assert(fpkg != NULL);
	assert(fpkg->pkg_type != fpkg_unknown);
	assert(fpkg->pkg_type != fpkg_from_empty);

	freebsd_open_control_files(fpkg);
	if (fpkg->control == NULL)
		return NULL;

	for (pos = 0; fpkg->control[pos] != NULL; pos++)
		if (strcmp(basename(fpkg->control[pos]->filename), filename)==0)
			return fpkg->control[pos];
	return NULL;
}

static int
freebsd_install(struct pkg *pkg, int reg, pkg_db_action *pkg_action, void *data,
		pkg_db_chdir *db_chdir, pkg_db_install_file *install_file,
		pkg_db_exec *do_exec, pkg_db_register *pkg_register)
{
	int ret;
	unsigned int pos;
	struct pkg_file **control;
	struct pkg_file *contents_file;
	struct pkg_freebsd_contents *contents;

	assert(pkg != NULL);
	assert(pkg_action != NULL);
	assert(data != NULL);
	assert(db_chdir != NULL);
	assert(install_file != NULL);
	assert(pkg_register != NULL);

	ret = -1;
	contents = NULL;

	/* Get the control files from the package */
	control = pkg_get_control_files(pkg);
	if (control == NULL) {
		return -1;
	}

	/* Find the +CONTENTS file in the control files */
	for (pos = 0; control[pos] != NULL; pos++)
		if (!strcmp(control[pos]->filename, "+CONTENTS"))
			break;
	contents_file = control[pos];
	if (contents_file == NULL) {
		return -1;
	}

	contents = pkg_freebsd_contents_new(pkg_file_get(contents_file));
	if (contents == NULL) {
		return -1;
	}

	for (pos = 0; pos < contents->line_count; pos++) {
		char ignore;

		ignore = 0;
		if (contents->lines[pos].line_type == PKG_LINE_IGNORE) {
			ignore = ~0;
			pos++;
		}
		switch (contents->lines[pos].line_type) {
		case PKG_LINE_IGNORE:
			/* Error in contents file */
			ret = -1;
			goto exit;
		case PKG_LINE_COMMENT:
		case PKG_LINE_UNEXEC:
		case PKG_LINE_DIRRM:
		case PKG_LINE_MTREE:
		case PKG_LINE_PKGDEP:
		case PKG_LINE_CONFLICTS:
			break;
		case PKG_LINE_NAME:
			/* Check the name is the same as the packages name */
			if (strcmp(pkg_get_name(pkg),
			    contents->lines[pos].data) != 0) {
				ret = -1;
				goto exit;
			}
			break;
		case PKG_LINE_CWD:
		{
			const char *dir;

			dir = contents->lines[pos].data;
			if (strcmp(dir, ".") == 0) {
				if (reg)
					db_chdir(pkg, pkg_action, data, dir);
			} else {
				db_chdir(pkg, pkg_action, data, dir);
			}
			break;
		}
		case PKG_LINE_FILE:
		{
			struct pkg_file *file;

			file = pkg_get_next_file(pkg);
			if (file == NULL)
				file = pkg_get_control_file(pkg,
				    contents->lines[pos].line);
			if (file == NULL) {
				/* File not found in the package */
				ret = -1;
				goto exit;
			}

			/* Check the file name is correct */
			if (strcmp(contents->lines[pos].line,
			    pkg_file_get_name(file)) != 0) {
				ret = -1;
				goto exit;
			}

			if (contents->lines[pos+1].line_type ==
			    PKG_LINE_COMMENT) {
				char *p;

				p = strchr(contents->lines[pos+1].data, ':');
				p++;
				if (pkg_checksum_md5(file, p) == 0) {
					if (!ignore) {
						install_file(pkg, pkg_action,
						    data, file);
					}
				} else {
					ret = -1;
					goto exit;
				}
				pos++;
			}
			break;
		}
		case PKG_LINE_EXEC:
		{
			do_exec(pkg, pkg_action, data,
			    contents->lines[pos].data);
			break;
		}

		default:
			warnx("ERROR: Incorrect line in +CONTENTS file "
			    "\"%s %s\"\n", contents->lines[pos].line,
			    contents->lines[pos].data);
		}
	}
	/* Register the package */
	pkg_register(pkg, pkg_action, data, control);

exit:
	if (contents != NULL)
		pkg_freebsd_contents_free(contents);

	return ret;
}

/**
 * @brief Callback for pkg_get_next_file()
 * @return The next non-control pkg_file or NULL
 */
static struct pkg_file *
freebsd_get_next_file(struct pkg *pkg)
{
	struct freebsd_package *fpkg;
	struct pkg_file *file;

	assert(pkg != NULL);
	fpkg = pkg->data;
	assert(fpkg != NULL);
	assert(fpkg->pkg_type == fpkg_from_file);

	if (fpkg->next_file) {
		struct pkg_file *pkg_file = fpkg->next_file;
		fpkg->next_file = NULL;
		return pkg_file;
	}
	if (fpkg->archive == NULL)
		return NULL;

	file = freebsd_get_next_entry(fpkg->archive);
	if (file == NULL) {
		archive_read_finish(fpkg->archive);
		fpkg->archive = NULL;
	}
	return file;
}

/**
 * @brief Callback for pkg_get_dependencies()
 * @todo Write
 * @return An array of empty package objects, or NULL
 */
static struct pkg **
freebsd_get_deps(struct pkg *pkg)
{
	struct freebsd_package *fpkg;
	struct pkg **pkgs;
	unsigned int pkg_count;
	size_t pkg_size;
	unsigned int line;

	assert(pkg != NULL);

	fpkg = pkg->data;
	assert(fpkg != NULL);
	assert(fpkg->pkg_type != fpkg_unknown);
	assert(fpkg->pkg_type != fpkg_from_installed);
	assert(fpkg->pkg_type != fpkg_from_empty);

	freebsd_open_control_files(fpkg);
	assert(fpkg->control != NULL);

	assert(strcmp("+CONTENTS", pkg_file_get_name(fpkg->control[0])) == 0);

	pkg_count = 0;
	pkg_size = sizeof(struct pkg *);
	pkgs = malloc(pkg_size);
	if (pkgs == NULL)
		return NULL;
	pkgs[0] = NULL;

	for (line = 0; line < fpkg->contents->line_count; line++) {
		if (fpkg->contents->lines[line].line_type == PKG_LINE_PKGDEP) {
			pkg_size += sizeof(struct pkg *);
			pkgs = realloc(pkgs, pkg_size);
			pkgs[pkg_count] = pkg_new_empty
			    (fpkg->contents->lines[line].data);
			pkg_count++;
			pkgs[pkg_count] = NULL;
		}
	}
		
	return pkgs;
}

/**
 * @brief Callback for pkg_run_script()
 * @return 0
 */
static int
freebsd_run_script(struct pkg *pkg, pkg_script script)
{
	struct freebsd_package *fpkg;
	struct pkg_file *script_file;
	char arg[FILENAME_MAX];
	char dir[FILENAME_MAX];
	char *dir1, *cwd;
	int ret;

	assert(pkg != NULL);

	fpkg = pkg->data;
	assert(fpkg != NULL);
	assert(fpkg->pkg_type != fpkg_unknown);
	assert(fpkg->pkg_type != fpkg_from_installed);
	assert(fpkg->pkg_type != fpkg_from_empty);

	script_file = NULL;
	arg[0] = '\0';
	switch (script) {
	case pkg_script_pre:
		script_file = pkg_get_control_file(pkg, "+PRE-INSTALL");
		if (script_file == NULL) {
			script_file = pkg_get_control_file(pkg, "+INSTALL");
			snprintf(arg, FILENAME_MAX, "PRE-INSTALL");
		}
		break;
	case pkg_script_post:
		script_file = pkg_get_control_file(pkg, "+POST-INSTALL");
		if (script_file == NULL) {
			script_file = pkg_get_control_file(pkg, "+INSTALL");
			snprintf(arg, FILENAME_MAX, "POST-INSTALL");
		}
		break;
	case pkg_script_mtree:
		script_file = pkg_get_control_file(pkg, "+MTREE_DIRS");
		break;
	case pkg_script_require:
		script_file = pkg_get_control_file(pkg, "+REQUIRE");
		break;
	default:
		return -1;
	}
	if (script_file == NULL)
		return 0;

	/** @todo Add a lock around mkdtemp ad arc4random is not thread safe */
	snprintf(dir, FILENAME_MAX, "/tmp/libpkg_XXXXXXX");
	dir1 = mkdtemp(dir);

	/* Change to the temp dir and back up the current dir to return here */
	cwd = getcwd(NULL, 0);
	chdir(dir1);

	/* Extract the script */
	pkg_file_write(script_file);
	switch(script) {
	case pkg_script_mtree:
	{
		const char *prefix = pkg_get_prefix(pkg);
		pkg_exec("mtree -U -f +MTREE_DIRS -d -e -p %s >/dev/null",
		    (prefix != NULL ? prefix : "/usr/local"));
		break;
	}
	case pkg_script_pre:
	case pkg_script_post:
		pkg_exec("chmod u+x %s", pkg_file_get_name(script_file));

		/* Execute the script */
		ret = pkg_exec("%s/%s %s %s", dir1,
		    pkg_file_get_name(script_file), pkg_get_name(pkg), arg);
		break;
	case pkg_script_require:
		pkg_exec("chmod u+x %s", pkg_file_get_name(script_file));

		ret = pkg_exec("%s/%s %s INSTALL", dir1,
		    pkg_file_get_name(script_file), pkg_get_name(pkg));
		break;
	case pkg_script_noop:
		break;
	}
	unlink(pkg_file_get_name(script_file));
	chdir(cwd);
	free(cwd);

	rmdir(dir1);
	return ret;
}

/**
 * @brief Callback for pkg_free()
 * @return 0
 */
static int
freebsd_free(struct pkg *pkg)
{
	struct freebsd_package *fpkg;
	assert(pkg != NULL);

	fpkg = pkg->data;
	if (fpkg) {
		if (fpkg->db_dir != NULL)
			free(fpkg->db_dir);

		if (fpkg->next_file != NULL)
			pkg_file_free(fpkg->next_file);

		if (fpkg->control != NULL) {
			int cur;

			for (cur = 0; fpkg->control[cur] != NULL; cur++) {
				pkg_file_free(fpkg->control[cur]);
			}
			free(fpkg->control);
		}
		if (fpkg->archive != NULL)
			archive_read_finish(fpkg->archive);

		free(fpkg);
	}

	return 0;
}

/**
 * @}
 */

/**
 * @defgroup FreebsdPackageInternals FreeBSD Package internal functions
 * @ingroup FreebsdPackage
 * @brief Internal package functions
 *
 * These are the internal FreeBSD package functions.
 * They are all declared static.
 * They are documented to help others understand the internals of a
 * package class callback system.
 *
 * @{
 */


/* Internal functions */

/**
 * @brief Creates an empty struct freebsd_package
 * @return A new creebsd_package object or NULL
 */
static struct freebsd_package *
freebsd_package_new()
{
	struct freebsd_package *fpkg;

	fpkg = malloc(sizeof(struct freebsd_package));
	if (fpkg == NULL) {
		return NULL;
	}

	fpkg->fd = NULL;
	fpkg->archive = NULL;
	fpkg->db_dir = NULL;
	fpkg->control = NULL;
	fpkg->contents = NULL;
	fpkg->origin = NULL;
	fpkg->version = NULL;
	fpkg->next_file = NULL;
	fpkg->pkg_type = fpkg_unknown;

	return fpkg;
}

#define FREE_CONTENTS(c) \
	{ \
		int i; \
		for (i=0; c[i] != NULL; i++) { \
			pkg_file_free(c[i]); \
		} \
		free(c); \
	}

/**
 * @brief Opens all the control files for a package
 * @todo Make it add the files to fpkg->control
 * @return An array of files or NULL
 */
int
freebsd_open_control_files(struct freebsd_package *fpkg)
{
	unsigned int control_size, control_count;
	struct pkg_file *pkg_file;

/** @todo Check the return of realloc */
#define addFile(pkg_file) \
	control_size += sizeof(struct pkg_file **); \
	fpkg->control = realloc(fpkg->control, control_size); \
	fpkg->control[control_count] = pkg_file; \
	control_count++; \
	fpkg->control[control_count] = NULL;
	
	assert(fpkg != NULL);

	/* Don't attempt to get the control files again */
	if (fpkg->control != NULL)
		return 0;

	if (fpkg->pkg_type != fpkg_from_installed &&
	    fpkg->pkg_type != fpkg_from_file) {
		assert(0);
		return -1;
	}

	/* Setup the store to hold all the files */
	control_size = sizeof(struct pkg_file **);
	fpkg->control = malloc(control_size);
	fpkg->control[0] = NULL;
	control_count = 0;
	
	if (fpkg->pkg_type == fpkg_from_installed) {
		DIR *d;
		struct dirent *de;
		assert(fpkg->db_dir != NULL);

		d = opendir(fpkg->db_dir);
		if (d == NULL)
			return -1;

		/* Load all the + files into control */
		while ((de = readdir(d)) != NULL) {
			char *file;
	
			if (de->d_name[0] == '.') {
				continue;
			} else if (de->d_type != DT_REG) {
				closedir(d);
				FREE_CONTENTS(fpkg->control);
				return -1;
			} else if (de->d_name[0] != '+') {
				/* All files must begin with + */
				closedir(d);
				FREE_CONTENTS(fpkg->control);
				return -1;
			}
			asprintf(&file, "%s/%s", fpkg->db_dir, de->d_name);
			if (!file) {
				closedir(d);
				FREE_CONTENTS(fpkg->control);
				return -1;
			}
			pkg_file = pkg_file_new(file);
			addFile(pkg_file);
			free(file);
		}
		closedir(d);

		return 0;
	} else if (fpkg->pkg_type == fpkg_from_file) {
		assert(fpkg->archive != NULL);
		pkg_file = freebsd_get_next_entry(fpkg->archive);
		while (pkg_file_get_name(pkg_file)[0] == '+') {
			addFile(pkg_file);
			pkg_file = freebsd_get_next_entry(fpkg->archive);
		}
		fpkg->next_file = pkg_file;
		return 0;
	}
	assert(0);
	return -1;
}

/**
 * @brief Retrieves a pointer to the next file in an archive
 * @param a A libarchive(3) archive object
 * @return A the next file in the archive or NULL
 */
static struct pkg_file *
freebsd_get_next_entry(struct archive *a)
{
	uint64_t length;
	char *str;
	struct archive_entry *entry;
	const struct stat *sb;

	assert(a != NULL);

	/* Read the next entry to a buffer. */
	if (archive_read_next_header(a, &entry) != ARCHIVE_OK) {
		return NULL;
	}

	/* Get the needed struct stat from the archive */
	sb = archive_entry_stat(entry);

	if (S_ISREG(sb->st_mode)) {
		/* Allocate enough space for the file and copy it to the string */
		length = archive_entry_size(entry);
		str = malloc(length+1);
		if (!str) {
			return NULL;
		}
		archive_read_data_into_buffer(a, str, length);
		str[length] = '\0';

		/* Create the pkg_file and return it */
		return pkg_file_new_from_buffer(archive_entry_pathname(entry),
			length, str, sb);
	} else if (S_ISLNK(sb->st_mode)) {
		str = strdup(archive_entry_symlink(entry));
		if (!str)
			return NULL;

		return pkg_file_new_symlink(archive_entry_pathname(entry),
		    str, sb);
	}
	errx(1, "File is not regular or symbolic link");
	return NULL;
}

/**
 * @brief Parses the packages +CONTENTS file
 * @return 0 on success, or -1 on error
 */
static int
freebsd_parse_contents(struct freebsd_package *fpkg)
{
	struct pkg_file *contents_file;
	int i;
	
	assert(fpkg != NULL);

	if (fpkg->contents != NULL)
		return 0;

	freebsd_open_control_files(fpkg);

	contents_file = NULL;
	for (i = 0; fpkg->control[i] != NULL; i++) {
		if (strcmp("+CONTENTS",
		    basename(pkg_file_get_name(fpkg->control[i]))) == 0) {
			contents_file = fpkg->control[i];
			break;
		}
	}
	if (contents_file == NULL)
		return -1;
	fpkg->contents = pkg_freebsd_contents_new(pkg_file_get(contents_file));
	return 0;
}

/**
 * @}
 */
