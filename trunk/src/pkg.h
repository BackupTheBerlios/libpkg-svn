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

#ifndef __LIBPKG_PKG_H__
#define __LIBPKG_PKG_H__

#include <sys/types.h>	/* uint64_t */
#include <sys/stat.h>	/* struct stat */

#include <stdio.h>	/* FILE */

/*
 * Generic object for all other objects.
 * It must be named pkg_object
 */
struct pkg_object;

/* This is used to call the individual callback */
typedef int	pkg_object_free_callback(struct pkg_object *);

/* This must be the first item in child structs so we know where it is. */
struct pkg_object {
	/* Object internal data */
	void				*data;
	pkg_object_free_callback	*free;
};

int	pkg_object_free(struct pkg_object *);

/*
 * Object to hold files in
 */
struct pkg_file;

struct pkg_file	*pkg_file_new_from_buffer(const char *, uint64_t, char *,
			const struct stat *);
int		 pkg_file_free(struct pkg_file *);
int		 pkg_file_write(struct pkg_file *);

struct pkg_list	*pkg_file_list_add(struct pkg_list *, struct pkg_file *);
struct pkg_file	*pkg_file_list_get_file(struct pkg_list *,
				const char *);

/*
 * Object to hold a collection of packages in
 */
struct pkg_list;

struct pkg_list	*pkg_list_add(struct pkg_list *, struct pkg_object *);
int		 pkg_list_free(struct pkg_list *);

/*
 * The package handling functions
 */
struct pkg;

struct pkg		*pkg_new_freebsd(FILE *);
struct pkg_list		*pkg_get_dependencies(struct pkg *);
/*
 * Returns all control files from the package
 * Eg. +CONTENTS from FreeBSD Packages
 */
struct pkg_file		**pkg_get_control_files(struct pkg *);
/* Returns the next non-control file */
struct pkg_file		*pkg_get_next_file(struct pkg *);
int			 pkg_free(struct pkg *);

#endif /* __LIBPKG_PKG_H__ */
