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

#ifndef __PKG_INFO_H__
#define __PKG_INFO_H__

#include <pkg.h>
#include <pkg_db.h>

#define SHOW_COMMENT	0x00001
#define SHOW_DESC	0x00002
#define SHOW_PLIST	0x00004
#define SHOW_INSTALL	0x00008
#define SHOW_DEINSTALL	0x00010
#define SHOW_REQUIRE	0x00020
#define SHOW_PREFIX	0x00040
#define SHOW_INDEX	0x00080
#define SHOW_FILES	0x00100
#define SHOW_DISPLAY	0x00200
#define SHOW_REQBY	0x00400
#define SHOW_MTREE	0x00800
#define SHOW_SIZE	0x01000
#define SHOW_ORIGIN	0x02000
#define SHOW_CKSUM	0x04000
#define SHOW_FMTREV	0x08000
#define SHOW_PTREV	0x10000
#define SHOW_DEPEND	0x20000
#define SHOW_PKGNAME	0x40000

/* The names of our "special" files */
#define CONTENTS_FNAME		"+CONTENTS"
#define COMMENT_FNAME		"+COMMENT"
#define DESC_FNAME		"+DESC"
#define INSTALL_FNAME		"+INSTALL"
#define POST_INSTALL_FNAME	"+POST-INSTALL"
#define DEINSTALL_FNAME		"+DEINSTALL"
#define POST_DEINSTALL_FNAME	"+POST-DEINSTALL"
#define REQUIRE_FNAME		"+REQUIRE"
#define REQUIRED_BY_FNAME	"+REQUIRED_BY"
#define DISPLAY_FNAME		"+DISPLAY"
#define MTREE_FNAME		"+MTREE_DIRS"

typedef enum {
	MATCH_ALL,
	MATCH_EXACT,
	MATCH_GLOB,
	MATCH_NGLOB,
	MATCH_EREGEX,
	MATCH_REGEX
} match_t;

struct pkg_info {
	struct pkg_db *db;
	char	**pkgs;
	int	  quiet;
	match_t	  match_type;
	int	  flags;
	const char *check_package;
	const char *origin;
};

struct pkg	**match_regex(struct pkg_db *, const char **, int);
struct pkg	**match_glob(struct pkg_db *, const char **, int);
int		  pkg_info(struct pkg_info);
void		  show(struct pkg_db *, struct pkg *, int, int);

#endif /* __PKG_INFO_H__ */
