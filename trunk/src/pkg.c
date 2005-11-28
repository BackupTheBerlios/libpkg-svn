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

struct pkg *
pkg_new(const char *name, 
		pkg_get_control_files_callback *control_files,
		pkg_get_next_file_callback *next_file,
		pkg_get_dependencies_callback *get_deps,
		pkg_free_callback *free_pkg)
{
	struct pkg *pkg;

	if (name == NULL)
		return NULL;
	
	pkg = malloc(sizeof(struct pkg));
	if (!pkg) {
		return NULL;
	}

	pkg->pkg_name = strdup(name);
	if (!pkg->pkg_name) {
		free(pkg);
		return NULL;
	}

	pkg_set_callbacks(pkg, control_files, next_file, get_deps, free_pkg);

	pkg->data = NULL;

	return pkg;
}

struct pkg*
pkg_new_empty(const char *name)
{
	return pkg_new(name, NULL, NULL, NULL, NULL);
}

struct pkg *
pkg_set_callbacks(struct pkg *pkg, 
		pkg_get_control_files_callback *control_files,
		pkg_get_next_file_callback *next_file,
		pkg_get_dependencies_callback *get_deps,
		pkg_free_callback *free_pkg)
{
	pkg->pkg_get_control_files = control_files;
	pkg->pkg_get_next_file = next_file;
	pkg->pkg_get_deps = get_deps;
	pkg->pkg_free = free_pkg;

	return pkg;
}

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

struct pkg **
pkg_get_dependencies(struct pkg *pkg)
{
	if (!pkg)
		return NULL;

	if (pkg->pkg_get_deps)
		return pkg->pkg_get_deps(pkg);
	return NULL;
}

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

char *
pkg_get_name(struct pkg *pkg)
{
	if (!pkg)
		return NULL;
	return pkg->pkg_name;
}
