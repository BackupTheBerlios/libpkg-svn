/*
 * Copyright (C) 2005 Andrew Turner
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

#ifndef __LIBPKG_PKG_H__
#define __LIBPKG_PKG_H__

#include <sys/types.h>	/* uint64_t */
#include <sys/stat.h>	/* struct stat */

#include <stdio.h>	/* FILE */

#ifndef __unused
#define __unused
#endif

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
FILE		*pkgfile_get_fileptr(struct pkgfile *);
const char	*pkgfile_get_type_string(struct pkgfile *);
int		 pkgfile_set_cwd(struct pkgfile *, const char *);
int		 pkgfile_set_checksum_md5(struct pkgfile *, const char *);
int		 pkgfile_compare_checksum_md5(struct pkgfile *);
int		 pkgfile_seek(struct pkgfile *, int64_t, int);
int		 pkgfile_set_mode(struct pkgfile *, mode_t);
int		 pkgfile_append(struct pkgfile *, const char *, uint64_t);
int		 pkgfile_append_string(struct pkgfile *, const char *, ...);
const char	*pkgfile_find_line(struct pkgfile *, const char *);
int		 pkgfile_remove_line(struct pkgfile *, const char *);
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
 * @brief The basic struct to use when interacting with a package
 * @struct pkg pkg.h <pkg.h>
 */
struct pkg;

/**
 * @brief An enum of all possible scripts that can be run by pkg_run_script()
 */
typedef enum _pkg_script {
	pkg_script_noop,		/**< Noop */
	pkg_script_pre,			/**< Pre-install */
	pkg_script_post,		/**< Post-install */
	pkg_script_mtree,		/**< Mtree */
	pkg_script_require,		/**< Requirement check */
	pkg_script_require_deinstall,	/**< Removal Requirement check */
	pkg_script_deinstall,		/**< Deinstall check */
	pkg_script_pre_deinstall,	/**< Pre-removal */
	pkg_script_post_deinstall	/**< Post-removal */
} pkg_script;

struct pkg		 *pkg_new_empty(const char *);
struct pkg		 *pkg_new_freebsd_from_file(FILE *);
struct pkg		 *pkg_new_freebsd_installed(const char *, const char *);
struct pkg		 *pkg_new_freebsd_empty(const char *);
int			  pkg_compare(const void *, const void *);
int			  pkg_set_prefix(struct pkg *, const char *);
const char		 *pkg_get_prefix(struct pkg *);
const char		**pkg_get_conflicts(struct pkg *);
struct pkgfile		**pkg_get_control_files(struct pkg *);
struct pkgfile		 *pkg_get_control_file(struct pkg *, const char *);
struct pkg		**pkg_get_dependencies(struct pkg *);
struct pkg		**pkg_get_reverse_dependencies(struct pkg *);
struct pkg_manifest	 *pkg_get_manifest(struct pkg *);
const char		 *pkg_get_name(struct pkg *);
struct pkgfile		 *pkg_get_next_file(struct pkg *);
const char		 *pkg_get_origin(struct pkg *);
int			  pkg_set_origin(struct pkg *, const char *);
const char		 *pkg_get_version(struct pkg *);
int			  pkg_run_script(struct pkg *, const char *,pkg_script);
int			  pkg_add_dependency(struct pkg *, struct pkg *);
int			  pkg_add_file(struct pkg *, struct pkgfile *);
int			  pkg_list_free(struct pkg **);
int			  pkg_free(struct pkg *);

/**
 * @}
 */

/**
 * @addtogroup PackageManifestItem
 *
 * @{
 */

/**
 * @brief The basic struct to use when describing an item within a package manifest
 * @struct pkg_manifest_item pkg.h <pkg.h>
 */
struct pkg_manifest_item;

/**
 * @brief The type of manifest item this is
 */
typedef enum _pkg_manifest_item_type {
	pmt_error = 0,	/**< An error occured */
	pmt_other,	/**< The item is package format dependent */
	pmt_file,	/**< The item is a file */
	pmt_dir,	/**< The item is a directory */
	pmt_dirlist,	/**< The item is a list of directories and files, eg. mtree */
	pmt_chdir,	/**< The item indicates a new directory to change to */
	pmt_output,	/**< The item indicates some message to display to the user */
	pmt_comment,	/**< The item is a comment */
	pmt_execute	/**< The item is a program to execute */
} pkg_manifest_item_type;

/**
 * @brief Possible attributes that can be set on an item
 */
typedef enum _pkg_manifest_item_attr {
	pmia_other = 0,		/**< Package dependent item */
	pmia_ignore,		/**< Ignore the current item */
	pmia_deinstall,		/**< The item is for deinstall rather than install */
	pmia_md5,		/**< Set the MD5 checksum of an item */
	pmia_max		/**< The largest possible attribute */
} pkg_manifest_item_attr;

struct pkg_manifest_item *pkg_manifest_item_new(pkg_manifest_item_type,
	    const char *);
int	pkg_manifest_item_free(struct pkg_manifest_item *);
pkg_manifest_item_type pkg_manifest_item_get_type(struct pkg_manifest_item *);
const char *pkg_manifest_item_get_attr(struct pkg_manifest_item *,
	    pkg_manifest_item_attr);
const void *pkg_manifest_item_get_data(struct pkg_manifest_item *);
int	pkg_manifest_item_set_attr(struct pkg_manifest_item *,
	    pkg_manifest_item_attr, const char *);
int	pkg_manifest_item_set_data(struct pkg_manifest_item *, const char *);

/**
 * @}
 */

/**
 * @addtogroup PackageManifest
 *
 * @{
 */

/**
 * @brief The basic struct to use when describing with a package manifest
 * @struct pkg_manifest pkg.h <pkg.h>
 */
struct pkg_manifest;

typedef enum _pkg_manifest_attr {
	pkgm_other = 0,	/**< Package dependant attribute */
	pkgm_origin,	/**< The package's origin */
	pkgm_prefix,	/**< Where the package will install files to */
	pkgm_max	/**< The largest attribute */
} pkg_manifest_attr;

struct pkg_manifest	 *pkg_manifest_new(void);
struct pkg_manifest	 *pkg_manifest_new_freebsd_pkgfile(struct pkgfile *);
int			  pkg_manifest_free(struct pkg_manifest *);

const char 		 *pkg_manifest_get_attr(struct pkg_manifest *,
			    pkg_manifest_item_attr);
int			  pkg_manifest_set_manifest_version(
			    struct pkg_manifest *, const char *);
const char		 *pkg_manifest_get_manifest_version(
			    struct pkg_manifest *);
int			  pkg_manifest_add_dependency(struct pkg_manifest *,
			    struct pkg *);
int			  pkg_manifest_replace_dependency(struct pkg_manifest *,
			    struct pkg *, struct pkg *);
struct pkg		**pkg_manifest_get_dependencies(struct pkg_manifest *);
int			  pkg_manifest_add_conflict(struct pkg_manifest *,
			    const char *);
int			  pkg_manifest_set_name(struct pkg_manifest *,
			    const char *);
const char		 *pkg_manifest_get_name(struct pkg_manifest *);
int			  pkg_manifest_set_attr(struct pkg_manifest *,
			    pkg_manifest_attr, const char *);
const char		**pkg_manifest_get_attrs(struct pkg_manifest *);
int			  pkg_manifest_append_item(struct pkg_manifest *,
			    struct pkg_manifest_item *);
const char		**pkg_manifest_get_conflicts(struct pkg_manifest *);
struct pkgfile		 *pkg_manifest_get_file(struct pkg_manifest *);
struct pkg_manifest_item **pkg_manifest_get_items(struct pkg_manifest *);

/**
 * @}
 */

char	*pkg_abspath(const char *);

#endif /* __LIBPKG_PKG_H__ */
