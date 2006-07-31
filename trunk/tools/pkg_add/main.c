/*
 *
 * FreeBSD install - a package for the installation and maintainance
 * of non-core utilities.
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
 * Jordan K. Hubbard
 * 18 July 1993
 *
 * This is the add module.
 */

#include <sys/param.h>

#include <assert.h>
#include <err.h>
#include <pkg.h>
#include <pkg_db.h>
#include <pkg_repo.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define verbosity_flag		(1)
#define keep_file_flag		(1<<1)
#define no_run_flag		(1<<2)
#define force_flag		(1<<3)
#define no_run_script_flag	(1<<4)
#define no_record_install_flag	(1<<5)

struct pkg_add {
	struct pkg_db	 *db;
	struct pkg_repo	 *repo;
	struct pkg	**pkgs;
	int		  flags;
	char		  chroot[PATH_MAX];
	char		 *base_prefix;
	char		 *prefix;
};

/* A linked list of packages that have been installed */
struct pkg_list {
	const char *name;
	struct pkg_list *next;
};

struct pkg_list *head = NULL;

static char options[] = "hvIRfnrp:P:SMt:C:K";

static void usage(void);
static int pkg_add(struct pkg_add);
static int install_package(struct pkg *, struct pkg_repo *,struct pkg_db *,
	const char *, const char *, int);

int
main (int argc, char *argv[])
{
	char ch;
	struct pkg_add add;
	int ret, i;
	int remote = 0;

	add.db = NULL;
	add.repo = NULL;
	add.flags = 0;
	add.chroot[0] = '\0';
	add.base_prefix = NULL;
	add.prefix = NULL;
	while ((ch = getopt(argc, argv, options)) != -1) {
		switch(ch) {
		/* Case statements marked TODO will be supported in the next release */
		case 'C':
			strlcpy(add.chroot, optarg, PATH_MAX);
			break;
		case 'f':
			add.flags |= force_flag;
			break;
		case 'I':
			add.flags |= no_run_script_flag;
			errx(1, "Unsupported argument");
			break;
		case 'K':
			/* Save the package file in . or ${PKGDIR} */
			add.flags |= keep_file_flag;
			break;
		case 'M':
			errx(1, "Unsupported argument");
			break;
		case 'n':
			add.flags |= no_run_flag;
			break;
		case 'P':
			add.base_prefix = optarg;
			add.prefix = optarg;
			break;
		case 'p':
			add.base_prefix = optarg;
			add.prefix = NULL;
			break;
		case 'R':
			add.flags |= no_record_install_flag;
			break;
		case 'r':
			remote = 1;
			break;
		case 'S':
			errx(1, "Unsupported argument");
			break;
		case 't':
			errx(1, "The -t argument is unneeded as the staging area is unused");
			break;
		case 'v':
			add.flags |= verbosity_flag;
			break;
		case 'h':
		case '?':
		default:
			usage();
			break;
		}
	}
	argc -= optind;
	argv += optind;

	if (remote != 0) {
		char *site, *path;
		char *pkg_site;

		site = NULL;
		path = NULL;
		if ((pkg_site = getenv("PACKAGESITE")) != NULL) {
			if (strncmp("http://", pkg_site, 7) == 0 ||
			    strncmp("ftp://", pkg_site, 6) == 0) {
				site = pkg_site;
			}
			if (site != NULL) {
				/* Find the first / after the :// */
				path = strstr(site, "://");
				path = strchr(path + 3, '/');
				path[0] = '\0';
				path++;
			}
			printf("%s %s\n", site, path);
		}

		pkg_repo_free(add.repo);
		if ((add.flags & keep_file_flag) == keep_file_flag)
			add.repo = pkg_repo_new_ftp(site, path, NULL);
		else
			add.repo = pkg_repo_new_ftp(site, path, ".");
	}
	/* There are no packages to install. just quit now */
	if (argc == 0) {
		pkg_repo_free(add.repo);
		return 0;
	}

	if (add.chroot[0] != '\0')
		if (chroot(add.chroot) == -1)
			err(1, "Could not chroot to %s", add.chroot);

	/* Open the repo and database */
	if (add.repo == NULL)
		add.repo = pkg_repo_new_local_freebsd();
	if (add.db == NULL)
		add.db = pkg_db_open_freebsd("/");

	/* The rest of the arguments are packages to install */
	add.pkgs = malloc(sizeof(struct pkg*) * (argc + 1));
	for (i=0; i < argc; i++) {
		add.pkgs[i] = pkg_repo_get_pkg(add.repo, argv[i]);
		if (add.pkgs[i] == NULL) {
			errx(1, "can't stat package file '%s'", argv[i]);
		}
	}
	add.pkgs[i] = NULL;

	/* Perform the installation */
	ret = pkg_add(add);
	for (i=0; add.pkgs[i] != NULL; i++) {
		pkg_free(add.pkgs[i]);
	}
	free(add.pkgs);
	pkg_db_free(add.db);
	pkg_repo_free(add.repo);
	return ret;
}

static void
usage()
{
	fprintf(stderr,
	    "usage: pkg_add [-vInrfRMSK] [-t template] [-p prefix] [-P prefix] [-C chrootdir]\n"
	    "               pkg-name [pkg-name ...]");
    exit(1);
}

/*
 * Get the list of packages and call install_package to install them
 */
static int
pkg_add(struct pkg_add add)
{
	int i, ret;
	assert(add.db != NULL);
	assert(add.repo != NULL);
	for (i=0; add.pkgs[i] != NULL; i++) {
		if (pkg_db_is_installed(add.db, add.pkgs[i]) == 0) {
			warnx("package '%s' or its older version already installed",
			    pkg_get_name(add.pkgs[i]));
			continue;
		}
		ret = install_package(add.pkgs[i], add.repo, add.db,
		    add.base_prefix, add.prefix, add.flags);
		if (ret != 0)
			return 1;
	}
	return 0;
}

/*
 * Print the message from fmt
 * Only used when -v is set
 */
static void
pkg_action(int level, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	if (level == PKG_DB_PACKAGE)
		printf("extract: ");
	vprintf(fmt, ap);
	putchar('\n');
	va_end(ap);
}

/*
 * Recursively install the required packages
 */
static int
install_package(struct pkg *pkg, struct pkg_repo *repo, struct pkg_db *db,
		const char *base_prefix, const char *prefix, int flags)
{
	unsigned int i;
	int ret;
	struct pkg **deps;
	struct pkg_list *cur;

	assert(pkg != NULL);
	assert(repo != NULL);
	assert(db != NULL);

	/*
	 * See if the package has been marked as installed in this run.
	 * If it has don't bother attempting to install it again
	 */
	cur = head;
	while (cur != NULL) {
		if (strcmp(cur->name, pkg_get_name(pkg)) == 0) {
			return 0;
		}
		cur = cur->next;
	}
	
	/* Don't install a package that has been registered in the db */
	if (pkg_db_is_installed(db, pkg) == 0) {
		return 0;
	}

	/* Get the package's dependencies */
	deps = pkg_get_dependencies(pkg);
	for (i=0; deps[i] != NULL; i++) {
		struct pkg *new_pkg;

		/* Replace the empty package with one from disk */
		new_pkg = pkg_repo_get_pkg(repo, pkg_get_name(deps[i]));
		if (new_pkg == NULL) {
			warnx("could not find package %s",
			    pkg_get_name(deps[i]));
			pkg_list_free(deps);
			return -1;
		}
		pkg_free(deps[i]);
		deps[i] = new_pkg;

		/* Install the dependency */
		if (install_package(deps[i], repo, db, prefix, prefix, flags)
		    != 0 && (flags & force_flag) != force_flag) {
			pkg_list_free(deps);
			return -1;
		}
	}
	pkg_list_free(deps);

	ret = -1;
	/* Install the package */
	if ((flags & verbosity_flag) == verbosity_flag) {
		/* Install with a verbose output */
		printf("extract: Package name is %s\n", pkg_get_name(pkg));
		ret = pkg_db_install_pkg_action(db, pkg, base_prefix,
		    ((flags & no_record_install_flag)!= no_record_install_flag),
		    ((flags & no_run_flag) == no_run_flag), pkg_action);
	} else if ((flags & no_run_flag) == 0) {
		ret = pkg_db_install_pkg(db, pkg, base_prefix,
		    ((flags & no_record_install_flag)!=no_record_install_flag));
	}
	/*
	 * Insert the installed package in a linked
	 * list to stop it being installed again.
	 */
	if (ret == 0) {
		cur = malloc(sizeof(struct pkg_list));
		cur->next = head;
		cur->name = pkg_get_name(pkg);
		head = cur;
	}
	/* XXX Add warning if ret != 0 */
	return ret;
}
