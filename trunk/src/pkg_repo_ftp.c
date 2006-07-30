/*
 * Copyright (C) 2004, Tim Kientzle, All rights Reserved.
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

#include <sys/param.h>
#include <sys/utsname.h>

#include <assert.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fetch.h>

#include "pkg.h"
#include "pkg_repo.h"
#include "pkg_freebsd.h"
#include "pkg_private.h"
#include "pkg_repo_private.h"

#define MAX_VERSION 9999999
static const struct {
        int lowver;     /* Lowest version number to match */
        int hiver;      /* Highest version number to match */
        const char *directory;  /* Directory it lives in */
} releases[] = {
	{ 410000, 410000, "/packages-4.1-release" },
	{ 420000, 420000, "/packages-4.2-release" },
	{ 430000, 430000, "/packages-4.3-release" },
	{ 440000, 440000, "/packages-4.4-release" },
	{ 450000, 450000, "/packages-4.5-release" },
	{ 460000, 460001, "/packages-4.6-release" },
	{ 460002, 460099, "/packages-4.6.2-release" },
	{ 470000, 470099, "/packages-4.7-release" },
	{ 480000, 480099, "/packages-4.8-release" },
	{ 490000, 490099, "/packages-4.9-release" },
	{ 491000, 491099, "/packages-4.10-release" },
	{ 492000, 492099, "/packages-4.11-release" },
	{ 500000, 500099, "/packages-5.0-release" },
	{ 501000, 501099, "/packages-5.1-release" },
	{ 502000, 502009, "/packages-5.2-release" },
	{ 502010, 502099, "/packages-5.2.1-release" },
	{ 503000, 503099, "/packages-5.3-release" },
	{ 504000, 504099, "/packages-5.4-release" },
	{ 600000, 600099, "/packages-6.0-release" },
	{ 300000, 399000, "/packages-3-stable" },
	{ 400000, 499000, "/packages-4-stable" },
	{ 502100, 502128, "/packages-5-current" },
	{ 503100, 599000, "/packages-5-stable" },
	{ 600100, 699000, "/packages-6-stable" },
	{ 700000, 799000, "/packages-7-current" },
        { 0, MAX_VERSION, "packages-current" },
        { 0, 0, NULL }
};

struct ftp_repo {
	char	*site;
	char	*path;
	int	 cache;
	char	 cache_dir[MAXPATHLEN];
};

int getosreldate(void);

/* Callbacks */
pkg_static struct pkg	*ftp_get_pkg(struct pkg_repo *, const char *);
pkg_static int		 ftp_free(struct pkg_repo *);
/* Internal */
pkg_static FILE		*ftp_get_fd(const char *, struct ftp_repo *);
pkg_static struct ftp_repo	*ftp_create_repo(const char *, const char *,
					const char *);
/*pkg_static int		 pkg_in_All(const char *); */
pkg_static int		 pkg_name_has_extension(const char *);

/**
 * @defgroup PackageRepoFtp FTP package repository
 * @ingroup PackageRepo
 *
 * @{
 */

/**
 * @brief Creates a pkg_repo with the given sie and path
 * @param site The ftp site to use. If NULL will use ftp.freebsd.org
 * @param path The path to the top level of the packages
 * @param cached_dir The directory to save a copy of each package file in or
 *     NULL. If NULL will use the default path
 * @return A pkg_repo object or NULL
 */
struct pkg_repo *
pkg_repo_new_ftp(const char *site, const char *path, const char *cache_dir)
{
	struct pkg_repo *repo;

	repo = pkg_repo_new(ftp_get_pkg, ftp_free);
	if (!repo) {
		return NULL;
	}
	
	repo->data = ftp_create_repo(site, path, cache_dir);
	if (!repo->data) {
		ftp_free(repo);
		return NULL;
	}

	return repo;
}

/**
 * @}
 */

/**
 * @defgroup PackageRepoFtpCallback FTP package repository callbacks
 * @ingroup PackageRepoFtp
 *
 * @{
 */

/**
 * @brief Callback for pkg_repo_get_pkg()
 * @return The requested package or NULL
 */
static struct pkg *
ftp_get_pkg(struct pkg_repo *repo, const char *pkg_name)
{
	FILE *fd, *fd2;
	struct pkg *pkg;
	struct ftp_repo *f_repo;

	assert(repo != NULL);
	assert(pkg_name != NULL);

	f_repo = repo->data;
	assert(f_repo != NULL);

	fd = ftp_get_fd(pkg_name, f_repo);
	if (fd == NULL)
		return NULL;

	if (f_repo->cache) {
		char cache_file[FILENAME_MAX];

		fd2 = fd;
		snprintf(cache_file, FILENAME_MAX, "%s/%s.tbz",
		    f_repo->cache_dir, pkg_name);
		fd = pkg_cached_file(fd2, cache_file);
	}	
	pkg = pkg_new_freebsd_from_file(fd);
	if (!pkg) {
		fclose(fd);
		return NULL;
	}

	return pkg;
}

/**
 * @brief Callback for pkg_repo_free()
 * @return 0 always
 */
static int
ftp_free(struct pkg_repo *repo)
{
	struct ftp_repo *f_repo;

	assert(repo != NULL);

	f_repo = repo->data;

	/* If there is no repo we don't need to free it */
	if (!f_repo)
		return 0;

	if (f_repo->site)
		free(f_repo->site);

	if (f_repo->path)
		free(f_repo->path);

	free(f_repo);

	return 0;
}

/**
 * @}
 */

/**
 * @defgroup PackageRepoFtpInternal FTP package repository Internal functions
 * @ingroup PackageRepoFtp
 *
 * @{
 */

/**
 * @brief Retrieves a FILE pointer for a given package name
 * @return A FILE pointer to get a package with fetch(3)
 */
static FILE *
ftp_get_fd(const char *pkg_name, struct ftp_repo *f_repo)
{
	const char *subdir;
	const char *fallback_subdir;
	const char *ext;
	char *ftpname;
	FILE *fd;

	/*
	 * Figure out what order to look for the package
	 */
	//if (pkg_in_All(pkg_name)) {
		subdir = "All";
		fallback_subdir = "Latest";
	//} else {
	//	subdir = "Latest";
	//	fallback_subdir = "All";
	//}

	/* Get the extension */
	if (pkg_name_has_extension(pkg_name))
		ext = "";
	else
		ext = ".tbz";

	asprintf(&ftpname, "%s/%s/%s/%s%s", f_repo->site, f_repo->path,
	    subdir, pkg_name, ext);
	if (!ftpname) {
		return NULL;
	}

	fd = fetchGetURL(ftpname, "p");

	/* Try the alternate subdir if the primary one fails. */
	if (fd == NULL) {
		free(ftpname);
		asprintf(&ftpname, "%s/%s/%s/%s%s", f_repo->site,
		    f_repo->path, fallback_subdir, pkg_name, ext);
		if (!ftpname) {
			return NULL;
		}
		fd = fetchGetURL(ftpname, "p");
		if (fd == NULL) {
			free(ftpname);
			return NULL;
		}
	}

	free(ftpname);

	return fd;
}

/**
 * @brief Creates a ftp_repo object for repo->data
 * @todo Free the object at all failure points
 * @return A ftp_repo object or NULL
 */
static struct ftp_repo *
ftp_create_repo(const char *site, const char *path, const char *cache_dir)
{
	struct ftp_repo *f_repo;

	f_repo = malloc(sizeof(struct ftp_repo));
	if (!f_repo) {
		return NULL;
	}

	/* Figure out the site */
	if (!site)
		f_repo->site = strdup("ftp://ftp.freebsd.org");
	else
		f_repo->site = strdup(site);

	if (!f_repo->site) {
		return NULL;
	}

	/* Figure out the path */
	f_repo->path = NULL;
	if (!path) {
		struct utsname	u;
		int		i, reldate;

		reldate = getosreldate();
		if(reldate > MAX_VERSION) {  /* bogus osreldate?? */
			return NULL;
		}

		uname(&u);

		/* Find the correct path from reldate */
		for(i = 0; releases[i].directory != NULL; i++) {
			if (reldate >= releases[i].lowver &&
			    reldate <= releases[i].hiver) {
				asprintf(&f_repo->path, "pub/FreeBSD/ports/%s/%s", u.machine,
				    releases[i].directory);
				break;
			}
		}

	} else {
		/* If the path ends with Latest or All strip it out */
		const char *last_dir;
		last_dir = basename(path);
		if (strcmp(last_dir, "All") == 0 ||
		    strcmp(last_dir, "Latest") == 0) {
			f_repo->path = strdup(dirname(path));
			printf("%s\n", f_repo->path);
		} else
			f_repo->path = strdup(path);
	}

	if (!f_repo->path) {
		return NULL;
	}

	f_repo->cache = 0;
	if (cache_dir != NULL) {
		f_repo->cache = 1;
		snprintf(f_repo->cache_dir, MAXPATHLEN, "%s", cache_dir);
	}

	return f_repo;
}

/**
 * @brief Find if a name has a known extension
 * @todo Return 0 and -1 like other functions
 * @return 1 if name ends with ".t[bg]z", otherwise 0
 */
static int
pkg_name_has_extension(const char *name)
{
	const char	*p;

	p = strrchr(name, '.');
	if (p == NULL)
		return (0);
	if (strcmp(p, ".tbz")==0)
		return (1);
	if (strcmp(p, ".tgz")==0)
		return (1);
	return (0);
}

/**
 * @}
 */
