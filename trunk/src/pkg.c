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

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "pkg.h"
#include "pkg_private.h"

/**
 * @defgroup PackageInternal Internal package functions
 * @ingroup Package
 * @brief Internal functions in the package module
 *
 * None of the callbacks for any given package format need to be specified.
 * If they are NULL the accessor function will return an error.
 * @{
 */

/**
 * @brief Creates a new package and associates callbacks that are used
 * by most types of packages.
 * @param pkg_name The name of the package
 * @param control_files A callback to be used by pkg_get_control_files()
 * @param control_file A callback to be used by pkg_get_control_file()
 * @param get_deps A callback to be used by pkg_get_dependencies()
 * @param get_rdeps A callback to be used by pkg_get_reverse_dependencies()
 * @param free_pkg A call back to be used by pkg_free()
 * @return A new pkg object, or NULL on error
 */
struct pkg *
pkg_new(const char *pkg_name,
		pkg_get_control_files_callback *control_files,
		pkg_get_control_file_callback *control_file,
		pkg_get_dependencies_callback *get_deps,
		pkg_get_dependencies_callback *get_rdeps,
		pkg_free_callback *free_pkg)
{
	struct pkg *pkg;

	/* A package must have a name */
	if (pkg_name == NULL)
		return NULL;

	pkg = malloc(sizeof(struct pkg));
	if (!pkg) {
		return NULL;
	}

	pkg->pkg_name = strdup(pkg_name);
	if (!pkg->pkg_name) {
		free(pkg);
		return NULL;
	}

	/* Add the given callbacks to the struct */
	pkg->pkg_get_control_files = control_files;
	pkg->pkg_get_control_file = control_file;
	pkg->pkg_get_deps = get_deps;
	pkg->pkg_get_rdeps = get_rdeps;
	pkg->pkg_free = free_pkg;

	/* Set the other callbacks to NULL */
	pkg->pkg_get_version = NULL;
	pkg->pkg_get_origin = NULL;
	pkg->pkg_set_origin = NULL;
	pkg->pkg_add_depend = NULL;
	pkg->pkg_add_file = NULL;
	pkg->pkg_get_next_file = NULL;
	pkg->pkg_run_script = NULL;
	pkg->pkg_install = NULL;
	pkg->pkg_deinstall = NULL;

	/* The data is unknown so set to NULL */
	pkg->pkg_prefix = NULL;
	pkg->data = NULL;

	return pkg;
}

/**
 * @brief Internal function to add callbacks to retrieve data
 * eg. the packages origin on FreeBSD
 * @param pkg The package returned by pkg_new()
 * @param get_version A callback to be used by pkg_get_version()
 * @param get_origin A callback to be used by pkg_get_origin()
 * @param set_origin A callback to be used by pkg_set_origin()
 * @return  0 on success
 * @return -1 on error
 */
int
pkg_add_callbacks_data(struct pkg *pkg,
		pkg_get_version_callback *get_version,
		pkg_get_origin_callback *get_origin,
		pkg_set_origin_callback *set_origin)
{
	if (pkg == NULL)
		return -1;

	pkg->pkg_get_version = get_version;
	pkg->pkg_get_origin = get_origin;
	pkg->pkg_set_origin = set_origin;
	return 0;
}

/**
 * @brief Internal function to add callbacks that add data to the package
 * @param pkg The package returned by pkg_new()
 * @param add_depend A callback to be used by pkg_get_dependencies()
 * @param add_file A callback to be used by pkg_add_file()
 * @return  0 on success
 * @return -1 on error
 */
int
pkg_add_callbacks_empty(struct pkg *pkg, 
		pkg_add_dependency_callback *add_depend,
		pkg_add_file_callback *add_file)
{
	if (pkg == NULL)
		return -1;

	pkg->pkg_add_depend = add_depend;
	pkg->pkg_add_file = add_file;

	return 0;
}

/**
 * @brief Internal function to add callbacks that are used when a package is installed
 * @param pkg The package returned by pkg_new()
 * @param install A callback to be used by pkg_install()
 * @param deinstall A callback to be used by pkg_deinstall()
 * @param next_file A callback to be used by pkg_get_next_file()
 * @param run_script A callback to be used by pkg_run_script()
 * @return  0 on success
 * @return -1 on error
 */
int
pkg_add_callbacks_install (struct pkg *pkg,
		pkg_install_callback *install,
		pkg_deinstall_callback *deinstall,
		pkg_get_next_file_callback *next_file,
		pkg_run_script_callback *run_script)
{
	if (pkg == NULL)
		return -1;

	pkg->pkg_install = install;
	pkg->pkg_deinstall = deinstall;
	pkg->pkg_get_next_file = next_file;
	pkg->pkg_run_script = run_script;
	return 0;
}

/**
 * @}
 */

/**
 * @defgroup Package Package manipulation Functions
 * 
 * These are the publicly availiable package manipulation functions.
 *
 * Most functions take a pointer to struct pkg as the first argument.
 * This is the package that is currently being worked on.
 * @{
 */

/**
 * @brief Creates an empty package with no callbacks
 * @param pkg_name The name of the package
 *
 * This is the simplest package constructor.
 * It is used to create a package with just a name associated with it.
 * Only pkg_get_name() and pkg_compare() will work with the created package.
 * @return An empty package
 */
struct pkg*
pkg_new_empty(const char *pkg_name)
{
	return pkg_new(pkg_name, NULL, NULL, NULL, NULL, NULL);
}

/**
 * @brief A function to pass to *sort[_r] to sort alphabeticly by package name
 * @param pkg_a the first package
 * @param pkg_b the second package
 *  
 * This is used to lexigraphically compare two packages by their name.
 * It is designed to be used with qsort() to sort a list of packages
 * @return 0 > if pkg_a should be before pkg_b,
 *         0 if both packages have the same name,
 *         0 < if pkg_b should be before pkg_a.
 */
int
pkg_compare(const void *pkg_a, const void *pkg_b)
{
	return strcmp((*(struct pkg * const *)pkg_a)->pkg_name,
	    (*(struct pkg * const *)pkg_b)->pkg_name);
}

/**
 * @brief Sets the location to install the package to
 * @param pkg The package
 * @param prefix The location in the filesystem to install the package pkg to
 * 
 * If the prefix is set and malloc fails the old prefix is kept.
 * Otherwise the prefix is set to the new prefix.
 * @return 0 on success, -1 on error
 */
int
pkg_set_prefix(struct pkg *pkg, const char *prefix)
{
	char *old_prefix;
	if (pkg == NULL)
		return -1;

	if (prefix == NULL)
		return -1;

	old_prefix = pkg->pkg_prefix;
	pkg->pkg_prefix = strdup(prefix);
	if (pkg->pkg_prefix == NULL) {
		pkg->pkg_prefix = old_prefix;
		return -1;
	}
	if (old_prefix != NULL)
		free(old_prefix);

	return 0;
}

/**
 * @brief Returns the prefix or NULL
 * @param pkg The package
 * @return the prefix or NULL if it hasn't been set
 */
const char *
pkg_get_prefix(struct pkg *pkg)
{
	if (pkg == NULL)
		return NULL;

	return pkg->pkg_prefix;
}

/**
 * @brief Gets the control files from a given package
 * @param pkg The package
 * 
 * @return A null-terminated array of pkgfile's contining the packages control files
 */
struct pkgfile **
pkg_get_control_files(struct pkg *pkg)
{
	if (!pkg) {
		return NULL;
	}

	if (!pkg->pkg_get_control_files) {
		return NULL;
	}

	return pkg->pkg_get_control_files(pkg);
}

/**
 * @brief Gets a given control file from a package
 * @param pkg The package
 * @param pkg_name The name of the file to return
 * @return The control file with the name pkg_name
 */
struct pkgfile *
pkg_get_control_file(struct pkg *pkg, const char *pkg_name)
{
	if (!pkg || !pkg_name)
		return NULL;

	if (pkg->pkg_get_control_file)
		return pkg->pkg_get_control_file(pkg, pkg_name);

	return NULL;
}

/**
 * @brief Gets all the dependencies for a given package
 * @param pkg The package
 *
 * This retrieves an array of packages that the named package depends on.
 * @return A NULL terminated array of packages or NULL;
 */
struct pkg **
pkg_get_dependencies(struct pkg *pkg)
{
	if (!pkg)
		return NULL;

	assert(pkg->pkg_get_deps == NULL ||
	    pkg->pkg_get_deps != pkg->pkg_get_rdeps);
	if (pkg->pkg_get_deps)
		return pkg->pkg_get_deps(pkg);
	return NULL;
}

/**
 * @brief Gets all the packages that depend on the named package
 * @param pkg The package
 *
 * This retrieves a list of packages that depend on the
 * given package.
 * If no packages depend on the given package the array
 * will only contain a NULL
 * @return A NULL terminated array of packages
 * @return NULL on error
 */
struct pkg **
pkg_get_reverse_dependencies(struct pkg *pkg)
{
	if (!pkg)
		return NULL;

	assert(pkg->pkg_get_rdeps == NULL ||
	    pkg->pkg_get_deps != pkg->pkg_get_rdeps);
	if (pkg->pkg_get_rdeps != NULL)
		return pkg->pkg_get_rdeps(pkg);

	return NULL;
}

/**
 * @brief Gets the name of a package
 * @param pkg The package
 *
 * @return a sting containing the package name. Do not free this.
 */
const char *
pkg_get_name(struct pkg *pkg)
{
	if (!pkg)
		return NULL;
	return pkg->pkg_name;
}

/**
 * @brief Gets the next file in a package.
 * @param pkg The package
 *
 * Ths is used during the instillation of a package to iterate over
 * all files to be installed in a package
 * @return the next non-control file in the package or NULL when done.
 */
struct pkgfile *
pkg_get_next_file(struct pkg *pkg)
{
	if (!pkg) {
		return NULL;
	}

	if (!pkg->pkg_get_next_file) {
		return NULL;
	}

	return pkg->pkg_get_next_file(pkg);
}

/**
 * @brief Gets a packages origin
 * @param pkg The package
 *
 * This is used to get the package origin from packages that record it.
 * Not all package formats have an origin.
 * @return The packages origin or NULL if it dosn't have one.
 */
const char *
pkg_get_origin(struct pkg *pkg)
{
	if (pkg == NULL)
		return NULL;

	if (pkg->pkg_get_origin)
		return pkg->pkg_get_origin(pkg);

	return NULL;
}

/**
 * @brief Gets a packages origin
 * @param pkg The package
 * @param origin The new origin
 *
 * This is used to set the package's origin.
 * It is intended to be used for empty packages and building a new package.
 * Not all package formats have an origin.
 * @return  0 on success
 * @return -1 on failure
 */
int
pkg_set_origin(struct pkg *pkg, const char *origin)
{
	if (pkg == NULL || origin == NULL)
		return -1;

	if (pkg->pkg_set_origin)
		return pkg->pkg_set_origin(pkg, origin);

	return -1;
}

/**
 * @brief Get the package format version
 * @param pkg The package
 *
 * This retrieves a free form string containing the package format's version.
 * It is intended to be shown to the user rather than be processed.
 * @return A string containing the package format version. Do not free.
 */
const char *
pkg_get_version(struct pkg *pkg)
{
	if (pkg == NULL)
		return NULL;

	if (pkg->pkg_get_version != NULL)
		return pkg->pkg_get_version(pkg);

	return NULL;
}

/**
 * @brief Runs the named script from the package
 * @param pkg The package
 * @param prefix The prefix to pass to the script or NULL for the default
 * @param script The script to run
 * @return The return value of the script, or -1
 */
int
pkg_run_script(struct pkg *pkg, const char *prefix, pkg_script script)
{
	if (pkg == NULL)
		return -1;

	if (pkg->pkg_run_script == NULL)
		return -1;

	return pkg->pkg_run_script(pkg, prefix, script);
}

/**
 * @brief Adds a dependency to a given package
 * @param pkg The package
 * @param depend The package to depend on
 * @return 0 on success, -1 on error.
 */
int
pkg_add_dependency(struct pkg *pkg, struct pkg *depend)
{
	if (!pkg || !depend)
		return -1;

	if (pkg->pkg_add_depend)
		return pkg->pkg_add_depend(pkg, depend);

	return -1;
}

/**
 * @brief Adds a file to a given package
 * @param pkg The package
 * @param file The file to add to the package
 * @return 0 on success, -1 on error.
 */
int
pkg_add_file(struct pkg *pkg, struct pkgfile *file)
{
	if (!pkg || !file)
		return -1;

	if (pkg->pkg_add_file)
		return pkg->pkg_add_file(pkg, file);

	return -1;
}

/**
 * @brief Installs a package using the given callbacks
 * @param pkg The package to install
 * @param prefix The prefix to install the package to or NULL for the default
 * @param reg If true the package will be registered
 * @param pkg_action A callback to display a message to the user
 * @param data A pointer to pass to pkg_db_* callbacks
 * @param db_chdir A callback to change to a new directory
 * @param install_file A callback to install a file in a database
 * @param do_exec A callback to execute a script
 * @param pkg_register A callback to register a package
 *
 * pkg_install() is an internal function to the library.
 * It is designed to seperate the knowledge of packages and
 * package databases apart.
 * The package database now only knows how to install the
 * parts of a package it is given, not the internals of a
 * given package.
 * 
 * @return 0 on success or -1 or error
 */
int
pkg_install(struct pkg *pkg, const char *prefix, int reg,
		pkg_db_action *pkg_action, void *data, pkg_db_chdir *db_chdir,
		pkg_db_install_file *install_file, pkg_db_exec *do_exec,
		pkg_db_register *pkg_register)
{
	if (pkg == NULL || data == NULL || db_chdir == NULL ||
	    install_file == NULL || do_exec == NULL || pkg_register == NULL)
		return -1;

	if (pkg->pkg_install == NULL)
		return -1;

	return pkg->pkg_install(pkg, prefix, reg, pkg_action, data, db_chdir,
	    install_file, do_exec, pkg_register);
}

/**
 * @brief Deinstalls a package using the given callbacks
 * @param pkg The package to deinstall
 * @param pkg_action A callback to display a message to the user
 * @param data A pointer to pass to pkg_db_* callbacks
 * @param db_chdir A callback to change to a new directory
 * @param deinstall_file A callback to install a file in a database
 * @param do_exec A callback to execute a script
 * @param pkg_deregister A callback to register a package
 *
 * pkg_deinstall() is an internal function to the library.
 * It is designed to seperate the knowledge of packages and
 * package databases apart.
 * The package database now only knows how to deinstall the
 * parts of a package it is given, not the internals of a
 * given package.
 * 
 * @return  0 on success
 * @return -1 or error
 */
int
pkg_deinstall(struct pkg *pkg, pkg_db_action *pkg_action, void *data,
		pkg_db_chdir *db_chdir,	pkg_db_install_file *deinstall_file,
		pkg_db_exec *do_exec, pkg_db_deregister *pkg_deregister)
{
	if (pkg == NULL || data == NULL || db_chdir == NULL ||
	    deinstall_file == NULL || do_exec == NULL || pkg_deregister == NULL)
		return -1;

	if (pkg->pkg_deinstall == NULL)
		return -1;

	return pkg->pkg_deinstall(pkg, pkg_action, data, db_chdir,
	    deinstall_file, do_exec, pkg_deregister);
}

/**
 * @brief Frees a NULL terminated array of packages
 * @param pkgs A NULL terminated array of pkg objects
 *
 * This is to be used to free the arrays generated by
 * pkg_get_dependencies()
 * @return 0 on success, -1 on error.
 */
int
pkg_list_free(struct pkg **pkgs)
{
	unsigned int cur;

	if (!pkgs)
		return -1;

	for (cur = 0; pkgs[cur] != NULL; cur++) {
		pkg_free(pkgs[cur]);
	}

	free(pkgs);
	return 0;
}

/**
 * @brief Frees a given package
 * @param pkg The package to free
 * @return 0 on success, -1 on error.
 */
int
pkg_free(struct pkg *pkg)
{
	if (!pkg) {
		return -1;
	}

	if (pkg->pkg_name != NULL)
		free(pkg->pkg_name);

	if (pkg->pkg_prefix != NULL)
		free(pkg->pkg_prefix);

	if (pkg->pkg_free != NULL)
		pkg->pkg_free(pkg);

	free(pkg);

	return 0;
}

/**
 * @}
 */
