/*
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
 * Modified by Andrew Turner 2005, 2006 to use libpkg(3)
 * 
 * This is the info module.
 */

#include "pkg_info.h"

#include <pkg_db.h>
#include <pkg_freebsd.h>
#include <err.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static char options[] = "abcdDe:EfgGhiIjkl:LmoO:pPqQrRst:vVW:xX";

static void usage (void);

int
main(int argc, char **argv)
{
	struct pkg_info info;
	int ret;

	info.match_type = MATCH_GLOB;
	info.flags = 0;
	info.pkgs = NULL;
	info.quiet = 0;
	info.origin = NULL;
	info.check_package = NULL;
	info.seperator = "";
	info.use_blocksize = 0;
	info.search_file = NULL;

	if (argc == 1) {
		info.match_type = MATCH_ALL;
		info.flags = SHOW_INDEX;
	} else {
		int ch;
		int i;
		while ((ch = getopt(argc, argv, options)) != -1) {
			switch (ch) {
			case 'a':
				info.match_type = MATCH_ALL;
				break;
			case 'b':
				info.use_blocksize = 1;
				break;
			case 'c':
				info.flags |= SHOW_COMMENT; 
				break;
			case 'd':
				info.flags |= SHOW_DESC; 
				break;
			case 'D':
				info.flags |= SHOW_DISPLAY;
				break;
			case 'e':
				info.check_package = optarg;
				break;
			case 'E':
				info.flags |= SHOW_PKGNAME;
				break;
			case 'f':
				info.flags |= SHOW_PLIST;
				break;
			case 'g':
				info.flags |= SHOW_CKSUM;
				break;
			case 'G':
				info.match_type = MATCH_EXACT;
				break;
			case 'i':
				info.flags |= SHOW_INSTALL;
				break;
			case 'I':
				info.flags |= SHOW_INDEX;
				break;
			case 'j':
				info.flags |= SHOW_REQUIRE;
				break;
			case 'k':
				info.flags |= SHOW_DEINSTALL;
				break;
			case 'l':
				info.seperator = optarg;
				break;
			case 'L':
				info.flags |= SHOW_FILES;
				break;
			case 'm':
				info.flags |= SHOW_MTREE;
				break;
			case 'o':
				info.flags |= SHOW_ORIGIN;
				break;
			case 'O':
				info.origin = optarg;
				break;
			case 'p':
				info.flags |= SHOW_PREFIX;
				break;
			case 'P':
				info.flags |= SHOW_PTREV;
				break;
			case 'q':
				info.quiet = 1;
				break;
			case 'Q':
				info.quiet = 2;
				break;
			case 'r':
				info.flags |= SHOW_DEPEND;
				break;
			case 'R':
				info.flags |= SHOW_REQBY;
				break;
			case 's':
				info.flags |= SHOW_SIZE;
				break;
			case 't':
				errx(1, "Unsupported argument");
				break;
			case 'v':
				info.flags = SHOW_COMMENT | SHOW_DESC |
					SHOW_PLIST | SHOW_INSTALL |
					SHOW_DEINSTALL | SHOW_REQUIRE |
					SHOW_DISPLAY | SHOW_MTREE;
				break;
			case 'V':
				info.flags |= SHOW_FMTREV;
				break;
			case 'W':
				/* XXX Allow multiple -W files */
				info.search_file = optarg;
				break;
			case 'x':
				info.match_type = MATCH_REGEX;
				break;
			case 'X':
				info.match_type = MATCH_EREGEX;
				break;
			case 'h':
			default:
				usage();
				break;
			}
		}
		argc -= optind;
		argv += optind;

		info.pkgs = malloc(sizeof(char *) * (argc + 1));
		for (i=0; i < argc; i++) {
			info.pkgs[i] = argv[i];
		}
		info.pkgs[i] = NULL;
	}

	/* Set the default flags */
	if(!info.flags)
		info.flags = SHOW_COMMENT | SHOW_DESC | SHOW_REQBY;
	
	info.db = pkg_db_open_freebsd("/");
	if (!info.db)
		return 1;
	ret = pkg_info(info);
	if (info.pkgs != NULL)
		free(info.pkgs);
	pkg_db_free(info.db);
	return ret;
}

static void
usage()
{
    fprintf(stderr, "%s\n%s\n%s\n%s\n%s\n",
	"usage: pkg_info [-bcdDEfgGiIjkLmopPqQrRsvVxX] [-e package] [-l prefix]",
	"                [-t template] -a | pkg-name ...",
	"       pkg_info [-qQ] -W filename",
	"       pkg_info [-qQ] -O origin",
	"       pkg_info");
    exit(1);
}

int
pkg_info(struct pkg_info info)
{
	unsigned int cur;
	int retval;
	struct pkg **pkgs;

	retval = 1;
	pkgs = NULL;

	/* -e package name */
	if (info.check_package != NULL) {
		struct pkg *pkg;
		pkg = pkg_db_get_package(info.db, info.check_package);
		if (pkg != NULL) {
			pkg_free(pkg);
			return 0;
		}
		return 1;
	}

	/* -W <filename> */
	if (info.search_file != NULL) {
		struct stat sb;
		char *abs_path;

		abs_path = pkg_abspath(info.search_file);
		if (stat(abs_path, &sb) != 0) {
			/* XXX */
			free(abs_path);
			return 1;
		}
		pkgs = pkg_db_get_installed_match_count(info.db,
		    pkg_match_by_file, 1, (const void *)abs_path);
		if (info.quiet == 0)
			printf("%s was installed by package ", abs_path);
		printf("%s\n", pkg_get_name(pkgs[0]));
		free(abs_path);
		return 0;
	}

	/* -O <origin> */
	if (info.origin != NULL) {
		unsigned int pos;
		pkgs = pkg_db_get_installed_match(info.db, pkg_match_by_origin,
		    (const void *)info.origin);
		if (info.quiet == 0)
			printf("The following installed package(s) has %s "
			    "origin:\n", info.origin);
		for (pos = 0; pkgs[pos] != NULL; pos++) {
			printf("%s\n", pkg_get_name(pkgs[pos]));
		}
		return 0;
	}
	
	switch(info.match_type) {
	case MATCH_ALL:
	case MATCH_GLOB:
	case MATCH_NGLOB:
	case MATCH_REGEX:
	case MATCH_EREGEX:
		/* Display all packages installed */
		if (info.match_type == MATCH_ALL)
			pkgs = pkg_db_get_installed(info.db);
		else if (info.match_type == MATCH_REGEX ||
		         info.match_type == MATCH_EREGEX)
			pkgs = match_regex(info.db, (const char**)info.pkgs,
			    (info.match_type == MATCH_EREGEX));
		else if (info.match_type == MATCH_GLOB ||
		         info.match_type == MATCH_NGLOB)
			pkgs = match_glob(info.db, (const char**)info.pkgs,
			    (info.match_type == MATCH_GLOB));
		else
			errx(1, "ERROR: Inconsistancy in pkg_info");

		/* Sort the packages and display them */
		if (pkgs == NULL) {
			/* XXX Error message */
			return 1;
		}
		for (cur = 0; pkgs[cur] != NULL; cur++)
			continue;
		qsort(pkgs, cur, sizeof(struct pkg *), pkg_compare);
		for (cur = 0; pkgs[cur] != NULL; cur++) {
			show(info.db, pkgs[cur], info.flags, info.quiet,
			    info.seperator, info.use_blocksize);
		}
		retval = 0;
		break;
	case MATCH_EXACT:
		/* Only match the exact names given */
		retval = 0;
		
		for (cur = 0; info.pkgs[cur] != NULL; cur++) {
			struct pkg *pkg;

			pkg = pkg_db_get_package(info.db, info.pkgs[cur]);
			if (pkg != NULL)
				show(info.db, pkg, info.flags, info.quiet,
				    info.seperator, info.use_blocksize);
			else {
				warnx("pkg_info: can't find package '%s' "
				    "installed or in a file!", info.pkgs[cur]);
				retval = 1;
			}
		}
		break;
	}
	if (pkgs != NULL)
		pkg_list_free(pkgs);
	return retval;
}
