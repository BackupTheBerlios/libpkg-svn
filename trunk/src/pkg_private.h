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

#ifndef __LIBPKG_PKG_PRIVATE_H__
#define __LIBPKG_PKG_PRIVATE_H__

#include <archive.h>

int archive_read_open_stream(struct archive *, FILE *, size_t);

struct pkg_file {
	char		*filename;
	uint64_t	 len;
	char		*contents;
	struct stat	*stat;
	FILE		*fd;
};

/*
 * Package Object
 */

/* Main callbacks used in most packages */
typedef struct pkg	**pkg_get_dependencies_callback(struct pkg *);
typedef struct pkg_file	**pkg_get_control_files_callback(struct pkg *);
typedef struct pkg_file  *pkg_get_control_file_callback(struct pkg *,
				const char *);
typedef int		  pkg_free_callback(struct pkg *);

struct pkg		 *pkg_new(const char *,
				pkg_get_control_files_callback *,
				pkg_get_control_file_callback *,
				pkg_get_dependencies_callback *,
				pkg_free_callback *);

/* Callbacks to get data from a package, eg. the description */
typedef char		 *pkg_get_origin_callback(struct pkg *);
int			  pkg_add_callbacks_data(struct pkg *,
				pkg_get_origin_callback *);

/* Callbacks used with empty packages to add files to */
typedef int		  pkg_add_dependency_callback(struct pkg *,
				struct pkg *);
typedef int		  pkg_add_file_callback(struct pkg *,
				struct pkg_file *);
int			  pkg_add_callbacks_empty(struct pkg *,
				pkg_add_dependency_callback *,
				pkg_add_file_callback *);

/* Callbacks used with installable packages. Used by pkg_repo */
typedef struct pkg_file	 *pkg_get_next_file_callback(struct pkg *);
int			  pkg_add_callbacks_install(struct pkg *,
				pkg_get_next_file_callback *);

struct pkg {
	void	*data;

	char	*pkg_name;

	/* Main callbacks */
	pkg_get_control_files_callback	*pkg_get_control_files;
	pkg_get_control_file_callback	*pkg_get_control_file;
	pkg_get_dependencies_callback	*pkg_get_deps;
	pkg_free_callback		*pkg_free;

	pkg_get_origin_callback		*pkg_get_origin;

	/* Callbacks usally used with empty packages */
	pkg_add_dependency_callback	*pkg_add_depend;
	pkg_add_file_callback		*pkg_add_file;

	/* Callbacks used with installing packages */
	pkg_get_next_file_callback	*pkg_get_next_file;
};

int pkg_dir_build(const char *);
int pkg_checksum_md5(struct pkg_file *, char *);

#endif /* __LIBPKG_PKG_PRIVATE_H__ */
