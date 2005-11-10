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

#include <stdio.h>	/* FILE * */

#define PKG_OK		0 /* Success */
#define PKG_YESNO	3
#define PKG_NO		2 /* PKG_NO & PKG_YESNO == FALSE */
#define PKG_YES		3 /* PKG_YES & PKG_YESNO == TRUE */
#define PKG_NOTSUP	4 /* Unsupported function */
#define PKG_FAIL	5 /* Failed */

/*
 * Generic object for all other objects.
 * It must be named pkg_object
 */
struct pkg_object;

/* This is used to call the individual callback */
typedef int	pkg_object_free_callback(struct pkg_object *);

/* This must be the first item in child structs so we know where it is. */
struct pkg_object {
	/* The Error string for the user */
	char				*error_str;
	/* Object internal data */
	void				*data;
	pkg_object_free_callback	*free;
};

int	pkg_object_free(struct pkg_object *);

/* This is the struct to read the error from when NULL is returned */
extern struct pkg_object pkg_null;

int	 pkg_error_set(struct pkg_object *, const char *, ...);
char	*pkg_error_string(struct pkg_object *);

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
 * The package handling functions
 */
struct pkg;

typedef struct pkg_list	*pkg_get_control_files_callback(struct pkg *);
typedef struct pkg_file	*pkg_get_next_file_callback(struct pkg *);
typedef int		 pkg_free_callback(struct pkg *);

struct pkg		*pkg_new(const char *,
				pkg_get_control_files_callback *,
				pkg_get_next_file_callback *,
				pkg_free_callback *);
struct pkg		*pkg_new_freebsd(FILE *);

/*
 * Object to hold a collection of packages in
 */
struct pkg_list;

struct pkg_list	*pkg_list_add(struct pkg_list *, struct pkg_object *);
int		 pkg_list_free(struct pkg_list *);

/*
 * Returns all control files from the package
 * Eg. +CONTENTS from FreeBSD Packages
 */
struct pkg_list	*pkg_get_control_files(struct pkg *);
/* Returns the next non-control file */
struct pkg_file	*pkg_get_next_file(struct pkg *);
int		 pkg_free(struct pkg *);

/*
 * A place to install packages to and uninstall packages from
 */
struct pkg_db;

typedef int	 pkg_db_install_pkg_callback(struct pkg_db *, struct pkg *);
typedef int 	 pkg_db_is_installed_callback(struct pkg_db *, const char *);

struct pkg_db	*pkg_db_open(const char *, pkg_db_install_pkg_callback *,
			pkg_db_is_installed_callback *);
struct pkg_db	*pkg_db_open_freebsd(const char *);
int		 pkg_db_install_pkg(struct pkg_db *, struct pkg *);
int		 pkg_db_is_installed(struct pkg_db *, const char *);
int		 pkg_db_free(struct pkg_db *);

/*
 * A Repo is a store of 0 or more packages.
 * eg. ftp server, cdrom, local directory.
 */
struct pkg_repo;

typedef int	 pkg_repo_mark_callback(struct pkg_repo *, const char *);
typedef int	 pkg_repo_unmark_callback(struct pkg_repo *, const char *);
typedef int	 pkg_repo_install_callback(struct pkg_repo *, struct pkg_db *);
typedef struct pkg *pkg_repo_get_pkg_callback(struct pkg_repo *, const char *);
typedef int	 pkg_repo_free_callback(struct pkg_repo *);

struct pkg_repo	*pkg_repo_new(pkg_repo_get_pkg_callback *,
			pkg_repo_free_callback *);
struct pkg_repo	*pkg_repo_new_files(void);
struct pkg_repo	*pkg_repo_new_ftp(const char *, const char *);
struct pkg	*pkg_repo_get_pkg(struct pkg_repo *, const char *);
int		 pkg_repo_free(struct pkg_repo *);

#endif /* __LIBPKG_PKG_H__ */
