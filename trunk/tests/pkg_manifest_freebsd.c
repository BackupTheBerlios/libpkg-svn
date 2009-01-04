/*
 * Copyright (C) 2007, 2009 Andrew Turner, All rights reserved.
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
#include <pkg_freebsd.h>
#include <pkg_freebsd_private.h>

#include <string.h>

#define pkg_manifest_default "@comment PKG_FORMAT_REVISION:1.1\n" \
    "@name package_name-1.0\n" \
    "@comment ORIGIN:package/origin\n" \
    "@cwd /usr/local\n"

START_TEST(pkg_manifest_freebsd_empty_test)
{
	struct pkgfile *file;
	struct pkg_manifest *manifest;

	file = pkgfile_new_regular("+CONTENTS", "", 0);

	manifest = pkg_manifest_new_freebsd_pkgfile(file);
	fail_unless(manifest == NULL, NULL);
	pkg_manifest_free(manifest);
	pkgfile_free(file);
}
END_TEST

static void
pkg_manifest_freebsd_good_common(struct pkg_manifest *manifest)
{
	fail_unless(manifest != NULL, NULL);

	fail_unless(pkg_manifest_get_manifest_version(manifest) != NULL);
	fail_unless(strcmp(pkg_manifest_get_manifest_version(manifest),
	    "1.1") == 0);

	fail_unless(pkg_manifest_get_name(manifest) != NULL);
	fail_unless(strcmp(pkg_manifest_get_name(manifest),
	    "package_name-1.0") == 0);

	fail_unless(pkg_manifest_get_attrs(manifest) != NULL);

	fail_unless(pkg_manifest_get_attr(manifest, pkgm_other) == NULL);

	fail_unless(pkg_manifest_get_attr(manifest, pkgm_origin) != NULL);
	fail_unless(strcmp(pkg_manifest_get_attr(manifest, pkgm_origin),
	    "package/origin") == 0);

	fail_unless(pkg_manifest_get_attr(manifest, pkgm_prefix) != NULL);
	fail_unless(strcmp(pkg_manifest_get_attr(manifest, pkgm_prefix),
	    "/usr/local") == 0);

	fail_unless(pkg_manifest_get_attr(manifest, pkgm_max) == NULL);

	fail_unless(pkg_manifest_get_file(manifest) != NULL);
}

static void
pkg_manifest_freebsd_good_basic_test_run(struct pkg_manifest *manifest)
{
	pkg_manifest_freebsd_good_common(manifest);

	fail_unless(pkg_manifest_get_conflicts(manifest) == NULL);
	fail_unless(pkg_manifest_get_dependencies(manifest) == NULL);
	fail_unless(pkg_manifest_get_items(manifest) == NULL);
}

/* Test if a minimal config file will work */
START_TEST(pkg_manifest_freebsd_good_basic_test)
{
	const char *pkg_data = pkg_manifest_default;
	struct pkgfile *file, *file2;
	struct pkg_manifest *manifest, *manifest2;

	file = pkgfile_new_regular("+CONTENTS", pkg_data, strlen(pkg_data));
	manifest = pkg_manifest_new_freebsd_pkgfile(file);
	pkg_manifest_freebsd_good_basic_test_run(manifest);

	file2 = pkg_manifest_get_file(manifest);
	manifest2 = pkg_manifest_new_freebsd_pkgfile(file2);
	pkg_manifest_freebsd_good_basic_test_run(manifest2);

	pkg_manifest_free(manifest2);
	pkg_manifest_free(manifest);
	pkgfile_free(file);
}
END_TEST

static void
check_good_command(struct pkg_manifest *manifest,
    pkg_manifest_item_type type)
{
	struct pkg_manifest_item **items;

	pkg_manifest_freebsd_good_common(manifest);

	fail_unless(pkg_manifest_get_conflicts(manifest) == NULL);
	fail_unless(pkg_manifest_get_dependencies(manifest) == NULL);

	items = pkg_manifest_get_items(manifest);
	fail_unless(items != NULL);
	fail_unless(items[0] != NULL);
	fail_unless(items[1] == NULL);
	fail_unless(pkg_manifest_item_get_type(items[0]) == type);
}

/*
 * Check a command with no data fails
 */
START_TEST(pkg_manifest_freebsd_good_comment_test)
{
	const char *pkg_data = pkg_manifest_default "@comment data\n";
	struct pkgfile *file;
	struct pkg_manifest *manifest;

	file = pkgfile_new_regular("+CONTENTS", pkg_data, strlen(pkg_data));
	manifest = pkg_manifest_new_freebsd_pkgfile(file);
	check_good_command(manifest, pmt_comment);

	pkg_manifest_free(manifest);
	pkgfile_free(file);
}
END_TEST

START_TEST(pkg_manifest_freebsd_good_cwd_test)
{
	const char *pkg_data = pkg_manifest_default "@cwd data\n";
	struct pkgfile *file;
	struct pkg_manifest *manifest;

	file = pkgfile_new_regular("+CONTENTS", pkg_data, strlen(pkg_data));
	manifest = pkg_manifest_new_freebsd_pkgfile(file);
	check_good_command(manifest, pmt_chdir);

	pkg_manifest_free(manifest);
	pkgfile_free(file);
}
END_TEST

START_TEST(pkg_manifest_freebsd_good_exec_test)
{
	const char *pkg_data = pkg_manifest_default "@exec data\n";
	struct pkgfile *file;
	struct pkg_manifest *manifest;

	file = pkgfile_new_regular("+CONTENTS", pkg_data, strlen(pkg_data));
	manifest = pkg_manifest_new_freebsd_pkgfile(file);
	check_good_command(manifest, pmt_execute);

	pkg_manifest_free(manifest);
	pkgfile_free(file);
}
END_TEST

START_TEST(pkg_manifest_freebsd_good_unexec_test)
{
	const char *pkg_data = pkg_manifest_default "@unexec data\n";
	struct pkgfile *file;
	struct pkg_manifest *manifest;

	file = pkgfile_new_regular("+CONTENTS", pkg_data, strlen(pkg_data));
	manifest = pkg_manifest_new_freebsd_pkgfile(file);
	check_good_command(manifest, pmt_execute);

	pkg_manifest_free(manifest);
	pkgfile_free(file);
}
END_TEST

START_TEST(pkg_manifest_freebsd_good_dirrm_test)
{
	const char *pkg_data = pkg_manifest_default "@dirrm data\n";
	struct pkgfile *file;
	struct pkg_manifest *manifest;

	file = pkgfile_new_regular("+CONTENTS", pkg_data, strlen(pkg_data));
	manifest = pkg_manifest_new_freebsd_pkgfile(file);
	check_good_command(manifest, pmt_dir);

	pkg_manifest_free(manifest);
	pkgfile_free(file);
}
END_TEST

START_TEST(pkg_manifest_freebsd_good_mtree_test)
{
	const char *pkg_data = pkg_manifest_default "@mtree data\n";
	struct pkgfile *file;
	struct pkg_manifest *manifest;

	file = pkgfile_new_regular("+CONTENTS", pkg_data, strlen(pkg_data));
	manifest = pkg_manifest_new_freebsd_pkgfile(file);
	check_good_command(manifest, pmt_dirlist);

	pkg_manifest_free(manifest);
	pkgfile_free(file);
}
END_TEST

START_TEST(pkg_manifest_freebsd_good_display_test)
{
	const char *pkg_data = pkg_manifest_default "@display data\n";
	struct pkgfile *file;
	struct pkg_manifest *manifest;

	file = pkgfile_new_regular("+CONTENTS", pkg_data, strlen(pkg_data));
	manifest = pkg_manifest_new_freebsd_pkgfile(file);
	check_good_command(manifest, pmt_output);

	pkg_manifest_free(manifest);
	pkgfile_free(file);
}
END_TEST

START_TEST(pkg_manifest_freebsd_good_pkgdep_test)
{
	const char *pkg_data = pkg_manifest_default "@pkgdep data\n";
	struct pkgfile *file;
	struct pkg_manifest *manifest;
	struct pkg **deps;

	file = pkgfile_new_regular("+CONTENTS", pkg_data, strlen(pkg_data));
	manifest = pkg_manifest_new_freebsd_pkgfile(file);

	pkg_manifest_freebsd_good_common(manifest);
	fail_unless(pkg_manifest_get_conflicts(manifest) == NULL);
	fail_unless(pkg_manifest_get_items(manifest) == NULL);

	deps = pkg_manifest_get_dependencies(manifest);
	fail_unless(deps != NULL);
	fail_unless(deps[0] != NULL);
	fail_unless(deps[1] == NULL);
	fail_unless(strcmp(pkg_get_name(deps[0]), "data") == 0);

	pkg_manifest_free(manifest);
	pkgfile_free(file);
}
END_TEST

START_TEST(pkg_manifest_freebsd_good_conflicts_test)
{
	const char *pkg_data = pkg_manifest_default "@conflicts data\n";
	struct pkgfile *file;
	struct pkg_manifest *manifest;
	struct pkg **conflicts;

	file = pkgfile_new_regular("+CONTENTS", pkg_data, strlen(pkg_data));
	manifest = pkg_manifest_new_freebsd_pkgfile(file);

	pkg_manifest_freebsd_good_common(manifest);
	fail_unless(pkg_manifest_get_dependencies(manifest) == NULL);
	fail_unless(pkg_manifest_get_items(manifest) == NULL);

	conflicts = pkg_manifest_get_conflicts(manifest);
	fail_unless(conflicts != NULL);
	fail_unless(conflicts[0] != NULL);
	fail_unless(conflicts[1] == NULL);
	fail_unless(strcmp(conflicts[0], "data") == 0);

	pkg_manifest_free(manifest);
	pkgfile_free(file);
}
END_TEST

/*
 * Check a command with no data fails
 */
START_TEST(pkg_manifest_freebsd_bad_empty_comment_test)
{
	const char *pkg_data = pkg_manifest_default "@comment\n";
	struct pkgfile *file;
	struct pkg_manifest *manifest;

	file = pkgfile_new_regular("+CONTENTS", pkg_data, strlen(pkg_data));
	manifest = pkg_manifest_new_freebsd_pkgfile(file);
	fail_unless(manifest == NULL);

	pkgfile_free(file);
}
END_TEST

START_TEST(pkg_manifest_freebsd_bad_empty_cwd_test)
{
	const char *pkg_data = pkg_manifest_default "@cwd\n";
	struct pkgfile *file;
	struct pkg_manifest *manifest;

	file = pkgfile_new_regular("+CONTENTS", pkg_data, strlen(pkg_data));
	manifest = pkg_manifest_new_freebsd_pkgfile(file);
	fail_unless(manifest == NULL);

	pkgfile_free(file);
}
END_TEST

START_TEST(pkg_manifest_freebsd_bad_empty_pkgdep_test)
{
	const char *pkg_data = pkg_manifest_default "@pkgdep\n";
	struct pkgfile *file;
	struct pkg_manifest *manifest;

	file = pkgfile_new_regular("+CONTENTS", pkg_data, strlen(pkg_data));
	manifest = pkg_manifest_new_freebsd_pkgfile(file);
	fail_unless(manifest == NULL);

	pkgfile_free(file);
}
END_TEST

START_TEST(pkg_manifest_freebsd_bad_empty_conflicts_test)
{
	const char *pkg_data = pkg_manifest_default "@conflicts\n";
	struct pkgfile *file;
	struct pkg_manifest *manifest;

	file = pkgfile_new_regular("+CONTENTS", pkg_data, strlen(pkg_data));
	manifest = pkg_manifest_new_freebsd_pkgfile(file);
	fail_unless(manifest == NULL);

	pkgfile_free(file);
}
END_TEST

START_TEST(pkg_manifest_freebsd_bad_empty_exec_test)
{
	const char *pkg_data = pkg_manifest_default "@exec\n";
	struct pkgfile *file;
	struct pkg_manifest *manifest;

	file = pkgfile_new_regular("+CONTENTS", pkg_data, strlen(pkg_data));
	manifest = pkg_manifest_new_freebsd_pkgfile(file);
	fail_unless(manifest == NULL);

	pkgfile_free(file);
}
END_TEST

START_TEST(pkg_manifest_freebsd_bad_empty_unexec_test)
{
	const char *pkg_data = pkg_manifest_default "@unexec\n";
	struct pkgfile *file;
	struct pkg_manifest *manifest;

	file = pkgfile_new_regular("+CONTENTS", pkg_data, strlen(pkg_data));
	manifest = pkg_manifest_new_freebsd_pkgfile(file);
	fail_unless(manifest == NULL);

	pkgfile_free(file);
}
END_TEST

START_TEST(pkg_manifest_freebsd_bad_empty_dirrm_test)
{
	const char *pkg_data = pkg_manifest_default "@dirrm\n";
	struct pkgfile *file;
	struct pkg_manifest *manifest;

	file = pkgfile_new_regular("+CONTENTS", pkg_data, strlen(pkg_data));
	manifest = pkg_manifest_new_freebsd_pkgfile(file);
	fail_unless(manifest == NULL);

	pkgfile_free(file);
}
END_TEST

START_TEST(pkg_manifest_freebsd_bad_empty_mtree_test)
{
	const char *pkg_data = pkg_manifest_default "@mtree\n";
	struct pkgfile *file;
	struct pkg_manifest *manifest;

	file = pkgfile_new_regular("+CONTENTS", pkg_data, strlen(pkg_data));
	manifest = pkg_manifest_new_freebsd_pkgfile(file);
	fail_unless(manifest == NULL);

	pkgfile_free(file);
}
END_TEST

START_TEST(pkg_manifest_freebsd_bad_empty_display_test)
{
	const char *pkg_data = pkg_manifest_default "@display\n";
	struct pkgfile *file;
	struct pkg_manifest *manifest;

	file = pkgfile_new_regular("+CONTENTS", pkg_data, strlen(pkg_data));
	manifest = pkg_manifest_new_freebsd_pkgfile(file);
	fail_unless(manifest == NULL);

	pkgfile_free(file);
}
END_TEST

/*
 * Test if a command on the second line with no contents fails
 */
START_TEST(pkg_manifest_freebsd_bad_empty2_comment_test)
{
	const char *pkg_data = pkg_manifest_default "@ignore\n@comment\n";
	struct pkgfile *file;
	struct pkg_manifest *manifest;

	file = pkgfile_new_regular("+CONTENTS", pkg_data, strlen(pkg_data));
	manifest = pkg_manifest_new_freebsd_pkgfile(file);
	fail_unless(manifest == NULL);

	pkgfile_free(file);
}
END_TEST

START_TEST(pkg_manifest_freebsd_bad_empty2_cwd_test)
{
	const char *pkg_data = pkg_manifest_default "@ignore\n@cwd\n";
	struct pkgfile *file;
	struct pkg_manifest *manifest;

	file = pkgfile_new_regular("+CONTENTS", pkg_data, strlen(pkg_data));
	manifest = pkg_manifest_new_freebsd_pkgfile(file);
	fail_unless(manifest == NULL);

	pkgfile_free(file);
}
END_TEST

START_TEST(pkg_manifest_freebsd_bad_empty2_pkgdep_test)
{
	const char *pkg_data = pkg_manifest_default "@ignore\n@pkgdep\n";
	struct pkgfile *file;
	struct pkg_manifest *manifest;

	file = pkgfile_new_regular("+CONTENTS", pkg_data, strlen(pkg_data));
	manifest = pkg_manifest_new_freebsd_pkgfile(file);
	fail_unless(manifest == NULL);

	pkgfile_free(file);
}
END_TEST

START_TEST(pkg_manifest_freebsd_bad_empty2_conflicts_test)
{
	const char *pkg_data = pkg_manifest_default "@ignore\n@conflicts\n";
	struct pkgfile *file;
	struct pkg_manifest *manifest;

	file = pkgfile_new_regular("+CONTENTS", pkg_data, strlen(pkg_data));
	manifest = pkg_manifest_new_freebsd_pkgfile(file);
	fail_unless(manifest == NULL);

	pkgfile_free(file);
}
END_TEST

START_TEST(pkg_manifest_freebsd_bad_empty2_exec_test)
{
	const char *pkg_data = pkg_manifest_default "@ignore\n@exec\n";
	struct pkgfile *file;
	struct pkg_manifest *manifest;

	file = pkgfile_new_regular("+CONTENTS", pkg_data, strlen(pkg_data));
	manifest = pkg_manifest_new_freebsd_pkgfile(file);
	fail_unless(manifest == NULL);

	pkgfile_free(file);
}
END_TEST

START_TEST(pkg_manifest_freebsd_bad_empty2_unexec_test)
{
	const char *pkg_data = pkg_manifest_default "@ignore\n@unexec\n";
	struct pkgfile *file;
	struct pkg_manifest *manifest;

	file = pkgfile_new_regular("+CONTENTS", pkg_data, strlen(pkg_data));
	manifest = pkg_manifest_new_freebsd_pkgfile(file);
	fail_unless(manifest == NULL);

	pkgfile_free(file);
}
END_TEST

START_TEST(pkg_manifest_freebsd_bad_empty2_dirrm_test)
{
	const char *pkg_data = pkg_manifest_default "@ignore\n@dirrm\n";
	struct pkgfile *file;
	struct pkg_manifest *manifest;

	file = pkgfile_new_regular("+CONTENTS", pkg_data, strlen(pkg_data));
	manifest = pkg_manifest_new_freebsd_pkgfile(file);
	fail_unless(manifest == NULL);

	pkgfile_free(file);
}
END_TEST

START_TEST(pkg_manifest_freebsd_bad_empty2_mtree_test)
{
	const char *pkg_data = pkg_manifest_default "@ignore\n@mtree\n";
	struct pkgfile *file;
	struct pkg_manifest *manifest;

	file = pkgfile_new_regular("+CONTENTS", pkg_data, strlen(pkg_data));
	manifest = pkg_manifest_new_freebsd_pkgfile(file);
	fail_unless(manifest == NULL);

	pkgfile_free(file);
}
END_TEST

START_TEST(pkg_manifest_freebsd_bad_empty2_display_test)
{
	const char *pkg_data = pkg_manifest_default "@ignore\n@display\n";
	struct pkgfile *file;
	struct pkg_manifest *manifest;

	file = pkgfile_new_regular("+CONTENTS", pkg_data, strlen(pkg_data));
	manifest = pkg_manifest_new_freebsd_pkgfile(file);
	fail_unless(manifest == NULL);

	pkgfile_free(file);
}
END_TEST

Suite *
pkg_manifest_freebsd_suite()
{
	Suite *s;
	TCase *tc;

	s = suite_create("pkg_manifest_freebsd");

	tc = tcase_create("empty");
	tcase_add_test(tc, pkg_manifest_freebsd_empty_test);
	suite_add_tcase(s, tc);

	tc = tcase_create("good");
	tcase_add_test(tc, pkg_manifest_freebsd_good_basic_test);
	tcase_add_test(tc, pkg_manifest_freebsd_good_comment_test);
	tcase_add_test(tc, pkg_manifest_freebsd_good_cwd_test);
	tcase_add_test(tc, pkg_manifest_freebsd_good_exec_test);
	tcase_add_test(tc, pkg_manifest_freebsd_good_unexec_test);
	tcase_add_test(tc, pkg_manifest_freebsd_good_dirrm_test);
	tcase_add_test(tc, pkg_manifest_freebsd_good_mtree_test);
	tcase_add_test(tc, pkg_manifest_freebsd_good_display_test);
	tcase_add_test(tc, pkg_manifest_freebsd_good_pkgdep_test);
	tcase_add_test(tc, pkg_manifest_freebsd_good_conflicts_test);
	suite_add_tcase(s, tc);

	tc = tcase_create("bad");
	tcase_add_test(tc, pkg_manifest_freebsd_bad_empty_comment_test);
	tcase_add_test(tc, pkg_manifest_freebsd_bad_empty_cwd_test);
	tcase_add_test(tc, pkg_manifest_freebsd_bad_empty_pkgdep_test);
	tcase_add_test(tc, pkg_manifest_freebsd_bad_empty_conflicts_test);
	tcase_add_test(tc, pkg_manifest_freebsd_bad_empty_exec_test);
	tcase_add_test(tc, pkg_manifest_freebsd_bad_empty_unexec_test);
	tcase_add_test(tc, pkg_manifest_freebsd_bad_empty_dirrm_test);
	tcase_add_test(tc, pkg_manifest_freebsd_bad_empty_mtree_test);
	tcase_add_test(tc, pkg_manifest_freebsd_bad_empty_display_test);

	tcase_add_test(tc, pkg_manifest_freebsd_bad_empty2_comment_test);
	tcase_add_test(tc, pkg_manifest_freebsd_bad_empty2_cwd_test);
	tcase_add_test(tc, pkg_manifest_freebsd_bad_empty2_pkgdep_test);
	tcase_add_test(tc, pkg_manifest_freebsd_bad_empty2_conflicts_test);
	tcase_add_test(tc, pkg_manifest_freebsd_bad_empty2_exec_test);
	tcase_add_test(tc, pkg_manifest_freebsd_bad_empty2_unexec_test);
	tcase_add_test(tc, pkg_manifest_freebsd_bad_empty2_dirrm_test);
	tcase_add_test(tc, pkg_manifest_freebsd_bad_empty2_mtree_test);
	tcase_add_test(tc, pkg_manifest_freebsd_bad_empty2_display_test);
	suite_add_tcase(s, tc);

	return s;
}


