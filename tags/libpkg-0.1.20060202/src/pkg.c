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
 * Creates a new package and associates callbacks that are
 * used by most types of packages
 */
struct pkg *
pkg_new(const char *pkg_name,
		pkg_get_control_files_callback *control_files,
		pkg_get_control_file_callback *control_file,
		pkg_get_dependencies_callback *get_deps,
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
	pkg->pkg_free = free_pkg;

	/* Set the other callbacks to NULL */
	pkg->pkg_get_version = NULL;
	pkg->pkg_get_origin = NULL;
	pkg->pkg_add_depend = NULL;
	pkg->pkg_add_file = NULL;
	pkg->pkg_get_next_file = NULL;

	/* The data is unknown so set to NULL */	
	pkg->data = NULL;

	return pkg;
}

/*
 * Internal function to add callbacks to retrieve data
 * eg. the packages origin on FreeBSD
 */
int
pkg_add_callbacks_data(struct pkg *pkg,
		pkg_get_version_callback *get_version,
		pkg_get_origin_callback *get_origin)
{
	if (pkg == NULL)
		return -1;

	pkg->pkg_get_version = get_version;
	pkg->pkg_get_origin = get_origin;
	return 0;
}

/*
 * Internal function to add callbacks that add data to the package
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

/*
 * Internal function to add callbacks that are used when a package is installed
 */
int
pkg_add_callbacks_install (struct pkg *pkg,
		pkg_get_next_file_callback *next_file)
{
	if (pkg == NULL)
		return -1;

	pkg->pkg_get_next_file = next_file;
	return 0;
}

/*
 * Creates an empty package with no callbacks
 */
struct pkg*
pkg_new_empty(const char *pkg_name)
{
	return pkg_new(pkg_name, NULL, NULL, NULL, NULL);
}

/*
 * A function to pass to *sort[_r] to sort alphabeticly by package name
 */
int
pkg_compare(const void *pkg_a, const void *pkg_b)
{
	/* XXX Makes WARNS <= 3 */
	return strcmp((*(const struct pkg **)pkg_a)->pkg_name,
	    (*(const struct pkg **)pkg_b)->pkg_name);
}

/*
 * Gets the control files from a given package
 */
struct pkg_file **
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

/*
 * Gets a given control file from a package
 */
struct pkg_file *
pkg_get_control_file(struct pkg *pkg, const char *pkg_name)
{
	if (!pkg || !pkg_name)
		return NULL;

	if (pkg->pkg_get_control_file)
		return pkg->pkg_get_control_file(pkg, pkg_name);

	return NULL;
}

/*
 * Gets all the dependencies for a given package
 */
struct pkg **
pkg_get_dependencies(struct pkg *pkg)
{
	if (!pkg)
		return NULL;

	if (pkg->pkg_get_deps)
		return pkg->pkg_get_deps(pkg);
	return NULL;
}

/*
 * Gets the name of a package
 */
char *
pkg_get_name(struct pkg *pkg)
{
	if (!pkg)
		return NULL;
	return pkg->pkg_name;
}

/*
 * Gets the next file in a package, used for installation
 * to iterate over all files to be installed in a package
 */
struct pkg_file *
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

/*
 * Gets a packages origin if it has one
 */
char *
pkg_get_origin(struct pkg *pkg)
{
	if (pkg == NULL)
		return NULL;

	if (pkg->pkg_get_origin)
		return pkg->pkg_get_origin(pkg);

	return NULL;
}

/*
 * Return a string containg the package format version
 */
char *
pkg_get_version(struct pkg *pkg)
{
	if (pkg == NULL)
		return NULL;

	if (pkg->pkg_get_version != NULL)
		return pkg->pkg_get_version(pkg);

	return NULL;
}

/*
 * Adds a dependency to a given package
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

/*
 * Adds a file to a given package
 */
int
pkg_add_file(struct pkg *pkg, struct pkg_file *file)
{
	if (!pkg || !file)
		return -1;

	if (pkg->pkg_add_file)
		return pkg->pkg_add_file(pkg, file);

	return -1;
}

/*
 * Frees a NULL terminated array of packages, eg. from pkg_get_dependencies
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

/*
 * Frees a given package
 */
int
pkg_free(struct pkg *pkg)
{
	if (!pkg) {
		return -1;
	}

	if (pkg->pkg_name)
		free(pkg->pkg_name);

	if (pkg->pkg_free)
		pkg->pkg_free(pkg);

	free(pkg);

	return 0;
}
