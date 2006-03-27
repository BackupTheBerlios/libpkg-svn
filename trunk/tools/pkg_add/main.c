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

struct pkg_add {
	struct pkg_db	 *db;
	struct pkg_repo	 *repo;
	struct pkg	**pkgs;
	int		  verbosity;
	char		  chroot[PATH_MAX];
};

static char options[] = "hvIRfnrp:P:SMt:C:K";

static void usage(void);
static int pkg_add(struct pkg_add);
static int install_package(struct pkg *, struct pkg_repo *, struct pkg_db *,
		int);

int
main (int argc, char *argv[])
{
	char ch;
	struct pkg_add add;
	int ret, i;

	add.db = NULL;
	add.repo = NULL;
	add.verbosity = 0;
	add.chroot[0] = '\0';
	while ((ch = getopt(argc, argv, options)) != -1) {
		switch(ch) {
		/* Case statements marked TODO will be supported in the next release */
		case 'C':
			strlcpy(add.chroot, optarg, PATH_MAX);
			break;
		case 'f':
			/* TODO */
			errx(1, "Unsupported argument");
			break;
		case 'I':
			/* TODO */
			errx(1, "Unsupported argument");
			break;
		case 'K':
			/* TODO */
			errx(1, "Unsupported argument");
			break;
		case 'M':
			errx(1, "Unsupported argument");
			break;
		case 'n':
			/* TODO */
			/* This dosn't seem to do anything in the base version */
			errx(1, "Unsupported argument");
			break;
		case 'P':
			errx(1, "Unsupported argument");
			break;
		case 'p':
			errx(1, "Unsupported argument");
			break;
		case 'R':
			/* TODO */
			errx(1, "Unsupported argument");
			break;
		case 'r':
			pkg_repo_free(add.repo);
			add.repo = pkg_repo_new_ftp(NULL, NULL);
			break;
		case 'S':
			errx(1, "Unsupported argument");
			break;
		case 't':
			errx(1, "The -t argument is unneded as the staging area is unused");
			break;
		case 'v':
			add.verbosity = 1;
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
	int i;
	assert(add.db != NULL);
	assert(add.repo != NULL);
	for (i=0; add.pkgs[i] != NULL; i++) {
		if (pkg_db_is_installed(add.db, add.pkgs[i]) == 0) {
			warnx("package '%s' or its older version already installed",
			    pkg_get_name(add.pkgs[i]));
			continue;
		}
		install_package(add.pkgs[i], add.repo, add.db, add.verbosity);
	}
	return 1;
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
 * Recursivley install the required packages
 */
static int
install_package(struct pkg *pkg, struct pkg_repo *repo, struct pkg_db *db, int verbosity)
{
	unsigned int i;
	int ret;
	struct pkg **deps;

	assert(pkg != NULL);
	assert(repo != NULL);
	assert(db != NULL);

	/* Don't install packages twice */
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
			continue;
		}
		pkg_free(deps[i]);
		deps[i] = new_pkg;

		/* Install the dependency */
		if (install_package(deps[i], repo, db, verbosity) != 0) {
			return -1;
		}
	}
	pkg_list_free(deps);

	if (verbosity) {
		printf("extract: Package name is %s\n", pkg_get_name(pkg));
		ret = pkg_db_install_pkg_action(db, pkg, pkg_action);
	} else {
		ret = pkg_db_install_pkg(db, pkg);
	}
	/* XXX Ass warning if ret != 0 */
	return ret;
}