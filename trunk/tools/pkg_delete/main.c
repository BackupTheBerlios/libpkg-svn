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
 * This is the delete module.
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
//#define keep_file_flag		(1<<1)
//#define no_run_flag		(1<<2)
//#define force_flag		(1<<3)
//#define no_run_script_flag	(1<<4)
//#define no_record_install_flag	(1<<5)

struct pkg_delete {
	struct pkg_db	 *db;
	struct pkg	**pkgs;
	int		  flags;
//	char		 *prefix;
};

static char options[] = "adDfGhinp:rvxX";

static void usage(void);
static int pkg_delete(struct pkg_delete);

int
main (int argc, char *argv[])
{
	char ch;
	struct pkg_delete delete;
	int ret;
	int i, j;

	delete.db = NULL;
	delete.pkgs = NULL;
	delete.flags = 0;
	//delete.chroot[0] = '\0';
	//delete.base_prefix = NULL;
	//delete.prefix = NULL;
	while ((ch = getopt(argc, argv, options)) != -1) {
		switch(ch) {
		case 'a':
			break;
		case 'd':
			break;
		case 'D':
			break;
		case 'f':
			break;
		case 'G':
			break;
		case 'i':
			break;
		case 'n':
			break;
		case 'p':
			break;
		case 'r':
			break;
		case 'v':
			delete.flags |= verbosity_flag;
			break;
		case 'x':
			break;
		case 'X':
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

	if (delete.db == NULL) {
		delete.db = pkg_db_open_freebsd("/");
	}

	delete.pkgs = malloc(sizeof(struct pkg*) * (argc + 1));
	if (delete.pkgs == NULL) {
		perror("pkg_delete");
		return 1;
	}

	/* Create an array of all packages to delete */
	for (i = 0, j = 0; i < argc; i++) {
		delete.pkgs[j] = pkg_db_get_package(delete.db, argv[i]);
		if (delete.pkgs[j] != NULL)
			j++;
		else
			fprintf(stderr,
			    "pkg_delete: no such package '%s' installed\n",
			    argv[i]);
	}
	if (delete.pkgs[0] == NULL) {
		ret = 1;
	} else {
		/* Perform the deinstallation */
		ret = pkg_delete(delete);
		for (i=0; delete.pkgs[i] != NULL; i++)
			pkg_free(delete.pkgs[i]);
	}
	free(delete.pkgs);
	pkg_db_free(delete.db);

	return ret;
}

static void
usage()
{
	fprintf(stderr,
	    "usage: pkg_delete [-dDfGinrvxX] [-p prefix] pkg-name ...\n"
	    "       pkg_delete -a [flags]");
	exit(1);
}

/*
 * Print the message from fmt
 * Only used when -v is set
 */
static void
pkg_action(int level __unused, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vprintf(fmt, ap);
	putchar('\n');
	va_end(ap);
}

/*
 * Deletes all the matching packages that were found on the system
 */
static int
pkg_delete(struct pkg_delete delete)
{
	int i;

	assert(delete.db != NULL);
	assert(delete.pkgs != NULL);

	for (i = 0; delete.pkgs[i] != NULL; i++) {
		if ((delete.flags & verbosity_flag) == verbosity_flag) {
			pkg_db_delete_package_action(delete.db, delete.pkgs[i],
			    0, 0, pkg_action);
		} else {
			pkg_db_delete_package_action(delete.db, delete.pkgs[i],
			    0, 0, NULL);
		}
	}
	return 1;
}
