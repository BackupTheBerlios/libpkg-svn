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

struct pkg_file_list {
	struct pkg_object	 pkg_object;

	struct pkg_file_list	*next;
	struct pkg_file		*file;
};

struct pkg_file {
	struct pkg_object	 pkg_object;

	char		*filename;
	uint64_t	 len;
	char		*contents;
	struct stat	*stat;
};

struct pkg {
	struct pkg_object	 pkg_object;

	char	*pkg_name;
	pkg_get_control_files_callback	*pkg_get_control_files;
	pkg_get_next_file_callback	*pkg_get_next_file;
};

struct pkg_db {
	struct pkg_object	 pkg_object;

	char	*db_base;

	pkg_db_install_pkg_callback	*pkg_install;
	pkg_db_is_installed_callback	*pkg_is_installed;
};

struct pkg_repo {
	struct pkg_object	 pkg_object;

	pkg_repo_get_pkg_callback	*pkg_get;
};

int pkg_dir_build(const char *);
int pkg_checksum_md5(struct pkg_file *, char *);

#endif /* __LIBPKG_PKG_PRIVATE_H__ */
