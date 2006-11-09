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

#include "pkg.h"
#include "pkg_db.h"

#include <assert.h>
#include <fnmatch.h>
#include <regex.h>
#include <string.h>
#include <stdlib.h>

/* A struct to hold many regex's to be or'ed in pkg_match_regex */
struct regex_or {
	unsigned int count;
	regex_t *rex;
};

struct glob_or {
	unsigned int count;
	const char **patterns;
};

static int pkg_match_name(struct pkg *, const void *);
static int pkg_match_regex(struct pkg *, const void *);
static int pkg_match_glob(struct pkg *, const void *);

/**
 * @defgroup PackageDBMatch
 * @ingroup PackageDB
 * @brief Functions to get lists of packages
 *
 * These functions are used to finds all the installed packages
 * that match a list of expressions.
 * All functions (except pkg_db_match_by_type()) are designed
 * to have a consistant signature so they could be used as a
 * callback if reqired to.
 * 
 * @{
 */

struct pkg **
pkg_db_match_by_type(struct pkg_db *db, const char **match, pkg_db_match_t type)
{
	struct pkg **ret;

	if (db == NULL)
		return NULL;

	if (match == NULL)
		return NULL;

	ret = NULL;
	switch (type) {
	case PKG_DB_MATCH_ALL:
		ret = pkg_db_match_all(db, NULL, 0);
		break;
	case PKG_DB_MATCH_EXACT:
		ret = pkg_db_match_name(db, match, 0);
		break;
	case PKG_DB_MATCH_GLOB:
		ret = pkg_db_match_glob(db, match, 0);
		break;
	case PKG_DB_MATCH_EREGEX:
	case PKG_DB_MATCH_REGEX:
		ret = pkg_db_match_regex(db, match,
		    (type == PKG_DB_MATCH_EREGEX));
		break;
	}

	return ret;
}

/**
 * @brief Finds all installed packages
 * @param db The database to search in
 * @param string Unused
 * @param type Unused
 *
 * This is a wrapper around pkg_db_get_installed_match().
 * @return A sorted NULL terminated array of all packages
 */
struct pkg **
pkg_db_match_all(struct pkg_db *db, const char **string __unused,
	int type __unused)
{
	if (db == NULL)
		return NULL;

	return pkg_db_get_installed_match(db, pkg_match_all, NULL);
}

/**
 * @brief Finds all installed with a given name
 * @param db The database to search in
 * @param name A null-terminated list of names
 * @param type Unused
 *
 * @return A sorted NULL terminated array of all packages
 */
struct pkg **
pkg_db_match_name(struct pkg_db *db, const char **name, int type __unused)
{
	if (db == NULL)
		return NULL;

	return pkg_db_get_installed_match(db, pkg_match_name, name);
}

/**
 * @brief Finds all installed packages that match on of the given regular expressions
 * @param db The database to search in
 * @param regex A NULL terminated array of strings containing the regular expressions
 * @param type If true use extended regular expressions
 * @return A sorted NULL terminated array of packages matching one of regex
 */
struct pkg **
pkg_db_match_regex(struct pkg_db *db, const char **regex, int type)
{
	struct regex_or rex;
	unsigned int i;
	struct pkg **pkgs;

	if (db == NULL)
		return NULL;

	if (regex == NULL)
		return NULL;

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

	pkgs = pkg_db_get_installed_match(db, pkg_match_regex, &rex);

	for(i=0; i < rex.count; i++) {
		regfree(&rex.rex[i]);
	}
	free(rex.rex);

	return pkgs;
}

/**
 * @brief Finds all installed packages that match a given shell like glob
 * @param db The database to search in
 * @param regex A NULL terminated array of strings containing the glob
 * @param type Unused
 * @return A NULL terminated array of packages matching a shell like glob
 */
struct pkg **
pkg_db_match_glob(struct pkg_db *db, const char **patterns, int type __unused)
{
	struct glob_or the_glob;
	struct pkg **pkgs;

	if (db == NULL)
		return NULL;

	if (patterns == NULL)
		return NULL;

	/* Count the number of globs */
	for (the_glob.count = 0; patterns[the_glob.count] != NULL;
	     the_glob.count++)
		continue;

	the_glob.patterns = patterns;

	pkgs = pkg_db_get_installed_match(db, pkg_match_glob, &the_glob);

	return pkgs;
}

/**
 * @}
 */

/**
 * @defgroup PackageDBMatchInternal
 * @ingroup PackageDBMatch
 * @brief Internal callbacks for pkg_db_get_installed_match()
 *
 * These are callbacks for pkg_db_installed_match().
 * They have a specific struct passed to them.
 * @{
 */

/**
 * @brief Function to match all packages with one of the given names
 * @return  0 if the package matches
 * @return -1 otherwise
 */
static int
pkg_match_name(struct pkg *pkg, const void *data)
{
	/** @todo pkg_match_name() can be public as it has no custom struct */
	int i;
	/* Use a union as I couldn't cast a const void * to a const char ** */
	union {
	    const char **str;
	    const void *data;
	} strings;

	assert(pkg != NULL);
	assert(data != NULL);

	strings.data = data;
	for(i=0; strings.str[i] != NULL; i++) {
		if (strcmp(strings.str[i], pkg_get_name(pkg)) == 0)
			return 0;
	}
	return -1;
}

/**
 * @brief Function to match one of the given regular expressions
 * @return  0 if matches
 * @return -1 otherwise
 */
static int
pkg_match_regex(struct pkg *pkg, const void *data)
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

/**
 * @brief Function to match one of the given shell like globs
 * @return  0 if matches
 * @return -1 otherwise
 */
static int
pkg_match_glob(struct pkg *pkg, const void *data)
{
	/** @todo Fix to just take a null terminated array of strings */
	unsigned int i;
	const struct glob_or *the_glob;
	
	assert(pkg != NULL);
	assert(data != NULL);

	the_glob = data;
	for(i=0; i < the_glob->count; i++) {
		/* This should use the csh_match from FreeBSD pkg_info */
		if (fnmatch(the_glob->patterns[i], pkg_get_name(pkg), 0) == 0)
			return 0;
	}
	return -1;
}

/**
 * @}
 */
