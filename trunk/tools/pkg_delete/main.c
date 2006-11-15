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
#define no_run_flag		(1<<1)
#define force_flag		(1<<2)
#define no_run_script_flag	(1<<3)
#define remove_empty_dirs_flag	(1<<4)
#define interactive_flag	(1<<5)
#define recursive_flag		(1<<6)

struct pkg_delete {
	struct pkg_db	 *db;
	struct pkg	**pkgs;
	int		  flags;
	pkg_db_match_t	  match_type;
	char		 *prefix;
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
	int i;

	delete.db = NULL;
	delete.pkgs = NULL;
	delete.flags = 0;
	delete.match_type = PKG_DB_MATCH_GLOB;
	delete.prefix = NULL;
	while ((ch = getopt(argc, argv, options)) != -1) {
		switch(ch) {
		case 'a':
			delete.match_type = PKG_DB_MATCH_ALL;
			break;
		case 'd':
			delete.flags |= remove_empty_dirs_flag;
			errx(1, "Unsupported argument");
			break;
		case 'D':
			delete.flags |= no_run_script_flag;
			break;
		case 'f':
			delete.flags |= force_flag;
			break;
		case 'G':
			delete.match_type = PKG_DB_MATCH_EXACT;
			break;
		case 'i':
			delete.flags |= interactive_flag;
			errx(1, "Unsupported argument");
			break;
		case 'n':
			delete.flags |= no_run_flag;
			break;
		case 'p':
			delete.prefix = optarg;
			errx(1, "Unsupported argument");
			break;
		case 'r':
			delete.flags |= recursive_flag;
			errx(1, "Unsupported argument");
			break;
		case 'v':
			delete.flags |= verbosity_flag;
			break;
		case 'x':
			delete.match_type = PKG_DB_MATCH_REGEX;
			break;
		case 'X':
			delete.match_type = PKG_DB_MATCH_EREGEX;
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

	delete.pkgs = NULL;
	delete.pkgs = pkg_db_match_by_type(delete.db, (const char **)argv,
	    delete.match_type);

	if (delete.pkgs == NULL) {
		ret = 1;
	} else if (delete.pkgs[0] == NULL) {
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
static int
pkg_action(enum pkg_action_level level __unused, int interactive,
	const char *fmt, ...)
{
	va_list ap;

	/* The interactive flag is ignored for now */
	assert(interactive == 0);

	va_start(ap, fmt);
	vprintf(fmt, ap);
	putchar('\n');
	va_end(ap);

	return 0;
}

/*
 * Deletes all the matching packages that were found on the system
 */
static int
pkg_delete(struct pkg_delete delete)
{
	pkg_db_action *action;
	int i, fake, scripts, force;

	assert(delete.db != NULL);
	assert(delete.pkgs != NULL);

	fake = ((delete.flags & no_run_flag) == no_run_flag);
	/*
	 * The scripts flag logic is reversed as
	 * it is set for no scripts to be run
	 */
	scripts = !((delete.flags & no_run_script_flag) == no_run_script_flag);
	force = ((delete.flags & force_flag) == force_flag);
	action = NULL;
	if ((delete.flags & verbosity_flag) == verbosity_flag || fake)
		action = pkg_action;

	for (i = 0; delete.pkgs[i] != NULL; i++) {
		pkg_db_delete_package_action(delete.db, delete.pkgs[i],
		    scripts, fake, force, action);
	}
	return 1;
}
