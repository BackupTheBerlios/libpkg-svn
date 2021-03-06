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

static void	show_cksum(struct pkg *, const char *, int);
static void	show_file(struct pkg_file *, const char *, int);
static void	show_fmtrev(struct pkg *, const char *, int);
static void	show_index(struct pkg *);
static void	show_origin(struct pkg *, const char *, int);
static void	show_plist(struct pkg *, const char *, int, int);
static void	show_size(struct pkg *, const char *, int quiet);

void
show(struct pkg_db *db __unused, struct pkg *pkg, int flags, int quiet)
{
	if (flags & SHOW_PKGNAME) {
		printf("%s\n", pkg_get_name(pkg));
		return;
	} else if (flags & SHOW_INDEX) {
		/* SHOW_INDEX stops all other flags working */
		show_index(pkg);
		return;
	}

	if (quiet == 0) {
		printf("Information for %s:\n\n", pkg_get_name(pkg));
	}
	if (flags & SHOW_COMMENT) {
		struct pkg_file *file;
		file = pkg_get_control_file(pkg, COMMENT_FNAME);
		show_file(file, "Comment:\n", quiet);
	}
#define ifexist_show(filename, title) \
	{ \
		struct pkg_file *file; \
		file = pkg_get_control_file(pkg, filename); \
		if (file != NULL) \
			show_file(file, title ":\n", quiet); \
	}

	/* XXX Abstract all this out to the appropriate object */
	if (flags & SHOW_DEPEND) {
		show_plist(pkg, "Depends on:\n", quiet, PKG_LINE_PKGDEP);
	}
	if ((flags & SHOW_REQBY)) {
		struct pkg_file *file;
		char *contents;

		file = pkg_get_control_file(pkg, REQUIRED_BY_FNAME);
		contents = pkg_file_get(file);
		if (contents != NULL && contents[0] != '\0')
			show_file(file, "Required by:\n", quiet);
	}
	if (flags & SHOW_DESC) {
		struct pkg_file *file;

		file = pkg_get_control_file(pkg, DESC_FNAME);
		show_file(file, "Description:\n", quiet);
	}
	if ((flags & SHOW_DISPLAY)) {
		ifexist_show(DISPLAY_FNAME, "Install notice");
	}
	if (flags & SHOW_PLIST) {
		show_plist(pkg, "Packing list:\n", quiet, 0);
	}
	if (flags & SHOW_REQUIRE) {
		ifexist_show(REQUIRE_FNAME, "Requirements script");
	}
	if ((flags & SHOW_INSTALL)) {
		ifexist_show(INSTALL_FNAME, "Install script");
	}
	if ((flags & SHOW_INSTALL)) {
		ifexist_show(POST_INSTALL_FNAME, "Post-Install script");
	}
	if ((flags & SHOW_DEINSTALL)) {
		ifexist_show(DEINSTALL_FNAME, "De-Install script");
	}
	if ((flags & SHOW_DEINSTALL)) {
		ifexist_show(POST_DEINSTALL_FNAME, "Post-DeInstall script");
	}
	if ((flags & SHOW_MTREE)) {
		ifexist_show(MTREE_FNAME, "mtree file");
	}
	if (flags & SHOW_PREFIX) {
		show_plist(pkg, "Prefix(s):\n", quiet, PKG_LINE_CWD);
	}
	if (flags & SHOW_FILES) {
	//    show_files("Files:\n", &plist);
	}
	if ((flags & SHOW_SIZE) &&
	    pkg_db_is_installed(db, pkg_get_name(pkg)) == 0) {
		show_size(pkg, "Package Size:\n", quiet);
	//    show_size("Package Size:\n", &plist);
	}
	if ((flags & SHOW_CKSUM) &&
	    pkg_db_is_installed(db, pkg_get_name(pkg)) == 0) {
		show_cksum(pkg, "Mismatched Checksums:\n", quiet);
	//    show_cksum("Mismatched Checksums:\n", &plist);
	}
	if (flags & SHOW_ORIGIN) {
		show_origin(pkg, "Origin:\n", quiet);
	}
	if (flags & SHOW_FMTREV) {
		show_fmtrev(pkg, "Packing list format revision:\n", quiet);
	}
	if (!quiet) {
		puts("");
	}
}

/* Show files that don't match the recorded checksum */
static void
show_cksum(struct pkg *pkg __unused, const char *title, int quiet)
{
	if (!quiet)
		printf("%s", title);

	/* XXX */
	errx(1, "%s: Unimplemented", __func__);
}

static void
show_file(struct pkg_file *file, const char *title, int quiet)
{
	if (!quiet)
		printf("%s", title);
	if (file == NULL) {
		printf("ERROR: show_file: Can't open '%s' for reading!\n",
		    pkg_file_get_name(file));
	} else {
		printf("%s", pkg_file_get(file));
	}
	putchar('\n');
	
}

static void
show_fmtrev(struct pkg* pkg, const char *title, int quiet)
{
	char *version;
	if (!quiet)
		printf("%s", title);

	version = pkg_get_version(pkg);
	if (version == NULL)
		errx(1, "pkg_get_version returned NULL");
	else
		printf("%s\n", version);
}

/*
 * Displays the package name and comment for a given package
 */
/* XXX Check output is correct */
static void
show_index(struct pkg *pkg)
{
	/* This assumes a terminal width of 80 characters */
	int len, pos;
	struct pkg_file *comment;
	len = printf("%s ", pkg_get_name(pkg));
	for (pos = len; pos < 19; pos++, len++) {
		putchar(' ');
	}
	comment = pkg_get_control_file(pkg, "+COMMENT");
	if (comment != NULL && len < 80) {
		char desc[60], *ptr;
		/*
		 * Copy the comment to a buffer
		 * so it is 80 characters wide
		 */
		strlcpy(desc, pkg_file_get(comment), (unsigned int)80-len);
		ptr = strchr(desc, '\n');
		if (ptr)
			ptr[0] = '\0';
		printf("%s\n", desc);
	} else
		putchar('\n');
}

static void
show_origin(struct pkg *pkg, const char *title, int quiet)
{
	if (!quiet)
		printf("%s", title);
	printf("%s\n", pkg_get_origin(pkg));
}

static void
show_plist(struct pkg *pkg, const char *title, int quiet, int type)
{
	struct pkg_freebsd_contents *contents;
	struct pkg_freebsd_contents_line *line;
	unsigned int i, ignore = 0;
	char *prefix = NULL;

	if (!quiet)
		printf("%s", title);
	contents = pkg_freebsd_get_contents(pkg);

	i = 0;
	while ((line = pkg_freebsd_contents_get_line(contents, i++)) != NULL) {
		if (line->line_type == PKG_LINE_UNKNOWN)
			continue;
		if (type != 0 && line->line_type != type)
			continue;
		switch(line->line_type) {
		case PKG_LINE_COMMENT:
			if (quiet) {
				printf("@comment %s\n", line->data);
				break;
			}
			if (strncmp("DEPORIGIN:", line->data, 10) == 0) {
				char *s = strchr(line->data, ':');
				*s++;
				printf("\tdependency origin: %s\n", s);
			} else if (strncmp("ORIGIN:", line->data, 7) == 0) {
				char *s = strchr(line->data, ':');
				*s++;
				printf("\tPackage origin: %s\n", s);
			} else {
				printf("\tComment: %s\n", line->data);
			}
			break;
		case PKG_LINE_NAME:
			printf(quiet ? "@name %s\n" : "\tPackage name: %s\n",
			    line->data);
			break;
		case PKG_LINE_CWD:
			if (prefix == NULL)
				prefix = line->data;
			printf(quiet ? "@cwd %s\n" : "\tCWD to %s\n",
			    (line->data == NULL) ? prefix : line->data);
			break;
		case PKG_LINE_PKGDEP:
			printf(quiet ? "@pkgdep %s\n" : "Dependency: %s\n",
			    line->data);
			break;
		case PKG_LINE_CONFLICTS:
			printf(quiet ? "@conflicts %s\n" : "Conflicts: %s\n",
			    line->data);
			break;
		case PKG_LINE_EXEC:
			printf(quiet ? "@exec %s\n" : "\tEXEC '%s'\n",
			    line->data);
			break;
		case PKG_LINE_UNEXEC:
			printf(quiet ? "@unexec %s\n" : "\tUNEXEC '%s'\n",
			    line->data);
			break;
		case PKG_LINE_IGNORE:
			ignore = 1;
			break;
		case PKG_LINE_DIRRM:
			printf(quiet ? "@dirrm %s\n" :
			    "\tDeinstall directory remove: %s\n", line->data);
			break;
		case PKG_LINE_MTREE:
			printf(quiet ? "@mtree %s\n" :
			    "\tPackage mtree file: %s\n", line->data);
			break;
		case PKG_LINE_FILE:
			if (ignore == 1)
				printf(quiet ? "%s\n" : "File: %s (ignored)\n",
				    line->line);
			else
				printf(quiet ? "%s\n" : "File: %s\n",
				    line->line);
			ignore = 0;
			break;
		default:
			errx(2, "%s: unknown command type %d (%s)",
			    __func__, line->line_type, line->line);
			break;
		}
	}
}

static void
show_size(struct pkg *pkg __unused, const char *title, int quiet)
{
	if (!quiet)
		printf("%s", title);

	/* XXX */
	errx(1, "%s: Unimplemented", __func__);
}
