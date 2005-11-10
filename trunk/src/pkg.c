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
		pkg_free_callback *free_pkg)
{
	struct pkg *pkg;

	pkg = malloc(sizeof(struct pkg));
	if (!pkg) {
		pkg_error_set(&pkg_null, "Out of Memory");
		return NULL;
	}

	pkg->pkg_name = strdup(name);
	if (!pkg->pkg_name) {
		free(pkg);
		pkg_error_set(&pkg_null, "Out of Memory");
		return NULL;
	}

	pkg->pkg_get_control_files = control_files;
	pkg->pkg_get_next_file = next_file;
	pkg->pkg_free = free_pkg;

	pkg->pkg_object.data = NULL;
	pkg->pkg_object.error_str = NULL;
	pkg->pkg_object.free = NULL;

	return pkg;
}

struct pkg_list *
pkg_get_control_files(struct pkg *pkg)
{
	if (!pkg) {
		pkg_error_set(&pkg_null, "No package specified");
		return NULL;
	}

	if (!pkg->pkg_get_control_files) {
		pkg_error_set((struct pkg_object*)pkg, "Package contains no control files");
		return NULL;
	}
	if (pkg->pkg_object.error_str) {
		free(pkg->pkg_object.error_str);
		pkg->pkg_object.error_str = NULL;
	}

	return pkg->pkg_get_control_files(pkg);
}

struct pkg_file *
pkg_get_next_file(struct pkg *pkg)
{
	if (!pkg) {
		pkg_error_set(&pkg_null, "No package specified");
		return NULL;
	}

	if (!pkg->pkg_get_next_file) {
		pkg_error_set((struct pkg_object*)pkg, "No more files in list");
		return NULL;
	}

	return pkg->pkg_get_next_file(pkg);
}

int
pkg_free(struct pkg *pkg)
{
	if (!pkg) {
		pkg_error_set(&pkg_null, "No package specified");
		return PKG_FAIL;
	}

	if (pkg->pkg_name)
		free(pkg->pkg_name);

	if (pkg->pkg_free)
		pkg->pkg_free(pkg);

	free(pkg);

	return PKG_OK;
}
