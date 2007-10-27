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
#include <pkg_freebsd.h>
#include <pkg_freebsd_private.h>

#include <string.h>

START_TEST(pkg_freebsd_contents_empty_test)
{
	struct pkg_freebsd_contents *contents;

	contents = pkg_freebsd_contents_new("", 0);
	fail_unless(contents == NULL, NULL);
	pkg_freebsd_contents_free(contents);
}
END_TEST

START_TEST(pkg_freebsd_contents_good_empty_ignore_test)
{
	struct pkg_freebsd_contents *contents;
	const char *pkg_data = "@ignore\n@ignore\n";

	contents = pkg_freebsd_contents_new(pkg_data, strlen(pkg_data));
	fail_unless(contents != NULL, NULL);
	pkg_freebsd_contents_free(contents);
}
END_TEST

/* Test if a minimal config file will work */
START_TEST(pkg_freebsd_contents_good_basic_test)
{
	struct pkg_freebsd_contents *contents;
	const char *pkg_data = "@comment PKG_FORMAT_REVISION:1.1\n"
	    "@name package_name-1.0\n"
	    "@comment ORIGIN:package/origin\n"
	    "@cwd /usr/local\n";

	contents = pkg_freebsd_contents_new(pkg_data, strlen(pkg_data));
	fail_unless(contents != NULL, NULL);
	fail_unless(contents->line_count == 4, NULL);
	fail_unless(contents->lines[0].line_type == PKG_LINE_COMMENT, NULL);
	fail_unless(contents->lines[1].line_type == PKG_LINE_NAME, NULL);
	fail_unless(contents->lines[2].line_type == PKG_LINE_COMMENT, NULL);
	fail_unless(contents->lines[3].line_type == PKG_LINE_CWD, NULL);

	fail_unless(strcmp(contents->lines[0].line, "@comment") == 0, NULL);
	fail_unless(strcmp(contents->lines[1].line, "@name") == 0, NULL);
	fail_unless(strcmp(contents->lines[2].line, "@comment") == 0, NULL);
	fail_unless(strcmp(contents->lines[3].line, "@cwd") == 0, NULL);

	fail_unless(strcmp(contents->lines[0].data, "PKG_FORMAT_REVISION:1.1")
	    == 0, NULL);
	fail_unless(strcmp(contents->lines[1].data, "package_name-1.0") == 0,
	    NULL);
	fail_unless(strcmp(contents->lines[2].data, "ORIGIN:package/origin")
	    == 0, NULL);
	fail_unless(strcmp(contents->lines[3].data, "/usr/local") == 0, NULL);
	pkg_freebsd_contents_free(contents);
}
END_TEST

static void
check_good_command(struct pkg_freebsd_contents *contents, int line_type)
{
	fail_unless(contents != NULL, NULL);
	fail_unless(contents->line_count == 2, NULL);
	fail_unless(contents->lines[1].line_type == line_type, NULL);
	fail_unless(strcmp(contents->lines[1].line,
	    pkg_freebsd_contents_line_str[line_type]) == 0, NULL);
	fail_unless(strcmp(contents->lines[1].data, "data") == 0, NULL);
}

/*
 * Check a command with no data fails
 */
START_TEST(pkg_freebsd_contents_good_comment_test)
{
	struct pkg_freebsd_contents *contents;
	const char *pkg_data = "@comment PKG_FORMAT_REVISION:1.1\n@comment data\n";

	contents = pkg_freebsd_contents_new(pkg_data, strlen(pkg_data));
	check_good_command(contents, PKG_LINE_COMMENT);
	pkg_freebsd_contents_free(contents);
}
END_TEST

START_TEST(pkg_freebsd_contents_good_name_test)
{
	struct pkg_freebsd_contents *contents;
	const char *pkg_data = "@comment PKG_FORMAT_REVISION:1.1\n@name data\n";

	contents = pkg_freebsd_contents_new(pkg_data, strlen(pkg_data));
	check_good_command(contents, PKG_LINE_NAME);
	pkg_freebsd_contents_free(contents);
}
END_TEST

START_TEST(pkg_freebsd_contents_good_cwd_test)
{
	struct pkg_freebsd_contents *contents;
	const char *pkg_data = "@comment PKG_FORMAT_REVISION:1.1\n@cwd data\n";

	contents = pkg_freebsd_contents_new(pkg_data, strlen(pkg_data));
	check_good_command(contents, PKG_LINE_CWD);
	pkg_freebsd_contents_free(contents);
}
END_TEST

START_TEST(pkg_freebsd_contents_good_pkgdep_test)
{
	struct pkg_freebsd_contents *contents;
	const char *pkg_data = "@comment PKG_FORMAT_REVISION:1.1\n@pkgdep data\n";

	contents = pkg_freebsd_contents_new(pkg_data, strlen(pkg_data));
	check_good_command(contents, PKG_LINE_PKGDEP);
	pkg_freebsd_contents_free(contents);
}
END_TEST

START_TEST(pkg_freebsd_contents_good_conflicts_test)
{
	struct pkg_freebsd_contents *contents;
	const char *pkg_data = "@comment PKG_FORMAT_REVISION:1.1\n@conflicts data\n";

	contents = pkg_freebsd_contents_new(pkg_data, strlen(pkg_data));
	check_good_command(contents, PKG_LINE_CONFLICTS);
	pkg_freebsd_contents_free(contents);
}
END_TEST

START_TEST(pkg_freebsd_contents_good_exec_test)
{
	struct pkg_freebsd_contents *contents;
	const char *pkg_data = "@comment PKG_FORMAT_REVISION:1.1\n@exec data\n";

	contents = pkg_freebsd_contents_new(pkg_data, strlen(pkg_data));
	check_good_command(contents, PKG_LINE_EXEC);
	pkg_freebsd_contents_free(contents);
}
END_TEST

START_TEST(pkg_freebsd_contents_good_unexec_test)
{
	struct pkg_freebsd_contents *contents;
	const char *pkg_data = "@comment PKG_FORMAT_REVISION:1.1\n@unexec data\n";

	contents = pkg_freebsd_contents_new(pkg_data, strlen(pkg_data));
	check_good_command(contents, PKG_LINE_UNEXEC);
	pkg_freebsd_contents_free(contents);
}
END_TEST

START_TEST(pkg_freebsd_contents_good_dirrm_test)
{
	struct pkg_freebsd_contents *contents;
	const char *pkg_data = "@comment PKG_FORMAT_REVISION:1.1\n@dirrm data\n";

	contents = pkg_freebsd_contents_new(pkg_data, strlen(pkg_data));
	check_good_command(contents, PKG_LINE_DIRRM);
	pkg_freebsd_contents_free(contents);
}
END_TEST

START_TEST(pkg_freebsd_contents_good_mtree_test)
{
	struct pkg_freebsd_contents *contents;
	const char *pkg_data = "@comment PKG_FORMAT_REVISION:1.1\n@mtree data\n";

	contents = pkg_freebsd_contents_new(pkg_data, strlen(pkg_data));
	check_good_command(contents, PKG_LINE_MTREE);
	pkg_freebsd_contents_free(contents);
}
END_TEST

START_TEST(pkg_freebsd_contents_good_display_test)
{
	struct pkg_freebsd_contents *contents;
	const char *pkg_data = "@comment PKG_FORMAT_REVISION:1.1\n@display data\n";

	contents = pkg_freebsd_contents_new(pkg_data, strlen(pkg_data));
	check_good_command(contents, PKG_LINE_DISPLAY);
	pkg_freebsd_contents_free(contents);
}
END_TEST

/*
 * Check a command with no data fails
 */
START_TEST(pkg_freebsd_contents_bad_empty_comment_test)
{
	struct pkg_freebsd_contents *contents;
	const char *pkg_data = "@comment PKG_FORMAT_REVISION:1.1\n@comment\n";

	contents = pkg_freebsd_contents_new(pkg_data, strlen(pkg_data));
	fail_unless(contents == NULL, NULL);
}
END_TEST

START_TEST(pkg_freebsd_contents_bad_empty_name_test)
{
	struct pkg_freebsd_contents *contents;
	const char *pkg_data = "@comment PKG_FORMAT_REVISION:1.1\n@name\n";

	contents = pkg_freebsd_contents_new(pkg_data, strlen(pkg_data));
	fail_unless(contents == NULL, NULL);
}
END_TEST

START_TEST(pkg_freebsd_contents_bad_empty_cwd_test)
{
	struct pkg_freebsd_contents *contents;
	const char *pkg_data = "@comment PKG_FORMAT_REVISION:1.1\n@cwd\n";

	contents = pkg_freebsd_contents_new(pkg_data, strlen(pkg_data));
	fail_unless(contents == NULL, NULL);
}
END_TEST

START_TEST(pkg_freebsd_contents_bad_empty_pkgdep_test)
{
	struct pkg_freebsd_contents *contents;
	const char *pkg_data = "@comment PKG_FORMAT_REVISION:1.1\n@pkgdep\n";

	contents = pkg_freebsd_contents_new(pkg_data, strlen(pkg_data));
	fail_unless(contents == NULL, NULL);
}
END_TEST

START_TEST(pkg_freebsd_contents_bad_empty_conflicts_test)
{
	struct pkg_freebsd_contents *contents;
	const char *pkg_data = "@comment PKG_FORMAT_REVISION:1.1\n@conflicts\n";

	contents = pkg_freebsd_contents_new(pkg_data, strlen(pkg_data));
	fail_unless(contents == NULL, NULL);
}
END_TEST

START_TEST(pkg_freebsd_contents_bad_empty_exec_test)
{
	struct pkg_freebsd_contents *contents;
	const char *pkg_data = "@comment PKG_FORMAT_REVISION:1.1\n@exec\n";

	contents = pkg_freebsd_contents_new(pkg_data, strlen(pkg_data));
	fail_unless(contents == NULL, NULL);
}
END_TEST

START_TEST(pkg_freebsd_contents_bad_empty_unexec_test)
{
	struct pkg_freebsd_contents *contents;
	const char *pkg_data = "@comment PKG_FORMAT_REVISION:1.1\n@unexec\n";

	contents = pkg_freebsd_contents_new(pkg_data, strlen(pkg_data));
	fail_unless(contents == NULL, NULL);
}
END_TEST

START_TEST(pkg_freebsd_contents_bad_empty_dirrm_test)
{
	struct pkg_freebsd_contents *contents;
	const char *pkg_data = "@comment PKG_FORMAT_REVISION:1.1\n@dirrm\n";

	contents = pkg_freebsd_contents_new(pkg_data, strlen(pkg_data));
	fail_unless(contents == NULL, NULL);
}
END_TEST

START_TEST(pkg_freebsd_contents_bad_empty_mtree_test)
{
	struct pkg_freebsd_contents *contents;
	const char *pkg_data = "@comment PKG_FORMAT_REVISION:1.1\n@mtree\n";

	contents = pkg_freebsd_contents_new(pkg_data, strlen(pkg_data));
	fail_unless(contents == NULL, NULL);
}
END_TEST

START_TEST(pkg_freebsd_contents_bad_empty_display_test)
{
	struct pkg_freebsd_contents *contents;
	const char *pkg_data = "@comment PKG_FORMAT_REVISION:1.1\n@display\n";

	contents = pkg_freebsd_contents_new(pkg_data, strlen(pkg_data));
	fail_unless(contents == NULL, NULL);
}
END_TEST

/*
 * Test if a command on the second line with no contents fails
 */
START_TEST(pkg_freebsd_contents_bad_empty2_comment_test)
{
	struct pkg_freebsd_contents *contents;
	const char *pkg_data = "@comment PKG_FORMAT_REVISION:1.1\n@ignore\n@comment\n";

	contents = pkg_freebsd_contents_new(pkg_data, strlen(pkg_data));
	fail_unless(contents == NULL, NULL);
}
END_TEST

START_TEST(pkg_freebsd_contents_bad_empty2_name_test)
{
	struct pkg_freebsd_contents *contents;
	const char *pkg_data = "@comment PKG_FORMAT_REVISION:1.1\n@ignore\n@name\n";

	contents = pkg_freebsd_contents_new(pkg_data, strlen(pkg_data));
	fail_unless(contents == NULL, NULL);
}
END_TEST

START_TEST(pkg_freebsd_contents_bad_empty2_cwd_test)
{
	struct pkg_freebsd_contents *contents;
	const char *pkg_data = "@comment PKG_FORMAT_REVISION:1.1\n@ignore\n@cwd\n";

	contents = pkg_freebsd_contents_new(pkg_data, strlen(pkg_data));
	fail_unless(contents == NULL, NULL);
}
END_TEST

START_TEST(pkg_freebsd_contents_bad_empty2_pkgdep_test)
{
	struct pkg_freebsd_contents *contents;
	const char *pkg_data = "@comment PKG_FORMAT_REVISION:1.1\n@ignore\n@pkgdep\n";

	contents = pkg_freebsd_contents_new(pkg_data, strlen(pkg_data));
	fail_unless(contents == NULL, NULL);
}
END_TEST

START_TEST(pkg_freebsd_contents_bad_empty2_conflicts_test)
{
	struct pkg_freebsd_contents *contents;
	const char *pkg_data = "@comment PKG_FORMAT_REVISION:1.1\n@ignore\n@conflicts\n";

	contents = pkg_freebsd_contents_new(pkg_data, strlen(pkg_data));
	fail_unless(contents == NULL, NULL);
}
END_TEST

START_TEST(pkg_freebsd_contents_bad_empty2_exec_test)
{
	struct pkg_freebsd_contents *contents;
	const char *pkg_data = "@comment PKG_FORMAT_REVISION:1.1\n@ignore\n@exec\n";

	contents = pkg_freebsd_contents_new(pkg_data, strlen(pkg_data));
	fail_unless(contents == NULL, NULL);
}
END_TEST

START_TEST(pkg_freebsd_contents_bad_empty2_unexec_test)
{
	struct pkg_freebsd_contents *contents;
	const char *pkg_data = "@comment PKG_FORMAT_REVISION:1.1\n@ignore\n@unexec\n";

	contents = pkg_freebsd_contents_new(pkg_data, strlen(pkg_data));
	fail_unless(contents == NULL, NULL);
}
END_TEST

START_TEST(pkg_freebsd_contents_bad_empty2_dirrm_test)
{
	struct pkg_freebsd_contents *contents;
	const char *pkg_data = "@comment PKG_FORMAT_REVISION:1.1\n@ignore\n@dirrm\n";

	contents = pkg_freebsd_contents_new(pkg_data, strlen(pkg_data));
	fail_unless(contents == NULL, NULL);
}
END_TEST

START_TEST(pkg_freebsd_contents_bad_empty2_mtree_test)
{
	struct pkg_freebsd_contents *contents;
	const char *pkg_data = "@comment PKG_FORMAT_REVISION:1.1\n@ignore\n@mtree\n";

	contents = pkg_freebsd_contents_new(pkg_data, strlen(pkg_data));
	fail_unless(contents == NULL, NULL);
}
END_TEST

START_TEST(pkg_freebsd_contents_bad_empty2_display_test)
{
	struct pkg_freebsd_contents *contents;
	const char *pkg_data = "@comment PKG_FORMAT_REVISION:1.1\n@ignore\n@display\n";

	contents = pkg_freebsd_contents_new(pkg_data, strlen(pkg_data));
	fail_unless(contents == NULL, NULL);
}
END_TEST

Suite *
pkg_freebsd_contents_suite()
{
	Suite *s;
	TCase *tc;

	s = suite_create("pkg_freebsd_contents");

	tc = tcase_create("empty");
	tcase_add_test(tc, pkg_freebsd_contents_empty_test);
	suite_add_tcase(s, tc);


	tc = tcase_create("good");
	tcase_add_test(tc, pkg_freebsd_contents_good_empty_ignore_test);
	tcase_add_test(tc, pkg_freebsd_contents_good_basic_test);

	tcase_add_test(tc, pkg_freebsd_contents_good_comment_test);
	tcase_add_test(tc, pkg_freebsd_contents_good_name_test);
	tcase_add_test(tc, pkg_freebsd_contents_good_cwd_test);
	tcase_add_test(tc, pkg_freebsd_contents_good_pkgdep_test);
	tcase_add_test(tc, pkg_freebsd_contents_good_conflicts_test);
	tcase_add_test(tc, pkg_freebsd_contents_good_exec_test);
	tcase_add_test(tc, pkg_freebsd_contents_good_unexec_test);
	tcase_add_test(tc, pkg_freebsd_contents_good_dirrm_test);
	tcase_add_test(tc, pkg_freebsd_contents_good_mtree_test);
	tcase_add_test(tc, pkg_freebsd_contents_good_display_test);
	suite_add_tcase(s, tc);

	tc = tcase_create("bad");
	tcase_add_test(tc, pkg_freebsd_contents_bad_empty_comment_test);
	tcase_add_test(tc, pkg_freebsd_contents_bad_empty_name_test);
	tcase_add_test(tc, pkg_freebsd_contents_bad_empty_cwd_test);
	tcase_add_test(tc, pkg_freebsd_contents_bad_empty_pkgdep_test);
	tcase_add_test(tc, pkg_freebsd_contents_bad_empty_conflicts_test);
	tcase_add_test(tc, pkg_freebsd_contents_bad_empty_exec_test);
	tcase_add_test(tc, pkg_freebsd_contents_bad_empty_unexec_test);
	tcase_add_test(tc, pkg_freebsd_contents_bad_empty_dirrm_test);
	tcase_add_test(tc, pkg_freebsd_contents_bad_empty_mtree_test);
	tcase_add_test(tc, pkg_freebsd_contents_bad_empty_display_test);

	tcase_add_test(tc, pkg_freebsd_contents_bad_empty2_comment_test);
	tcase_add_test(tc, pkg_freebsd_contents_bad_empty2_name_test);
	tcase_add_test(tc, pkg_freebsd_contents_bad_empty2_cwd_test);
	tcase_add_test(tc, pkg_freebsd_contents_bad_empty2_pkgdep_test);
	tcase_add_test(tc, pkg_freebsd_contents_bad_empty2_conflicts_test);
	tcase_add_test(tc, pkg_freebsd_contents_bad_empty2_exec_test);
	tcase_add_test(tc, pkg_freebsd_contents_bad_empty2_unexec_test);
	tcase_add_test(tc, pkg_freebsd_contents_bad_empty2_dirrm_test);
	tcase_add_test(tc, pkg_freebsd_contents_bad_empty2_mtree_test);
	tcase_add_test(tc, pkg_freebsd_contents_bad_empty2_display_test);
	suite_add_tcase(s, tc);

	return s;
}


