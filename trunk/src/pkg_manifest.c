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

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

struct pkg_manifest_item_attr {
	pkg_manifest_item_attr attr;
	char *data;
};

struct pkg_manifest_item {
	pkg_manifest_item_type type;
	void		*data;

	struct pkg_manifest_item_attr *attrs;
	unsigned int	 attr_count;
	size_t		 attr_size;
};

struct pkg_manifest {
	void		*data;

	struct pkg_manifest_item **items;
	unsigned int	 item_count;
	size_t		 item_size;
};

/* These are used by the FreeBSD parser */
extern FILE *pkg_freebsd_in;
int pkg_freebsd_parse(struct pkg_manifest **);

/**
 * @defgroup PackageManifest Package manifest functions
 * 
 * These are the package manifest manipulation functions.
 *
 * A package manifest is a desctiption of how to install
 * and deinstall a given package.
 * @{
 */

/**
 * @brief Creates a new package manifest to describe a (de)install process
 * @return A new package manifest
 * @return NULL on error
 */
struct pkg_manifest *
pkg_manifest_new()
{
	struct pkg_manifest *manifest;

	manifest = malloc(sizeof(struct pkg_manifest));
	if (manifest == NULL)
		return NULL;

	manifest->data = NULL;
	manifest->items = NULL;
	manifest->item_count = 0;
	manifest->item_size = 0;

	return manifest;
}

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

	return manifest;
}

/**
 * @brief Cleans up a package manifest
 * @param manifest The manifest to free
 * @return  0 on success
 * @return -1 on failure
 */
int
pkg_manifest_free(struct pkg_manifest *manifest)
{
	unsigned int pos;

	if (manifest == NULL)
		return -1;

	for (pos = 0; pos < manifest->item_count; pos++) {
		assert(manifest->items[pos] != NULL);
		pkg_manifest_item_free(manifest->items[pos]);
	}

	free(manifest->items);
	free(manifest);
	return 0;
}

/**
 * @brief Adds a manifest item to a package manifest
 * @param manifest The manifest to append the item to
 * @param item The item to append to the manifest
 * @return  0 on success
 * @return -1 on failure
 */
int
pkg_manifest_append_item(struct pkg_manifest *manifest,
    struct pkg_manifest_item *item)
{
	struct pkg_manifest_item **new_items;

	if (manifest == NULL)
		return -1;
	if (manifest->items == NULL) {
		manifest->item_size = sizeof(struct pkg_manifest_item *) * 2;
		manifest->items = malloc(manifest->item_size);
		if (manifest->items == NULL) {
			manifest->item_size = 0;
			return -1;
		}
	} else {
		manifest->item_size += sizeof(struct pkg_manifest_item *);
		new_items = realloc(manifest->items, manifest->item_size);
		if (new_items == NULL) {
			manifest->item_size -=
			    sizeof(struct pkg_manifest_item *);
			return -1;
		}
		manifest->items = new_items;
	}
	manifest->items[manifest->item_count] = item;
	manifest->item_count++;
	manifest->items[manifest->item_count] = NULL;

	return 0;
}

/**
 * @}
 */

/**
 * @defgroup PackageManifestItem Package manifest item functions
 * @ingroup PackageManifest
 * 
 * These are the manifest item functions.
 *
 * A manifest item it the smallest part of a manifest.
 * @{
 */

/**
 * @brief Creates a new package manifest object
 * @param type The type of package manifest item to create
 * @param data The data to associate with this item
 * @return A new package manifest item object
 * @return NULL on error
 */
struct pkg_manifest_item *
pkg_manifest_item_new(pkg_manifest_item_type type, const char *data)
{
	struct pkg_manifest_item *item;

	item = malloc(sizeof(struct pkg_manifest_item));
	if (item == NULL)
		return NULL;

	item->type = type;
	item->attrs = NULL;
	item->attr_count = 0;
	item->attr_size = 0;

	if (data == NULL) {
		item->data = NULL;
	} else {
		item->data = strdup(data);
	}

	return item;
}

/**
 * @brief Cleans up a package manifest object
 * @param item The package manifest item to free
 * @return  0 on success
 * @return -1 on error
 */
int
pkg_manifest_item_free(struct pkg_manifest_item *item)
{
	if (item == NULL)
		return -1;

	if (item->data != NULL)
		free(item->data);

	if (item->attrs != NULL) {
		unsigned int pos;
		for (pos = 0; pos < item->attr_count; pos++) {
			if (item->attrs[pos].data != NULL)
				free(item->attrs[pos].data);
		}
		free(item->attrs);
	}

	free(item);

	return 0;
}

/**
 * @brief Sets the given attribute on the item
 * @param item The package item
 * @param attr The attribute to set
 * @param data The value to set the attribute to
 * @return  0 on success
 * @return -1 on error
 */
int
pkg_manifest_item_set_attr(struct pkg_manifest_item *item,
    pkg_manifest_item_attr attr, char *data)
{
	unsigned int pos;

	if (item == NULL)
		return -1;

	/*
	 * If there are attr's already check if the one
	 * we are setting is there. If it is replace the
	 * old data with the new data.
	 */
	if (item->attrs != NULL) {
		for (pos = 0; pos < item->attr_count; pos++) {
			if (item->attrs[pos].attr == attr) {
				if (item->attrs[pos].data != NULL)
					free(item->attrs[pos].data);
				item->attrs[pos].data = strdup(data);
				return 0;
			}
		}
	}

	if (item->attrs == NULL) {
		/* Create the attribute array */
		item->attr_size = sizeof(struct pkg_manifest_item_attr);
		item->attrs = malloc(item->attr_size);
		if (item->attrs == NULL)
			return -1;
	} else {
		/* Increase the size of the attribute array */
		struct pkg_manifest_item_attr *new_attr;
		item->attr_size += sizeof(struct pkg_manifest_item_attr);
		new_attr = realloc(item->attrs, item->attr_size);
		if (new_attr == NULL) {
			item->attr_size -=sizeof(struct pkg_manifest_item_attr);
			return -1;
		}
		item->attrs = new_attr;
	}
	/* Set the new attribute */
	item->attrs[item->attr_count].attr = attr;
	item->attrs[item->attr_count].data = strdup(data);
	item->attr_count++;

	return 0;
}

/**
 * @}
 */
