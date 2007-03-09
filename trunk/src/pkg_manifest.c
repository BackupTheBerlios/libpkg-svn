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
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


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
	unsigned int pos;
	struct pkg_manifest *manifest;

	manifest = malloc(sizeof(struct pkg_manifest));
	if (manifest == NULL)
		return NULL;

	manifest->data = NULL;
	manifest->file = NULL;
	manifest->name = NULL;

	for (pos = 0; pos < pkgm_max; pos++) {
		manifest->attrs[pos] = NULL;
	}
	STAILQ_INIT(&manifest->deps);
	STAILQ_INIT(&manifest->conflicts);
	STAILQ_INIT(&manifest->items);

	manifest->manifest_get_file = NULL;

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
	struct pkgm_deps *dep;
	struct pkgm_conflicts *conflict;
	struct pkgm_items *item;
	unsigned int pos;

	if (manifest == NULL)
		return -1;

	while ((dep = STAILQ_FIRST(&manifest->deps)) != NULL) {
		STAILQ_REMOVE_HEAD(&manifest->deps, list);
		pkg_free(dep->pkg);
		free(dep);
	}

	while ((conflict = STAILQ_FIRST(&manifest->conflicts)) != NULL) {
		STAILQ_REMOVE_HEAD(&manifest->conflicts, list);
		free(conflict->conflict);
		free(conflict);
	}

	while ((item = STAILQ_FIRST(&manifest->items)) != NULL) {
		STAILQ_REMOVE_HEAD(&manifest->items, list);
		pkg_manifest_item_free(item->item);
		free(item);
	}

	for (pos = 0; pos < pkgm_max; pos++) {
		if (manifest->attrs[pos] != NULL &&
		    manifest->attrs[pos] != (char *)0x1)
			free(manifest->attrs[pos]);
	}

	if (manifest->name != NULL)
		free(manifest->name);

	pkgfile_free(manifest->file);

	free(manifest);

	return 0;
}

/**
 * @brief Adds a dependency to the package manifest
 * @param manifest The manifest to add the dependency to
 * @param dep The package to depend on
 * @return  0 on success
 * @return -1 on error
 */
int
pkg_manifest_add_dependency(struct pkg_manifest *manifest, struct pkg *dep)
{
	struct pkgm_deps *the_dep;

	if (manifest == NULL || dep == NULL)
		return -1;

	/* Create the new dependency */
	the_dep = malloc(sizeof(struct pkgm_deps));
	if (the_dep == NULL)
		return -1;
	the_dep->pkg = dep;

	/* Add the dependency to the list */
	STAILQ_INSERT_TAIL(&manifest->deps, the_dep, list);

	return 0;
}

/**
 * @brief Adds a conflict to the package manifest
 * @param manifest The manifest to add the dependency to
 * @param conflict The package to conflict with
 * @return  0 on success
 * @return -1 on error
 */
int
pkg_manifest_add_conflict(struct pkg_manifest *manifest, const char *conflict)
{
	struct pkgm_conflicts *the_conflict;

	if (manifest == NULL || conflict == NULL)
		return -1;

	/* Create the new conflict */
	the_conflict = malloc(sizeof(struct pkgm_conflicts));
	if (the_conflict == NULL)
		return -1;
	the_conflict->conflict = strdup(conflict);

	/* Add the conflict to the list */
	STAILQ_INSERT_TAIL(&manifest->conflicts, the_conflict, list);

	return 0;
}

/**
 * @brief Sets the package's name in it's manifest
 * @param manifest The package manifest
 * @param name The package's name
 * @return  0 on success
 * @return -1 on error
 */
int
pkg_manifest_set_name(struct pkg_manifest *manifest, const char *name)
{
	char *new_name;
	if (manifest == NULL || name == NULL)
		return -1;

	new_name = strdup(name);
	if (new_name == NULL)
		return -1;

	if (manifest->name != NULL)
		free(manifest->name);

	manifest->name = new_name;
	return -0;
}

/**
 * @brief Adds an attribute to the package manifest
 * @param manifest The manifest to add attribute to
 * @param attr The attribute to add
 * @param data The value of the data
 * @return  0 on success
 * @return -1 on error
 */
int
pkg_manifest_set_attr(struct pkg_manifest *manifest, pkg_manifest_attr attr,
    const char *data)
{
	char *new_attr;

	if (manifest == NULL)
		return -1;

	if (attr >= pkgm_max)
		return -1;

	/* Set new_attr to the new value for the attribute */
	if (data == NULL) {
		new_attr = NULL;
	} else {
		new_attr = strdup(data);
		if (new_attr == NULL)
			return -1;
	}

	/* If the old attribure was set free it */
	if (manifest->attrs[attr] != NULL) {
		free(manifest->attrs[attr]);
	}

	/* assign the new attribute */
	manifest->attrs[attr] = new_attr;
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
	struct pkgm_items *the_item;

	if (manifest == NULL || item == NULL)
		return -1;

	/* Create the new item */
	the_item = malloc(sizeof(struct pkgm_items));
	if (the_item == NULL)
		return -1;
	the_item->item = item;

	/* Add the conflict to the list */
	STAILQ_INSERT_TAIL(&manifest->items, the_item, list);

	return 0;
}

/**
 * @brief Gets a pkgfile of the given manifest
 * @param manifest The manifest to read
 * @return A pkgfile containing the manifest
 * @return NULL on error
 */
struct pkgfile *
pkg_manifest_get_file(struct pkg_manifest *manifest)
{
	if (manifest == NULL)
		return NULL;

	if (manifest->file == NULL) {
		if (manifest->manifest_get_file != NULL)
			manifest->manifest_get_file(manifest);
	}

	return manifest->file;
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
		for (pos = 0; pos < pmia_max; pos++) {
			if (item->attrs[pos] != NULL &&
			    item->attrs[pos] != (char *)0x1)
				free(item->attrs[pos]);
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
    pkg_manifest_item_attr attr, const char *data)
{
	if (item == NULL)
		return -1;

	if (item->attrs == NULL) {
		item->attrs = malloc(pmia_max * sizeof(pkg_manifest_item_attr));
		if (item->attrs == NULL)
			return -1;
		memset(item->attrs, 0, pmia_max *
		    sizeof(pkg_manifest_item_attr));
	}

	if (item->attrs[attr] != NULL && item->attrs[attr] != (char *)0x1) {
		free(item->attrs[attr]);
	}

	if (data == NULL) {
		item->attrs[attr] = (char *)0x1;
	} else {
		item->attrs[attr] = strdup(data);
		if (item->attrs[attr] == NULL)
			return -1;
	}

	return 0;
}

/**
 * @brief Sets the data of the given item
 * @param item The manifest item
 * @param data The new data value
 * @return  0 on success
 * @return -1 on error
 */
int
pkg_manifest_item_set_data(struct pkg_manifest_item *item, const char *data)
{
	if (item == NULL)
		return -1;

	if (item->data != NULL)
		free(item->data);

	if (data == NULL) {
		item->data = NULL;
	} else {
		item->data = strdup(data);
		if (item->data == NULL)
			return -1;
	}

	return 0;
}

/**
 * @}
 */
