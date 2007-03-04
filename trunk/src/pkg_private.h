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
#include "pkg_db.h"

int archive_read_open_stream(struct archive *, FILE *, size_t);

/* Package file location */
typedef enum {
	pkgfile_loc_disk,
	pkgfile_loc_mem
} pkgfile_loc;

/**
 * @brief The type of file the pkgfile object is interacting with
 */
typedef enum {
	pkgfile_none, /**< No file */
	pkgfile_regular, /**< A regular file */
	pkgfile_hardlink, /**< A hard link */
	pkgfile_symlink, /**< A symlink */
	pkgfile_dir /**< A directory */
} pkgfile_type;

/** @todo Reorder the struct to remove alignment gaps */
struct pkgfile {
	char		*name;
	char		*cwd;
	char		*real_name;
	pkgfile_type	 type;
	pkgfile_loc	 loc;
	int		 follow_link;
	FILE		*fd;
	char		*data;
	uint64_t	 length;
	uint64_t	 offset;
	mode_t		 mode;
	char		 md5[33];
};

/*
 * Package Object
 */

/* Main callbacks used in most packages */
typedef struct pkg	**pkg_get_dependencies_callback(struct pkg *);
typedef struct pkgfile	**pkg_get_control_files_callback(struct pkg *);
typedef struct pkgfile   *pkg_get_control_file_callback(struct pkg *,
				const char *);
typedef int		  pkg_free_callback(struct pkg *);

struct pkg		 *pkg_new(const char *,
				pkg_get_control_files_callback *,
				pkg_get_control_file_callback *,
				pkg_get_dependencies_callback *,
				pkg_get_dependencies_callback *,
				pkg_free_callback *);

/* Callbacks to get data from a package, eg. the description */
typedef const char	 *pkg_get_version_callback(struct pkg *);
typedef const char	 *pkg_get_origin_callback(struct pkg *);
int			  pkg_add_callbacks_data(struct pkg *,
				pkg_get_version_callback *,
				pkg_get_origin_callback *);

/* Callbacks used with empty packages to add files to */
typedef int		  pkg_add_dependency_callback(struct pkg *,
				struct pkg *);
typedef int		  pkg_add_file_callback(struct pkg *,
				struct pkgfile *);
int			  pkg_add_callbacks_empty(struct pkg *,
				pkg_add_dependency_callback *,
				pkg_add_file_callback *);

/* Callbacks used with installable packages. Used by pkg_repo */
typedef int	  	  pkg_db_chdir(struct pkg *, pkg_db_action *, void *,
				const char *);
typedef int		  pkg_db_install_file(struct pkg *, pkg_db_action *,
				void *, struct pkgfile *);
typedef int		  pkg_db_exec(struct pkg *, pkg_db_action *, void *,
				const char *);
typedef int		  pkg_db_register(struct pkg *, pkg_db_action *, void *,
				struct pkgfile **, const char *);
typedef int		  pkg_db_deregister(struct pkg *, pkg_db_action *,
				void *,	struct pkgfile **);
typedef int	  	  pkg_install_callback(struct pkg *, const char *, int,
				pkg_db_action *, void *, pkg_db_chdir *,
				pkg_db_install_file *, pkg_db_exec *,
				pkg_db_register *);
typedef int		  pkg_deinstall_callback(struct pkg *, pkg_db_action *,
				void *, pkg_db_chdir *, pkg_db_install_file *,
				pkg_db_exec *, pkg_db_deregister *);
typedef struct pkgfile	 *pkg_get_next_file_callback(struct pkg *);
typedef int		  pkg_run_script_callback(struct pkg *, const char *,
				pkg_script);

int			  pkg_add_callbacks_install(struct pkg *,
				pkg_install_callback *,
				pkg_deinstall_callback *,
				pkg_get_next_file_callback *,
				pkg_run_script_callback *);
int			  pkg_install(struct pkg *, const char *, int,
				pkg_db_action *, void *, pkg_db_chdir *,
				pkg_db_install_file *, pkg_db_exec *,
				pkg_db_register *);
int			  pkg_deinstall(struct pkg *,
				pkg_db_action *, void *, pkg_db_chdir *,
				pkg_db_install_file *, pkg_db_exec *,
				pkg_db_deregister *);

struct pkg {
	void	*data;

	char	*pkg_name;
	char	*pkg_prefix;

	/* Main callbacks */
	pkg_get_control_files_callback	*pkg_get_control_files;
	pkg_get_control_file_callback	*pkg_get_control_file;
	pkg_get_dependencies_callback	*pkg_get_deps;
	pkg_get_dependencies_callback	*pkg_get_rdeps;
	pkg_free_callback		*pkg_free;

	pkg_get_version_callback	*pkg_get_version;
	pkg_get_origin_callback		*pkg_get_origin;

	/* Callbacks usally used with empty packages */
	pkg_add_dependency_callback	*pkg_add_depend;
	pkg_add_file_callback		*pkg_add_file;

	/* Callbacks used with (de)installing packages */
	pkg_install_callback		*pkg_install;
	pkg_deinstall_callback		*pkg_deinstall;
	pkg_get_next_file_callback	*pkg_get_next_file;
	pkg_run_script_callback		*pkg_run_script;
};

int pkg_dir_build(const char *, mode_t);
int pkg_dir_clean(const char *);
int pkg_exec(const char *, ...);
FILE *pkg_cached_file(FILE *, const char *);

/* 
 * Remove extra slashes from the path
 * The first is slower
 * Valgrind complains on the second
 */
#ifdef FOR_VALGRIND
#define pkg_remove_extra_slashes(path) \
	{ \
		char *str_a, *str_b; \
		str_a = path + 1; \
		str_b = str_a; \
		while(*str_a != '\0') { \
			while(str_b[0] == '/' && str_b[-1] == '/') \
				str_b++; \
			if (str_a != str_b) \
				str_a[0] = str_b[0]; \
			str_a++; str_b++; \
		} \
	}
#else
#define pkg_remove_extra_slashes(path) \
	{ \
		char *tmp_str; \
		while((tmp_str = strstr(path, "//")) != NULL) { \
			strcpy(tmp_str, tmp_str+1); \
		} \
	}
#endif


#endif /* __LIBPKG_PKG_PRIVATE_H__ */
