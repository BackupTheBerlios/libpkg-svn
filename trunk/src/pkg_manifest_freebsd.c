/*
 * Copyright (C) 2007, Andrew Turner, All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name(s) of the author(s) may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "pkg.h"
#include "pkg_private.h"

#include <assert.h>
#include <string.h>

/* These are used by the FreeBSD parser */
extern FILE *pkg_freebsd_in;
int pkg_freebsd_parse(struct pkg_manifest **);

static struct pkgfile *freebsd_manifest_get_file(struct pkg_manifest *);

/**
 * @defgroup FreeBSDManifest FreeBSD Package Manifest
 * @ingroup PackageManifest
 *
 * @{
 */

/**
 * @brief Creates a new FreeBSD package manifest from a struct pkgfile
 * @param file The file to create the manifest from
 * @return A new package manifest
 * @return NULL on error
 * @todo Check if pkg_freebsd_parse is thread safe
 */
struct pkg_manifest *
pkg_manifest_new_freebsd_pkgfile(struct pkgfile *file)
{
	struct pkg_manifest *manifest;

	pkgfile_seek(file, 0, SEEK_SET);
	pkg_freebsd_in = pkgfile_get_fileptr(file);
	pkg_freebsd_parse(&manifest);

	manifest->manifest_get_file = freebsd_manifest_get_file;

	return manifest;
}

/**
 * @}
 */

/**
 * @defgroup FreeBSDManifestCallbacks FreeBSD Manifest callbacks
 * @ingroup FreeBSDManifest
 *
 * @{
 */

/**
 * @brief Callback for pkg_manifest_get_file
 * @param manifest The manifest to get the file for
 * @return The pkgfile containinf the Manifest
 * @return NULL on error including incomplete manifest data
 */
static struct pkgfile *
freebsd_manifest_get_file(struct pkg_manifest *manifest)
{
	struct pkgm_deps *dep;
	struct pkgm_conflicts *conflict;
	struct pkgm_items *item;
	const char *data = "@comment PKG_FORMAT_REVISION:1.1\n";

	assert(manifest != NULL);
	assert(manifest->file == NULL);

	manifest->file = pkgfile_new_regular("+CONTENTS", data, strlen(data));
	pkgfile_append_string(manifest->file, "@name %s\n", manifest->name);

	/* Set the package's origin */
	if (manifest->attrs[pkgm_origin] != NULL) {
		pkgfile_append_string(manifest->file, "@comment ORIGIN:%s\n",
		    manifest->attrs[pkgm_origin]);
	}

	/* Set the package's prefix */
	if (manifest->attrs[pkgm_prefix] != NULL) {
		pkgfile_append_string(manifest->file, "@cwd %s\n",
		    manifest->attrs[pkgm_prefix]);
	}

	/* Add the package's dependency's */
	STAILQ_FOREACH(dep, &manifest->deps, list) {
		pkgfile_append_string(manifest->file, "@pkgdep %s\n",
		    pkg_get_name(dep->pkg));
		pkgfile_append_string(manifest->file, "@comment DEPORIGIN:%s\n",
		    pkg_get_origin(dep->pkg));
	}

	/* Add the package's conflicts */
	STAILQ_FOREACH(conflict, &manifest->conflicts, list) {
		pkgfile_append_string(manifest->file, "@conflicts %s\n",
		    conflict->conflict);
	}

	/* Add the package's (de)install items */
	STAILQ_FOREACH(item, &manifest->items, list) {
		switch(item->item->type) {
		case pmt_file:
			pkgfile_append_string(manifest->file, "%s\n",
			    (char *)item->item->data);
			if (item->item->attrs != NULL &&
			    item->item->attrs[pmia_md5] != NULL) {
				pkgfile_append_string(manifest->file,
				    "@comment MD5:%s\n",
				    item->item->attrs[pmia_md5]);
			}
			break;
		case pmt_dir:
			pkgfile_append_string(manifest->file, "@dirrm %s\n",
			    (char *)item->item->data);
			break;
		case pmt_dirlist:
			pkgfile_append_string(manifest->file, "@mtree %s\n",
			    (char *)item->item->data);
			break;
		case pmt_chdir:
			pkgfile_append_string(manifest->file, "@cwd %s\n",
			    (char *)item->item->data);
			break;
		case pmt_output:
			pkgfile_append_string(manifest->file, "@display %s\n",
			    (char *)item->item->data);
			break;
		case pmt_comment:
			pkgfile_append_string(manifest->file, "@comment %s\n",
			    (char *)item->item->data);
			break;
		case pmt_execute:
		{
			const char *cmd;

			if (item->item->attrs != NULL &&
			    item->item->attrs[pmia_deinstall] != NULL) {
				cmd = "@unexec";
			} else {
				cmd = "@exec";
			}
			pkgfile_append_string(manifest->file, "%s %s\n", cmd,
			    (char *)item->item->data);
			break;
		}
		case pmt_other:
		case pmt_error:
			break;
		}
	}

	return manifest->file;
}

/**
 * @}
 */
