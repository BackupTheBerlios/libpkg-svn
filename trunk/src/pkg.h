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

/**
 * @addtogroup PackageFile
 *
 * @{
 */

/**
 * @brief The struct to use to interact with files in a safe way
 * @struct pkgfile pkg.h <pkg.h>
 */
struct pkgfile;

struct pkgfile	*pkgfile_new_from_disk(const char *, int);
struct pkgfile	*pkgfile_new_regular(const char *, const char *, uint64_t);
struct pkgfile	*pkgfile_new_symlink(const char *, const char *);
struct pkgfile	*pkgfile_new_hardlink(const char *, const char *);
struct pkgfile	*pkgfile_new_directory(const char *);
const char	*pkgfile_get_name(struct pkgfile *);
uint64_t	 pkgfile_get_size(struct pkgfile *);
const char	*pkgfile_get_data(struct pkgfile *);
int		 pkgfile_set_checksum_md5(struct pkgfile *, const char *);
int		 pkgfile_compare_checksum_md5(struct pkgfile *);
int		 pkgfile_seek(struct pkgfile *, uint64_t, int);
int		 pkgfile_set_mode(struct pkgfile *, mode_t);
int		 pkgfile_write(struct pkgfile *);
int		 pkgfile_unlink(struct pkgfile *);
int		 pkgfile_free(struct pkgfile *);

/**
 * @}
 */

/**
 * @addtogroup Package
 *
 * @{
 */

/*
 * The package handling functions
 */

/**
 * @brief The basic struct to use when interacting with a Package
 * @struct pkg pkg.h <pkg.h>
 */
struct pkg;

/**
 * @brief An enum of all possible scripts that can be run by pkg_run_script()
 */
typedef enum {
	pkg_script_noop, /**< Noop */
	pkg_script_pre, /**< Pre-install */
	pkg_script_post, /**< Post-install */
	pkg_script_mtree, /**< Mtree */
	pkg_script_require, /**< Requirement check */
	pkg_script_require_deinstall, /**< Removal Requirement check */
	pkg_script_deinstall, /**< Deinstall check */
	pkg_script_pre_deinstall, /**< Pre-removal */
	pkg_script_post_deinstall /**< Post-removal */
} pkg_script;

struct pkg		 *pkg_new_empty(const char *);
struct pkg		 *pkg_new_freebsd_from_file(FILE *);
struct pkg		 *pkg_new_freebsd_installed(const char *, const char *);
#ifndef DOXYGEN_SHOULD_SKIP_THIS
struct pkg		 *pkg_new_freebsd_empty(const char *);
#endif
int			  pkg_compare(const void *, const void *);
int			  pkg_set_prefix(struct pkg *, const char *);
const char		 *pkg_get_prefix(struct pkg *);
struct pkgfile		**pkg_get_control_files(struct pkg *);
struct pkgfile		 *pkg_get_control_file(struct pkg *, const char *);
struct pkg		**pkg_get_dependencies(struct pkg *);
struct pkg		**pkg_get_reverse_dependencies(struct pkg *);
const char		 *pkg_get_name(struct pkg *);
struct pkgfile		 *pkg_get_next_file(struct pkg *);
const char		 *pkg_get_origin(struct pkg *);
const char		 *pkg_get_version(struct pkg *);
int			  pkg_run_script(struct pkg *, const char *,pkg_script);
int			  pkg_add_dependency(struct pkg *, struct pkg *);
int			  pkg_add_file(struct pkg *, struct pkgfile *);
int			  pkg_list_free(struct pkg **);
int			  pkg_free(struct pkg *);

/**
 * @}
 */
char	*pkg_abspath(const char *);

#endif /* __LIBPKG_PKG_H__ */
