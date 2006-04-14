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
#include <assert.h>
#include <pkg.h>
#include <regex.h>
#include <stdlib.h>

/* A struct to hold many regex's to be or'ed in _pkg_match_regex */
struct regex_or {
	unsigned int count;
	regex_t *rex;
};

static int _pkg_match_regex(struct pkg *, const void *);

static int
_pkg_match_regex(struct pkg *pkg, const void *data)
{
	unsigned int i;
	const struct regex_or *rex;
	
	assert(pkg != NULL);
	assert(data != NULL);

	rex = data;
	for(i=0; i < rex->count; i++) {
		if (regexec(&rex->rex[i], pkg_get_name(pkg), 0, NULL, 0) == 0)
			return 0;
	}
	return -1;
}

/*
 * Returnes a sorted NULL terminates array of packages matching one of regex
 */
struct pkg **
match_regex(struct pkg_db *db, char **regex, int type __unused)
{
	struct regex_or rex;
	unsigned int i;
	struct pkg **pkgs;

	/* Count the number of regex's */
	for (rex.count = 0; regex[rex.count] != NULL; rex.count++)
		continue;

	rex.rex = malloc(rex.count * sizeof(regex_t));
	if (rex.rex == NULL)
		return NULL;

	for(i=0; i < rex.count; i++) {
		regcomp(&rex.rex[i], regex[i], (type ? REG_EXTENDED : REG_BASIC)
		    | REG_NOSUB);
	}

	pkgs = pkg_db_get_installed_match(db, _pkg_match_regex, &rex);

	for(i=0; i < rex.count; i++) {
		regfree(&rex.rex[i]);
	}
	free(rex.rex);

	return pkgs;
}
