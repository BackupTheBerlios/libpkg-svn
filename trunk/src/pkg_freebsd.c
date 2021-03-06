/*
 * Copyright (C) 2006, Andrew Turner
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include "pkg.h"
#include "pkg_private.h"

#include <sys/param.h>
#include <sys/types.h>

#include <assert.h>
#include <archive.h>
#include <archive_entry.h>
#include <dirent.h>
#include <err.h>
#include <errno.h>
#include <libgen.h>
#include <stdlib.h>
#include <string.h>

/* Callbacks */
static const char	 *freebsd_get_version(struct pkg *);
static const char	 *freebsd_get_origin(struct pkg *);
static int		  freebsd_set_origin(struct pkg *, const char *);
#ifdef NOT_YET
static int		  freebsd_add_depend(struct pkg *,struct pkg *);
static int		  freebsd_add_file(struct pkg *,
				struct pkgfile *);
#endif
static struct pkgfile	**freebsd_get_control_files(struct pkg *);
static struct pkgfile	 *freebsd_get_control_file(struct pkg *,
					const char *);
static struct pkg_manifest *freebsd_get_manifest(struct pkg *);
static struct pkgfile	 *freebsd_get_next_file(struct pkg *);
static int		  freebsd_install(struct pkg *, const char *,
				int, pkg_db_action *, void *,
				pkg_db_chdir *, pkg_db_install_file *,
				pkg_db_exec *, pkg_db_register *);
static int		  freebsd_deinstall(struct pkg *,
				pkg_db_action *, void *,
				pkg_db_chdir *, pkg_db_install_file *,
				pkg_db_exec *, pkg_db_deregister *);
static struct pkg	**freebsd_get_deps(struct pkg *);
static struct pkg	**freebsd_get_rdeps(struct pkg *);
static int		  freebsd_run_script(struct pkg *,const char *,
				pkg_script);
static int		  freebsd_free(struct pkg *);

/* Internal functions */
static struct freebsd_package	 *freebsd_package_new(void);
static int			  freebsd_open_control_files(
					struct freebsd_package *);
static struct pkgfile		 *freebsd_get_next_entry(struct archive *);

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
	char *origin;
	struct pkgfile **control;
	struct pkgfile *next_file;
	struct pkgfile *cur_file;
	unsigned int line;
	char *curdir;
	freebsd_type pkg_type;
};


/**
 * @defgroup FreebsdPackage FreeBSD Package
 * @ingroup Package
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
	struct pkg_manifest *manifest;
	const char *pkg_name;
	int i;

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
	archive_read_open_FILE(fpkg->archive, fd);

	/*
	 * Get the +CONTENTS file.
	 * We can't use the callbacks as we need the
	 * package name to use with pkg_new
	 */
	freebsd_open_control_files(fpkg);
	assert(fpkg->control != NULL);

	/* Read in the manifest to check if this is a FreeBSD package */
	for (i = 0; fpkg->control[i] != NULL; i++) {
		if (strcmp("+CONTENTS",
		    basename(pkgfile_get_name(fpkg->control[i]))) == 0) {
			manifest =
			    pkg_manifest_new_freebsd_pkgfile(fpkg->control[i]);
			break;
		}
	}
	if (manifest == NULL) {
		/* TODO: Cleanup */
		return NULL;
	}

	pkg_name = pkg_manifest_get_name(manifest);
	pkg = pkg_new(pkg_name, manifest, freebsd_get_control_files,
	    freebsd_get_control_file, freebsd_get_manifest, freebsd_get_deps,
	    NULL, freebsd_free);
	if (pkg == NULL) {
		/** @todo cleanup */
		return NULL;
	}
	pkg_add_callbacks_data(pkg, freebsd_get_version, freebsd_get_origin,
	    freebsd_set_origin);
	pkg_add_callbacks_install(pkg, freebsd_install, NULL,
	    freebsd_get_next_file, freebsd_run_script);
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

	pkg = pkg_new(pkg_name, NULL, freebsd_get_control_files,
	    freebsd_get_control_file, freebsd_get_manifest, freebsd_get_deps,
	    freebsd_get_rdeps, freebsd_free);
	if (pkg == NULL)
		return NULL;
	pkg_add_callbacks_data(pkg, freebsd_get_version, freebsd_get_origin,
	    freebsd_set_origin);
	pkg_add_callbacks_install(pkg, NULL, freebsd_deinstall,
	    freebsd_get_next_file, freebsd_run_script);

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
pkg_new_freebsd_empty(const char *pkg_name)
{
	struct pkg *pkg;
	struct freebsd_package *fpkg;

	/* Create the package */
	pkg = pkg_new(pkg_name, NULL, NULL, NULL, NULL, NULL,NULL,freebsd_free);
	if (pkg == NULL)
		return NULL;

	/* Add the data callbacks */
	pkg_add_callbacks_data(pkg, freebsd_get_version, freebsd_get_origin,
	    freebsd_set_origin);

	/* Create the FreeBSD data and add it to the package */
	fpkg = freebsd_package_new();
	if (fpkg == NULL) {
		pkg_free(pkg);
		return NULL;
	}
	pkg->data = fpkg;
	fpkg->pkg_type = fpkg_from_empty;

	return pkg;
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

	assert(pkg != NULL);

	fpkg = pkg->data;
	assert(fpkg != NULL);
	assert(fpkg->pkg_type != fpkg_unknown);

	if (fpkg->version == NULL) {
		const char *version;

		pkg_get_manifest(pkg);
		if (pkg->pkg_manifest == NULL)
			return NULL;

		version = pkg_manifest_get_manifest_version(pkg->pkg_manifest);
		if (version == NULL)
			return NULL;

		fpkg->version = strdup(version);
	}
	
	return fpkg->version;
}

/**
 * @brief Callback for pkg_get_origin()
 * @param pkg The package to find the origin for
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

	if (fpkg->origin == NULL) {
		pkg_get_manifest(pkg);

		if (pkg->pkg_manifest == NULL)
			return NULL;

		fpkg->origin = strdup(pkg_manifest_get_attr(pkg->pkg_manifest,
		    pkgm_origin));
	}

	return fpkg->origin;
}

/**
 * @brief Callback for pkg_set_origin()
 * @param pkg The package to set the origin for
 * @param origin The new origin to set
 * @return  0 on success
 * @return -1 on error
 */
static int
freebsd_set_origin(struct pkg *pkg, const char *origin)
{
	struct freebsd_package *fpkg;

	assert(pkg != NULL);

	fpkg = pkg->data;
	assert(fpkg != NULL);
	assert(fpkg->pkg_type != fpkg_unknown);

	if (fpkg->origin != NULL)
		free(fpkg->origin);

	fpkg->origin = strdup(origin);
	if (fpkg->origin == NULL)
		return -1;

	return 0;
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
freebsd_add_file(struct pkg *pkg __unused, struct pkgfile *file __unused)
{
	assert(0);
	return -1;
}
#endif

/**
 * @brief Callback for pkg_get_control_files()
 * @return An array of pkgfile or NULL
 */
static struct pkgfile **
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
 * @return The named pkgfile or NULL
 */
static struct pkgfile *
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

	for (pos = 0; fpkg->control[pos] != NULL; pos++) {
		const char *pkg_filename = pkgfile_get_name(fpkg->control[pos]);
		if (strcmp(basename(pkg_filename), filename) == 0)
			return fpkg->control[pos];
	}
	return NULL;
}

/**
 * @brief Callback for pkg_get_manifest
 * @param pkg The package to get the manifest from
 * @return The packages manifest
 * @return NULL if the manifest is bad
 */
static struct pkg_manifest *
freebsd_get_manifest(struct pkg *pkg)
{
	struct pkgfile *contents_file;

	assert(pkg != NULL);
	assert(pkg->pkg_manifest == NULL);

	/* Get the +CONTENTS file */
	contents_file = pkg_get_control_file(pkg, "+CONTENTS");

	pkg->pkg_manifest = pkg_manifest_new_freebsd_pkgfile(contents_file);

	return pkg->pkg_manifest;
}

/**
 * @brief Callback for pkg_install()
 * @return 0 on success or -1 on error
 */
static int
freebsd_install(struct pkg *pkg, const char *prefix, int reg,
		pkg_db_action *pkg_action, void *data, pkg_db_chdir *db_chdir,
		pkg_db_install_file *install_file, pkg_db_exec *do_exec,
		pkg_db_register *pkg_register)
{
	struct pkg_manifest_item **items;
	int ret;
	unsigned int pos;
	const char *cwd, *dir;
	int only_control_files = 0;

	assert(pkg != NULL);
	assert(pkg_action != NULL);
	assert(data != NULL);
	assert(db_chdir != NULL);
	assert(install_file != NULL);
	assert(pkg_register != NULL);

	ret = -1;
	cwd = NULL;

	pkg_get_manifest(pkg);
	assert(pkg->pkg_manifest != NULL);

	/* chdir to the packages prefix */
	dir = prefix;
	if (dir == NULL)
		dir = pkg_manifest_get_attr(pkg->pkg_manifest, pkgm_prefix);
	cwd = dir;
	db_chdir(pkg, pkg_action, data, dir);

	items = pkg_manifest_get_items(pkg->pkg_manifest);
	for (pos = 0; items[pos] != NULL; pos++) {
		switch(pkg_manifest_item_get_type(items[pos])) {
		/* Unused item types */
		case pmt_comment:
		case pmt_dir:
		case pmt_dirlist:
			break;
		case pmt_file:
		{
			struct pkgfile *file = NULL;
			const char *name;

			name = pkg_manifest_item_get_data(items[pos]);

			if (only_control_files == 0)
				file = pkg_get_next_file(pkg);
			if (only_control_files != 0 || file == NULL) {
				only_control_files = ~0;
				file = pkg_get_control_file(pkg, name);
			}
			if (file == NULL) {
				/* File not found in the package */
				ret = -1;
				goto exit;
			}

			/* Check the file name is correct */
			if (strcmp(name, pkgfile_get_name(file)) != 0) {
				ret = -1;
				goto exit;
			}

			/* Install the file */
			if (pkgfile_compare_checksum_md5(file) == 0 || 1) {
				if (pkg_manifest_item_get_attr(items[pos],
				    pmia_ignore) == NULL) {
					pkgfile_set_cwd(file, cwd);
					install_file(pkg, pkg_action, data, file);
				}
			} else {
				ret = -1;
				goto exit;
			}
			break;
		}
		case pmt_chdir:
		{
			const char *new_dir;
			dir = NULL;
			new_dir = pkg_manifest_item_get_data(items[pos]);
			if (strcmp(new_dir, ".") == 0) {
				if (reg)
					dir = new_dir;
			} else
				dir = new_dir;

			if (dir != NULL) {
				cwd = dir;
				db_chdir(pkg, pkg_action, data, dir);
			}

			break;
		}
		case pmt_output:
			printf("%s\n",
			  (const char *)pkg_manifest_item_get_data(items[pos]));
			break;
		case pmt_execute:
		{
			const char *cmd, *attr;

			attr = pkg_manifest_item_get_attr(items[pos],
			    pmia_deinstall);
			if (attr == NULL || strcasecmp(attr, "NO") == 0) {
				cmd = pkg_manifest_item_get_data(items[pos]);
				do_exec(pkg, pkg_action, data, cmd);
			}
			break;
		}
		case pmt_other:
		case pmt_error:
			/*
			 * This should never happen as pmt_other and
			 * pmt_error don't appear in pkg_freebsd_parser.y
			 */
			abort();
			break;
		}
	}

	/* Set the return to 0 as we have fully installed the package */
	ret = 0;

	if (reg) {
		/* Register the package */
		ret = pkg_register(pkg, pkg_action, data, prefix);
	}

exit:
	return ret;
}

/**
 * @brief Callback for pkg_deinstall()
 * @return  0 on success
 * @return -1 on error
 */
static int
freebsd_deinstall(struct pkg *pkg, pkg_db_action *pkg_action, void *data,
		pkg_db_chdir *db_chdir, pkg_db_install_file *deinstall_file,
		pkg_db_exec *do_exec, pkg_db_deregister *pkg_deregister)
{
	struct pkg_manifest_item **items;
	int ret;
	unsigned int pos;
	const char *cwd, *dir;

	assert(pkg != NULL);
	assert(pkg_action != NULL);
	assert(data != NULL);
	assert(db_chdir != NULL);
	assert(deinstall_file != NULL);
	assert(do_exec != NULL);
	assert(pkg_deregister != NULL);

	ret = -1;

	/* Read the package manifest */
	pkg_get_manifest(pkg);
	assert(pkg->pkg_manifest != NULL);

	dir = pkg_manifest_get_attr(pkg->pkg_manifest, pkgm_prefix);
	cwd = dir;
	db_chdir(pkg, pkg_action, data, dir);

	items = pkg_manifest_get_items(pkg->pkg_manifest);
	for (pos = 0; items[pos] != NULL; pos++) {
		switch(pkg_manifest_item_get_type(items[pos])) {
		/* Unused item types */
		case pmt_comment:
		case pmt_dirlist:
			break;
		case pmt_dir:
		case pmt_file:
		{
			const char *attr, *file_name;
			struct pkgfile *file;

			attr =
			    pkg_manifest_item_get_attr(items[pos], pmia_ignore);

			if (attr == NULL || strcasecmp(attr, "NO") == 0) {
				file_name =
				    pkg_manifest_item_get_data(items[pos]);
				file = pkgfile_new_from_disk(file_name, 0);
				deinstall_file(pkg, pkg_action, data, file);
			}
			break;
		}
		case pmt_chdir:
			dir = pkg_manifest_item_get_data(items[pos]);

			if (dir != NULL) {
				cwd = dir;
				db_chdir(pkg, pkg_action, data, dir);
			}

			break;
		case pmt_output:
			break;
		case pmt_execute:
		{
			const char *cmd, *attr;

			attr = pkg_manifest_item_get_attr(items[pos],
			    pmia_deinstall);
			if (attr != NULL && strcasecmp(attr, "YES") == 0) {
				cmd = pkg_manifest_item_get_data(items[pos]);
				do_exec(pkg, pkg_action, data, cmd);
			}
			break;
		}
		case pmt_other:
		case pmt_error:
			/*
			 * This should never happen as pmt_other and
			 * pmt_error don't appear in pkg_freebsd_parser.y
			 */
			abort();
			break;
		}
	}

	db_chdir(pkg, pkg_action, data, ".");
	/* Register the package */
	pkg_deregister(pkg, pkg_action, data);

	/* Set the return to 0 as we have fully installed the package */
	ret = 0;

	return ret;
}

/**
 * @brief Callback for pkg_get_next_file()
 * @return The next non-control pkgfile or NULL
 */
static struct pkgfile *
freebsd_get_next_file(struct pkg *pkg)
{
	struct pkg_manifest_item **items;
	struct freebsd_package *fpkg;
	struct pkgfile *file;

	assert(pkg != NULL);
	fpkg = pkg->data;
	assert(fpkg != NULL);

	file = NULL;
	if (fpkg->next_file != NULL) {
		file = fpkg->next_file;

		/*
		 * Hand over the file to be free'ed from
		 * memory when the next file is read
		 */
		fpkg->cur_file = fpkg->next_file;
		fpkg->next_file = NULL;
	} else if (fpkg->archive == NULL)  {
		/* Read the file from disk */
		items = pkg_manifest_get_items(pkg->pkg_manifest);
		for (; items[fpkg->line] != NULL; fpkg->line++) {
			switch(pkg_manifest_item_get_type(items[fpkg->line])) {
			/* Unused item types */
			case pmt_comment:
			case pmt_dir:
			case pmt_dirlist:
			case pmt_output:
			case pmt_execute:
				break;
			case pmt_chdir:
			{
				const char *dir;
				if (fpkg->curdir != NULL)
					free(fpkg->curdir);
				dir = pkg_manifest_item_get_data(
				    items[fpkg->line]);
				fpkg->curdir = strdup(dir);
				break;
			}
			case pmt_file:
			{
				const char *file_name, *md5;
				char the_file[FILENAME_MAX + 1];

				/*
				 * We will always return from
				 * this so increment the line
				 * now to stop an infinite loop
				*/
				fpkg->line++;

				/* Get the file's absolute name */
				file_name = pkg_manifest_item_get_data(
				    items[fpkg->line]);
				snprintf(the_file, FILENAME_MAX, "%s/%s",
				    fpkg->curdir, file_name);
				/* Remove extra slashes from the path */
				pkg_remove_extra_slashes(the_file);

				/* Open the file */
				file = pkgfile_new_from_disk(the_file, 1);

				if (file == NULL)
					return NULL;

				/* Add the recorded md5 to the file */
				md5 = pkg_manifest_item_get_attr(
				    items[fpkg->line], pmia_md5);
				if (md5 != NULL) {
					strncpy(file->md5, md5, 32);
					file->md5[33] = '\0';
				}
				return file;
			}
			case pmt_other:
			case pmt_error:
				/*
				 * This should never happen as
				 * pmt_other and pmt_error don't
				 * appear in pkg_freebsd_parser.y
				 */
				abort();
				break;
			}
		}


		/* If we are here there must be no more files in the manifest */
		fpkg->line = 0;
		return NULL;
	} else {
		if (fpkg->cur_file != NULL)
			pkgfile_free(fpkg->cur_file);
		file = freebsd_get_next_entry(fpkg->archive);
		if (file == NULL) {
			archive_read_finish(fpkg->archive);
			fpkg->archive = NULL;
		}
		fpkg->cur_file = file;
	}
	return file;
}

/**
 * @brief Callback for pkg_get_dependencies()
 * @param pkg The package to find the dependencies for
 * @return An array of empty package objects
 * @return NULL on error
 */
static struct pkg **
freebsd_get_deps(struct pkg *pkg)
{
	assert(pkg != NULL);

	pkg_get_manifest(pkg);
	if (pkg->pkg_manifest == NULL)
		return NULL;

	return pkg_manifest_get_dependencies(pkg->pkg_manifest);
}

static struct pkg **
freebsd_get_rdeps(struct pkg *pkg)
{
	struct freebsd_package *fpkg;

	assert(pkg != NULL);

	fpkg = pkg->data;
	assert(fpkg != NULL);
	assert(fpkg->pkg_type != fpkg_unknown);
	assert(fpkg->pkg_type != fpkg_from_file);
	assert(fpkg->pkg_type != fpkg_from_empty);

	if (fpkg->pkg_type == fpkg_from_installed) {
		unsigned int pos, size;
		struct pkgfile **control;
		struct pkg **ret;
		unsigned int ret_size, ret_count;
		const char *data, *str1, *str2;
		char pkg_name[MAXPATHLEN];

		ret = malloc(sizeof(struct pkg *));
		if (ret == NULL)
			return NULL;
		ret[0] = NULL;

		/* Open the control files and find the +REQUIRED_BY file */
		freebsd_open_control_files(fpkg);
		control = fpkg->control;
		for (pos = 0; control[pos] != NULL; pos++) {
			const char *pkg_filename=pkgfile_get_name(control[pos]);
			if (strcmp(basename(pkg_filename), "+REQUIRED_BY") == 0)
				break;
		}
		/*
		 * If there is no +REQUIRED_BY file
		 * there are no reverse dependencies
		 */
		if (control[pos] == NULL)
			return ret;

		ret_count = 0;
		ret_size = 0;
/** @todo make this general enough to remove the repeated code */
#define addPkg(pkg_name) \
	{ \
		struct pkg *dep; \
		dep = pkg_new_freebsd_empty(pkg_name); \
		ret_size += sizeof(struct pkg **); \
		ret = realloc(ret, ret_size); \
		ret[ret_count] = dep; \
		ret_count++; \
		ret[ret_count] = NULL; \
	}

		data = pkgfile_get_data(control[pos]);
		if (data != NULL) {
			str1 = data;
			/* @todo Write comment on what this does */
			while ((str2 = strchr(str1, '\n')) != NULL) {
				unsigned int len = str2-str1;
				strncpy(pkg_name, str1, len);
				pkg_name[len] = '\0';
				addPkg(pkg_name);
				str1 = str2+1;
			}

			size = pkgfile_get_size(control[pos]);
			if ((unsigned int)(str1 - data) != size) {
				unsigned int len = data + size - str1;
				strncpy(pkg_name, str1, len);
				pkg_name[len] = '\0';
				addPkg(pkg_name);
			}
		}
#undef addPkg
		return ret;
	}

	return NULL;
}

/**
 * @brief Callback for pkg_run_script()
 * @return 0
 */
static int
freebsd_run_script(struct pkg *pkg, const char *prefix, pkg_script script)
{
	struct freebsd_package *fpkg;
	struct pkgfile *script_file;
	char arg[FILENAME_MAX];
	char dir[FILENAME_MAX];
	char *cwd;
	int ret = -1;

	assert(pkg != NULL);

	fpkg = pkg->data;
	assert(fpkg != NULL);
	assert(fpkg->pkg_type != fpkg_unknown);
	assert(fpkg->pkg_type != fpkg_from_empty);

	script_file = NULL;
	arg[0] = '\0';
	dir[0] = '\0';
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
	case pkg_script_pre_deinstall:
	case pkg_script_post_deinstall:
		break;
	case pkg_script_mtree:
		assert(script_file == NULL);
		script_file = pkg_get_control_file(pkg, "+MTREE_DIRS");
		break;
	case pkg_script_require:
	case pkg_script_require_deinstall:
		script_file = pkg_get_control_file(pkg, "+REQUIRE");
		if (script == pkg_script_require) {
			snprintf(arg, FILENAME_MAX, "INSTALL");
		} else if (script == pkg_script_require_deinstall) {
			snprintf(arg, FILENAME_MAX, "DEINSTALL");
		}
		break;
	case pkg_script_deinstall:
		script_file = pkg_get_control_file(pkg, "+DEINSTALL");
		snprintf(arg, FILENAME_MAX, "DEINSTALL");
		break;
	case pkg_script_noop:
		return -1;
	}

	/* The script was not found so ignore it */
	if (script_file == NULL)
		return 0;

	if (fpkg->pkg_type == fpkg_from_file) {
		/**
		 * @todo Add a lock around mkdtemp as
		 * arc4random is not thread safe
		 */
		snprintf(dir, FILENAME_MAX, "/tmp/libpkg_XXXXXXX");
		mkdtemp(dir);

		/* Change to the temp dir and back up the current dir to return here */
		cwd = getcwd(NULL, 0);
		chdir(dir);

		/* Extract the script */
		pkgfile_write(script_file);
	}

	switch(script) {
	case pkg_script_mtree:
	{
		if (prefix == NULL)
			prefix = pkg_get_prefix(pkg);
		pkg_exec("mtree -U -f +MTREE_DIRS -d -e -p %s >/dev/null",
		    (prefix != NULL ? prefix : "/usr/local"));
		break;
	}
	case pkg_script_pre:
	case pkg_script_post:
	case pkg_script_require:
	case pkg_script_require_deinstall:
	case pkg_script_pre_deinstall:
	case pkg_script_post_deinstall:
	case pkg_script_deinstall:
		/* Check the script has the execute bit set */
		pkg_exec("chmod u+x %s", pkgfile_get_name(script_file));

		/* Execute the script */
		ret = pkg_exec("%s/%s %s %s", dir,
		    pkgfile_get_name(script_file), pkg_get_name(pkg), arg);
		break;
	case pkg_script_noop:
		break;
	}
	unlink(pkgfile_get_name(script_file));

	if (fpkg->pkg_type == fpkg_from_file) {
		chdir(cwd);
		free(cwd);
		rmdir(dir);
	}
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

		/** @todo Fix this to only call free when required */
		/* if (fpkg->origin != NULL)
			free(fpkg->origin); */

		if (fpkg->next_file != NULL)
			pkgfile_free(fpkg->next_file);

		if (fpkg->cur_file == NULL)
			pkgfile_free(fpkg->cur_file);

		if (fpkg->control != NULL) {
			int cur;

			for (cur = 0; fpkg->control[cur] != NULL; cur++) {
				pkgfile_free(fpkg->control[cur]);
			}
			free(fpkg->control);
		}
		if (fpkg->fd != NULL)
			fclose(fpkg->fd);
		if (fpkg->archive != NULL)
			archive_read_finish(fpkg->archive);

		if (fpkg->curdir != NULL)
			free(fpkg->curdir);

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
	fpkg->origin = NULL;
	fpkg->version = NULL;
	fpkg->next_file = NULL;
	fpkg->cur_file = NULL;
	fpkg->line = 0;
	fpkg->curdir = NULL;
	fpkg->pkg_type = fpkg_unknown;

	return fpkg;
}

/**
 * @brief Frees a file list
 */
#define FREE_CONTENTS(c) \
	{ \
		int i; \
		for (i=0; c[i] != NULL; i++) { \
			pkgfile_free(c[i]); \
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
	struct pkgfile *pkgfile;

/** @todo Check the return of realloc */
#define addFile(pkgfile) \
	control_size += sizeof(struct pkgfile **); \
	fpkg->control = realloc(fpkg->control, control_size); \
	fpkg->control[control_count] = pkgfile; \
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
	control_size = sizeof(struct pkgfile **);
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
			pkg_remove_extra_slashes(file)
			pkgfile = pkgfile_new_from_disk(file, 1);
			addFile(pkgfile);
			free(file);
		}
		closedir(d);

		return 0;
	} else if (fpkg->pkg_type == fpkg_from_file) {
		assert(fpkg->archive != NULL);
		pkgfile = freebsd_get_next_entry(fpkg->archive);
		while (pkgfile_get_name(pkgfile)[0] == '+') {
			addFile(pkgfile);
			pkgfile = freebsd_get_next_entry(fpkg->archive);
		}
		fpkg->next_file = pkgfile;
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
static struct pkgfile *
freebsd_get_next_entry(struct archive *a)
{
	uint64_t length;
	char *str;
	struct archive_entry *entry;
	const struct stat *sb;
	struct pkgfile *file;

	assert(a != NULL);

	/* Read the next entry to a buffer. */
	if (archive_read_next_header(a, &entry) != ARCHIVE_OK) {
		return NULL;
	}

	/* Get the needed struct stat from the archive */
	sb = archive_entry_stat(entry);

	file = NULL;
	if (S_ISREG(sb->st_mode)) {
		/* Allocate enough space for the file and copy it to the string */
		length = archive_entry_size(entry);
		str = malloc(length+1);
		if (!str) {
			return NULL;
		}
		archive_read_data_into_buffer(a, str, length);
		str[length] = '\0';

		/* Create the pkgfile and return it */
		file = pkgfile_new_regular(archive_entry_pathname(entry), str,
		    length);
		free(str);
	} else if (S_ISLNK(sb->st_mode)) {
		file = pkgfile_new_symlink(archive_entry_pathname(entry),
		    archive_entry_symlink(entry));
		
	} else {
		/* Probibly a hardlink */
		const char *hard_link = archive_entry_hardlink(entry);
		if (hard_link != NULL) {
			file = pkgfile_new_hardlink(
			    archive_entry_pathname(entry), hard_link);
		}
	}
	if (file == NULL)
		errx(1, "File is not regular, a hard link or a symbolic link");

	pkgfile_set_mode(file, sb->st_mode);
	return file;
}

/**
 * @}
 */
