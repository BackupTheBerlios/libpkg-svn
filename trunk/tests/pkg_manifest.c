/*
 * Copyright (C) 2007, Andrew Turner, All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include "test.h"

#include <pkg.h>
#include <pkg_private.h>

#include <string.h>

START_TEST(pkg_manifest_empty)
{
	fail_unless(pkg_manifest_free(NULL) == -1);
	fail_unless(pkg_manifest_set_manifest_version(NULL, NULL) == -1);
	fail_unless(pkg_manifest_set_manifest_version(NULL, "str") == -1);
	fail_unless(pkg_manifest_get_manifest_version(NULL) == NULL);
	fail_unless(pkg_manifest_add_dependency(NULL, NULL) == -1);
	fail_unless(pkg_manifest_replace_dependency(NULL, NULL, NULL) == -1);
	fail_unless(pkg_manifest_get_dependencies(NULL) == NULL);
	fail_unless(pkg_manifest_add_conflict(NULL, NULL) == -1);
	fail_unless(pkg_manifest_add_conflict(NULL, "str") == -1);
	fail_unless(pkg_manifest_set_name(NULL, NULL) == -1);
	fail_unless(pkg_manifest_set_name(NULL, "str") == -1);
	fail_unless(pkg_manifest_get_name(NULL) == NULL);
	fail_unless(pkg_manifest_set_attr(NULL, pkgm_other, NULL) == -1);
	fail_unless(pkg_manifest_set_attr(NULL, pkgm_other, "str") == -1);
	fail_unless(pkg_manifest_get_attrs(NULL) == NULL);
	fail_unless(pkg_manifest_append_item(NULL, NULL) == -1);
	fail_unless(pkg_manifest_get_conflicts(NULL) == NULL);
	fail_unless(pkg_manifest_get_file(NULL) == NULL);
	fail_unless(pkg_manifest_get_items(NULL) == NULL);
}
END_TEST

START_TEST(pkg_manifest_bad)
{
	struct pkg_manifest *manifest;

	fail_unless((manifest = pkg_manifest_new()) != NULL);

	fail_unless(pkg_manifest_set_manifest_version(manifest, NULL) == -1);
	fail_unless(pkg_manifest_add_dependency(manifest, NULL) == -1);
	fail_unless(pkg_manifest_replace_dependency(manifest, NULL, NULL) ==-1);
	fail_unless(pkg_manifest_add_conflict(manifest, NULL) == -1);
	fail_unless(pkg_manifest_set_name(manifest, NULL) == -1);
	fail_unless(pkg_manifest_get_name(manifest) == NULL);
	fail_unless(pkg_manifest_append_item(manifest, NULL) == -1);

	fail_unless(pkg_manifest_free(manifest) == 0);
}
END_TEST

START_TEST(pkg_manifest_version)
{
	struct pkg_manifest *manifest;

	fail_unless((manifest = pkg_manifest_new()) != NULL);

	fail_unless(pkg_manifest_get_manifest_version(manifest) == NULL);
	fail_unless(pkg_manifest_set_manifest_version(manifest, "version") ==0);
	fail_unless(strcmp(pkg_manifest_get_manifest_version(manifest),
	    "version") == 0);
	fail_unless(pkg_manifest_set_manifest_version(manifest, "new") == 0);
	fail_unless(strcmp(pkg_manifest_get_manifest_version(manifest), "new")
	    == 0);

	fail_unless(pkg_manifest_free(manifest) == 0);
}
END_TEST

START_TEST(pkg_manifest_dependency)
{
	struct pkg_manifest *manifest;
	struct pkg *pkg1, *pkg2, *pkg3, **pkg_list;

	fail_unless((manifest = pkg_manifest_new()) != NULL);
	fail_unless((pkg1 = pkg_new_freebsd_empty("foo")) != NULL);
	fail_unless((pkg2 = pkg_new_freebsd_empty("bar")) != NULL);
	fail_unless((pkg3 = pkg_new_freebsd_empty("baz")) != NULL);

	fail_unless(pkg_manifest_get_dependencies(manifest) == NULL);
	fail_unless(pkg_manifest_add_dependency(manifest, pkg1) == 0);
	fail_unless((pkg_list = pkg_manifest_get_dependencies(manifest)) !=
	    NULL);
	fail_unless(pkg_list[0] == pkg1);
	fail_unless(pkg_list[1] == NULL);

	pkg_list = NULL;
	fail_unless(pkg_manifest_add_dependency(manifest, pkg2) == 0);
	fail_unless((pkg_list = pkg_manifest_get_dependencies(manifest)) !=
	    NULL);
	fail_unless(pkg_list[0] == pkg1 || pkg_list[0] == pkg2);
	fail_unless(pkg_list[1] == pkg1 || pkg_list[1] == pkg2);
	fail_unless(pkg_list[0] != pkg_list[1]);
	fail_unless(pkg_list[2] == NULL);

	fail_unless(pkg_manifest_replace_dependency(manifest, pkg2, pkg3) == 0);
	fail_unless((pkg_list = pkg_manifest_get_dependencies(manifest)) !=
	    NULL);
	fail_unless(pkg_list[0] == pkg1 || pkg_list[0] == pkg3);
	fail_unless(pkg_list[1] == pkg1 || pkg_list[1] == pkg3);
	fail_unless(pkg_list[0] != pkg_list[1]);
	fail_unless(pkg_list[2] == NULL);

	fail_unless(pkg_manifest_free(manifest) == 0);
}
END_TEST

START_TEST(pkg_manifest_conflict)
{
	struct pkg_manifest *manifest;
	const char **pkg_list;

	fail_unless((manifest = pkg_manifest_new()) != NULL);

	fail_unless(pkg_manifest_get_conflicts(manifest) == NULL);
	fail_unless(pkg_manifest_add_conflict(manifest, "foo") == 0);
	fail_unless((pkg_list = pkg_manifest_get_conflicts(manifest)) != NULL);
	fail_unless(strcmp(pkg_list[0], "foo") == 0);

	fail_unless(pkg_manifest_add_conflict(manifest, "bar") == 0);
	fail_unless((pkg_list = pkg_manifest_get_conflicts(manifest)) != NULL);
	fail_unless(strcmp(pkg_list[0], "foo") == 0);
	fail_unless(strcmp(pkg_list[1], "bar") == 0);

	fail_unless(pkg_manifest_free(manifest) == 0);
}
END_TEST

START_TEST(pkg_manifest_name)
{
	struct pkg_manifest *manifest;

	fail_unless((manifest = pkg_manifest_new()) != NULL);

	fail_unless(pkg_manifest_set_name(manifest, "foo") == 0);
	fail_unless(strcmp(pkg_manifest_get_name(manifest), "foo") == 0);
	fail_unless(pkg_manifest_set_name(manifest, "bar") == 0);
	fail_unless(strcmp(pkg_manifest_get_name(manifest), "bar") == 0);

	fail_unless(pkg_manifest_free(manifest) == 0);
}
END_TEST

START_TEST(pkg_manifest_attrib)
{
	struct pkg_manifest *manifest;
	const char **attrs;

	fail_unless((manifest = pkg_manifest_new()) != NULL);

	fail_unless(pkg_manifest_set_attr(manifest, pkgm_other, "attr") == 0);
	fail_unless((attrs = pkg_manifest_get_attrs(manifest)) != NULL);
	fail_unless(strcmp(attrs[pkgm_other], "attr") == 0);
	fail_unless(attrs[pkgm_origin] == NULL);
	fail_unless(attrs[pkgm_prefix] == NULL);

	fail_unless(pkg_manifest_set_attr(manifest, pkgm_origin, "foo") == 0);
	fail_unless((attrs = pkg_manifest_get_attrs(manifest)) != NULL);
	fail_unless(strcmp(attrs[pkgm_other], "attr") == 0);
	fail_unless(strcmp(attrs[pkgm_origin], "foo") == 0);
	fail_unless(attrs[pkgm_prefix] == NULL);

	fail_unless(pkg_manifest_set_attr(manifest, pkgm_other, NULL) == 0);
	fail_unless((attrs = pkg_manifest_get_attrs(manifest)) != NULL);
	fail_unless(attrs[pkgm_other] == NULL);
	fail_unless(strcmp(attrs[pkgm_origin], "foo") == 0);
	fail_unless(attrs[pkgm_prefix] == NULL);

	fail_unless(pkg_manifest_free(manifest) == 0);
}
END_TEST

START_TEST(pkg_manifest_item)
{
	struct pkg_manifest *manifest;
	struct pkg_manifest_item *item1, *item2, **item_list;

	fail_unless((manifest = pkg_manifest_new()) != NULL);

	fail_unless((item1 = pkg_manifest_item_new(pmia_ignore, "ignore"))
	    != NULL);
	fail_unless((item2 = pkg_manifest_item_new(pmia_md5, "md5")) != NULL);

	fail_unless(pkg_manifest_append_item(manifest, item1) == 0);
	fail_unless((item_list = pkg_manifest_get_items(manifest)) != NULL);
	fail_unless(item_list[0] == item1);

	fail_unless(pkg_manifest_append_item(manifest, item2) == 0);
	fail_unless((item_list = pkg_manifest_get_items(manifest)) != NULL);
	fail_unless(item_list[0] == item1);
	fail_unless(item_list[1] == item2);

	fail_unless(pkg_manifest_free(manifest) == 0);
}
END_TEST

/*
 * TODO: Test pkg_manifest_get_file()
 */

Suite *
pkg_manifest_suite()
{
	Suite *s;
	TCase *tc;

	s = suite_create("pkg_manifest");

	tc = tcase_create("bad");
	tcase_add_test(tc, pkg_manifest_empty);
	tcase_add_test(tc, pkg_manifest_bad);
	tcase_add_test(tc, pkg_manifest_version);
	tcase_add_test(tc, pkg_manifest_dependency);
	tcase_add_test(tc, pkg_manifest_conflict);
	tcase_add_test(tc, pkg_manifest_name);
	tcase_add_test(tc, pkg_manifest_attrib);
	tcase_add_test(tc, pkg_manifest_item);
	suite_add_tcase(s, tc);

	return s;
}


