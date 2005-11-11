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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fetch.h>

#include "pkg.h"
#include "pkg_repo.h"
#include "pkg_private.h"
#include "pkg_repo_private.h"

#define MAX_VERSION 9999999
static struct {
        int lowver;     /* Lowest version number to match */
        int hiver;      /* Highest version number to match */
        const char *directory;  /* Directory it lives in */
} releases[] = {
        { 410000, 410000, "packages-4.1-release" },
        { 420000, 420000, "packages-4.2-release" },
        { 430000, 430000, "packages-4.3-release" },
        { 440000, 440000, "packages-4.4-release" },
        { 450000, 450000, "packages-4.5-release" },
        { 460000, 460001, "packages-4.6-release" },
        { 460002, 460099, "packages-4.6.2-release" },
        { 470000, 470099, "packages-4.7-release" },
        { 480000, 480099, "packages-4.8-release" },
        { 490000, 490099, "packages-4.9-release" },
        { 491000, 491099, "packages-4.10-release" },
        { 492000, 492099, "packages-4.11-release" },
        { 500000, 500099, "packages-5.0-release" },
        { 501000, 501099, "packages-5.1-release" },
        { 502000, 502009, "packages-5.2-release" },
        { 502010, 502099, "packages-5.2.1-release" },
        { 503000, 503099, "packages-5.3-release" },
        { 504000, 504099, "packages-5.4-release" },
        { 300000, 399000, "packages-3-stable" },
        { 400000, 499000, "packages-4-stable" },
        { 502100, 502128, "packages-5-current" },
        { 503100, 599000, "packages-5-stable" },
        { 600000, 699000, "packages-6-current" },
        { 700000, 799000, "packages-7-current" },
        { 0, MAX_VERSION, "packages-current" },
        { 0, 0, NULL }
};

struct ftp_repo {
	char	*site;
	char	*path;
	FILE	*fd;
};

int getosreldate(void);

static struct pkg *ftp_get_pkg(struct pkg_repo *, const char *);
static int ftp_free(struct pkg_repo *);

//static int pkg_in_All(const char *);
static int pkg_name_has_extension(const char *);

/*
 * A repo with files on a remote ftp server
 */
struct pkg_repo *
pkg_repo_new_ftp(const char *site, const char *path)
{
	struct pkg_repo *pkg;
	struct ftp_repo *f_repo;

	assert(site != NULL);
	assert(path != NULL);

	pkg = pkg_repo_new(ftp_get_pkg, ftp_free);
	if (!pkg) {
		/* pkg_null will contain the error string */
		return NULL;
	}

	f_repo = malloc(sizeof(struct ftp_repo));
	if (!f_repo) {
		pkg_repo_free(pkg);
		return NULL;
	}

	pkg->data = f_repo;

	/* Figure out the site */
	if (!site)
		f_repo->site = strdup("ftp.freebsd.org");
	else
		f_repo->site = strdup(site);

	if (!f_repo->site) {
		pkg_repo_free(pkg);
		return NULL;
	}

	/* Figure out the path */
	f_repo->path = NULL;
	if (!path) {
		struct utsname	u;
		int		i, reldate;

		reldate = getosreldate();
		if(reldate > MAX_VERSION) {  /* bogus osreldate?? */
			pkg_repo_free(pkg);
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

	} else
		f_repo->path = strdup(path);

	if (!f_repo->path) {
		pkg_repo_free(pkg);
		return NULL;
	}

	return pkg;
}

static struct pkg *
ftp_get_pkg(struct pkg_repo *repo, const char *pkg_name)
{
	FILE *fd;
	struct pkg *pkg;
	struct ftp_repo *f_repo;
	char *ftpname;
	const char	*subdir;
	const char	*ext;
	const char	*fallback_subdir;

	if (!repo) {
		return NULL;
	}

	if (!pkg_name) {
		return NULL;
	}

	f_repo = repo->data;
	if (!f_repo) {
		return NULL;
	}

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

	asprintf(&ftpname, "ftp://%s/%s/%s/%s%s", f_repo->site, f_repo->path,
	    subdir, pkg_name, ext);
	if (!ftpname) {
		return NULL;
	}

	fd = fetchGetURL(ftpname, "p");

	/* Try the alternate subdir if the primary one fails. */
	if (fd == NULL) {
		free(ftpname);
		asprintf(&ftpname, "ftp://%s/%s/%s/%s%s", f_repo->site,
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

	pkg = pkg_new_freebsd(fd);
	if (!pkg) {
		fclose(fd);
		return NULL;
	}

	return pkg;
}

/*
 * Free the struct ftp_repo
 */
static int
ftp_free(struct pkg_repo *repo)
{
	struct ftp_repo *f_repo;

	if (!repo) {
		return -1;
	}

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
