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

#include <sys/param.h>

#include <assert.h>
#include <err.h>
#include <inttypes.h>
#include <pkg_db.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


static void	show_cksum(struct pkg *, const char *, const char *, int);
static void	show_deps(struct pkg *, const char *, const char *, int);
static void	show_file(struct pkgfile *, const char *, const char *, int);
static void	show_files(struct pkg *, const char *, const char *, int);
static void	show_fmtrev(struct pkg *, const char *, const char *, int);
static void	show_index(struct pkg *);
static void	show_origin(struct pkg *, const char *, const char *, int);
static void	show_plist(struct pkg *, const char *, const char *, int);
static void	show_prefix(struct pkg *, const char *, const char *, int);
static void	show_size(struct pkg *, const char *, const char *, int, int);

void
show(struct pkg_db *db, struct pkg *pkg, int flags, int quiet,
    const char *seperator, int use_blocksize __unused)
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
		struct pkgfile *file;
		file = pkg_get_control_file(pkg, COMMENT_FNAME);
		assert(file != NULL);
		show_file(file, seperator, "Comment:\n", quiet);
	}
#define ifexist_show(filename, title) \
	{ \
		struct pkgfile *file; \
		file = pkg_get_control_file(pkg, filename); \
		if (file != NULL) \
			show_file(file, seperator, title ":\n", quiet); \
	}

	/* XXX Abstract all this out to the appropriate object */
	if (flags & SHOW_DEPEND) {
		show_deps(pkg, seperator, "Depends on:\n", quiet);
	}
	if ((flags & SHOW_REQBY)) {
		struct pkgfile *file;

		file = pkg_get_control_file(pkg, REQUIRED_BY_FNAME);
		if (file != NULL) {
			if (pkgfile_get_size(file) > 0)
				show_file(file, seperator, "Required by:\n",
				    quiet);
		}
	}
	if (flags & SHOW_DESC) {
		struct pkgfile *file;

		file = pkg_get_control_file(pkg, DESC_FNAME);
		assert(file != NULL);
		show_file(file, seperator, "Description:\n", quiet);
	}
	if ((flags & SHOW_DISPLAY)) {
		ifexist_show(DISPLAY_FNAME, "Install notice");
	}
	if (flags & SHOW_PLIST) {
		show_plist(pkg, seperator, "Packing list:\n", quiet);
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
		show_prefix(pkg, seperator, "Prefix(s):\n", quiet);
	}
	if (flags & SHOW_FILES) {
		show_files(pkg, seperator, "Files:\n", quiet);
	}
	if ((flags & SHOW_SIZE) &&
	    pkg_db_is_installed(db, pkg) == 0) {
		show_size(pkg, seperator, "Package Size:\n", quiet,
		    use_blocksize);
	}
	if ((flags & SHOW_CKSUM) &&
	    pkg_db_is_installed(db, pkg) == 0) {
		show_cksum(pkg, seperator, "Mismatched Checksums:\n", quiet);
	}
	if (flags & SHOW_ORIGIN) {
		show_origin(pkg, seperator, "Origin:\n", quiet);
	}
	if (flags & SHOW_FMTREV) {
		show_fmtrev(pkg, seperator, "Packing list format revision:\n",
		    quiet);
	}
	if (!quiet) {
		puts(seperator);
	}
}

/* Show files that don't match the recorded checksum */
static void
show_cksum(struct pkg *pkg, const char *seperator, const char *title, int quiet)
{
	struct pkgfile *file;

	if (!quiet)
		printf("%s%s", seperator, title);

	file = pkg_get_next_file(pkg);
	while (file != NULL) {
		if (pkgfile_compare_checksum_md5(file) == 1)
			printf("%s\n", pkgfile_get_name(file));
		pkgfile_free(file);
		file = pkg_get_next_file(pkg);
	}
}

static void
show_deps(struct pkg *pkg, const char *seperator, const char *title, int quiet)
{
	struct pkg **deps;
	unsigned int i;

	if (!quiet)
		printf("%s%s", seperator, title);

	deps = pkg_get_dependencies(pkg);
	if (deps != NULL) {
		for (i = 0; deps[i] != NULL; i++) {
			printf("Dependency: %s\n",
			    pkg_get_name(deps[i]));
		}
	}
}

static void
show_file(struct pkgfile *file, const char *seperator, const char *title,
    int quiet)
{
	assert(file != NULL);
	if (!quiet)
		printf("%s%s", seperator, title);
	if (file == NULL) {
		printf("ERROR: show_file: Can't open '%s' for reading!\n",
		    pkgfile_get_name(file));
	} else {
		const char *str;
		uint64_t length, pos;

		length = pkgfile_get_size(file);
		str = pkgfile_get_data(file);
		for (pos = 0; pos < length; pos++) {
			putchar(str[pos]);
		}
	}
	putchar('\n');
	putchar('\n');
	
}

static void
show_files(struct pkg *pkg, const char *seperator, const char *title, int quiet)
{
	struct pkgfile *file;
	assert(pkg != NULL);
	if (!quiet)
		printf("%s%s", seperator, title);
	file = pkg_get_next_file(pkg);
	while (file != NULL) {
		printf("%s\n", pkgfile_get_name(file));
		pkgfile_free(file);
		file = pkg_get_next_file(pkg);
	}
}

static void
show_fmtrev(struct pkg* pkg, const char *seperator, const char *title,
    int quiet)
{
	const char *version;
	if (!quiet)
		printf("%s%s", seperator, title);

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
	struct pkgfile *comment;
	len = printf("%s ", pkg_get_name(pkg));
	for (pos = len; pos < 19; pos++, len++) {
		putchar(' ');
	}
	assert(pkg != NULL);
	comment = pkg_get_control_file(pkg, "+COMMENT");
	assert(comment != NULL);
	if (comment != NULL && len < 80) {
		/** @todo Rewrite */
		char desc[60], *s;
		const char *ptr;
		/*
		 * Copy the comment to a buffer
		 * so it is 80 characters wide
		 */
		ptr = pkgfile_get_data(comment);
		assert(ptr != NULL);
		strlcpy(desc, ptr, (unsigned int)80-len);

		/* Make sure the line is null terminated at the line break */
		s = strchr(desc, '\n');
		if (s)
			s[0] = '\0';
		printf("%s\n", desc);
	} else
		putchar('\n');
}

static void
show_origin(struct pkg *pkg, const char *seperator, const char *title,
    int quiet)
{
	if (!quiet)
		printf("%s%s", seperator, title);
	printf("%s\n", pkg_get_origin(pkg));
}

static void
show_prefix(struct pkg *pkg, const char *seperator, const char *title,
    int quiet)
{
	if (!quiet)
		printf("%s%s", seperator, title);

	printf("\tCWD %s\n", pkg_get_prefix(pkg));
}

static void
show_plist(struct pkg *pkg, const char *seperator, const char *title, int quiet)
{
	struct pkg_manifest *manifest;

	if (!quiet)
		printf("%s%s", seperator, title);
	
	manifest = pkg_get_manifest(pkg);
	assert(manifest != NULL);

	if (quiet) {
		struct pkgfile *manifest_file;
		const char *data;
		size_t len;

		manifest_file = pkg_manifest_get_file(manifest);
		data = pkgfile_get_data(manifest_file);
		len = pkgfile_get_size(manifest_file);
		fwrite(data, len, 1, stdout);
	} else {
		const char **conflicts;
		struct pkg **deps;
		struct pkg_manifest_item **items;
		unsigned int i;

		/* TODO: Push the logic to get the packaging instructions to libpkg */
		/* Print the head of the contents file */
		printf("\tComment: PKG_FORMAT_REVISION:1.1\n");
		printf("\tPackage name: %s\n",pkg_get_name(pkg));
		printf("\tPackage origin: %s\n", pkg_get_origin(pkg));
		printf("\tCWD %s\n", pkg_get_prefix(pkg));

		/* Print the dependencies of the contents file */
		deps = pkg_get_dependencies(pkg);
		if (deps != NULL) {
			for (i = 0; deps[i] != NULL; i++) {
				printf("Dependency: %s\n",
				    pkg_get_name(deps[i]));
				printf("\tdependency origin: %s\n",
				    pkg_get_origin(deps[i]));
			}
		}

		conflicts = pkg_get_conflicts(pkg);
		if (conflicts != NULL) {
			for (i = 0; conflicts[i] != NULL; i++) {
				printf("Conflicts: %s\n", conflicts[i]);
			}
		}

		items = pkg_manifest_get_items(manifest);
		for (i = 0; items[i] != NULL; i++) {
			switch(pkg_manifest_item_get_type(items[i])) {
			case pmt_file:
				if (pkg_manifest_item_get_attr(items[i],
				    pmia_ignore) != NULL) {
					printf("File: %s (ignored)\n",
					    (const char *)
					    pkg_manifest_item_get_data
					    (items[i]));
				} else {
					printf("File: %s\n", (const char *)
					    pkg_manifest_item_get_data
					    (items[i]));
				}
				if (pkg_manifest_item_get_attr(items[i],
				    pmia_md5) != NULL) {
					printf("\tComment: MD5:%s\n",
					    pkg_manifest_item_get_attr(items[i],
					    pmia_md5));
				}
				break;
			case pmt_dir:
				printf("\tDeinstall directory remove: %s\n",
				    (const char *)pkg_manifest_item_get_data
				    (items[i]));
				break;
			case pmt_dirlist:
				printf("\tPackage mtree file: %s\n",
				    (const char *)pkg_manifest_item_get_data
				    (items[i]));
				break;
			case pmt_chdir:
				printf("\tCWD to %s\n", (const char *)
				    pkg_manifest_item_get_data(items[i]));
				break;
			case pmt_comment:
				printf("\tComment: %s\n", (const char *)
				    pkg_manifest_item_get_data(items[i]));
				break;
			case pmt_execute:
				if (pkg_manifest_item_get_attr(items[i],
				    pmia_deinstall) != NULL) {
					printf("\tUNEXEC '%s'\n", (const char *)
					    pkg_manifest_item_get_data
					    (items[i]));
				} else {
					printf("\tEXEC '%s'\n", (const char *)
					    pkg_manifest_item_get_data
					    (items[i]));
				}
				break;
			case pmt_other:
				printf("?");
			case pmt_output:
				printf("?\n");
			case pmt_error:
				break;
			}
		}
	}
}

static void
show_size(struct pkg *pkg, const char *seperator, const char *title,
    int quiet, int use_blocksize)
{
	uint64_t size = 0;
	long block_size;
	int headerlen; /* Used only with getbsize(3) */
	char *descr;
	struct pkgfile *file;

	if (!quiet)
		printf("%s%s", seperator, title);

	descr = getbsize(&headerlen, &block_size);
	
	/* XXX When getting files and size we should only run through the files once */
	file = pkg_get_next_file(pkg);
	while (file != NULL) {
		size += pkgfile_get_size(file);
		pkgfile_free(file);
		file = pkg_get_next_file(pkg);
	}
	if (!quiet)
		printf("%" PRIu64 "\t(%s)\n", howmany(size, block_size), descr);
	else
		if (use_blocksize)
			printf("%" PRIu64 "\n", howmany(size, block_size));
		else
			printf("%" PRIu64 "\n", size);
}
