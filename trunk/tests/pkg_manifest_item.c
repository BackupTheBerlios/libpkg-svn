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

#include <string.h>

#include <pkg.h>
#include <pkg_private.h>

static const pkg_manifest_item_attr all_attrs[] =
	{ pmia_other, pmia_ignore, pmia_deinstall, pmia_md5, pmia_max };

void test_manifest_item(pkg_manifest_item_type, const char *);

void
test_manifest_item(pkg_manifest_item_type type, const char *init_data)
{
	struct pkg_manifest_item *item;
	unsigned int i;

	item = pkg_manifest_item_new(type, init_data);
	fail_unless(item != NULL, NULL);
	fail_unless(pkg_manifest_item_get_type(item) == type, NULL);

	/*
	 * First test the data
	 */
	if (init_data == NULL) {
		fail_unless(pkg_manifest_item_get_data(item) == NULL, NULL);
		fail_unless(item->data == NULL, NULL);
	} else {
		fail_unless(strcmp(pkg_manifest_item_get_data(item), init_data)
		    == 0, NULL);
		fail_unless(strcmp(item->data, init_data) == 0, NULL);
	}

	/* Check if setting data works */
	fail_unless(pkg_manifest_item_set_data(item, "data") == 0, NULL);
	fail_unless(strcmp(pkg_manifest_item_get_data(item), "data") == 0,NULL);
	fail_unless(strcmp(item->data, "data") == 0, NULL);

	/* Check if clearing data works */
	fail_unless(pkg_manifest_item_set_data(item, NULL) == 0, NULL);
	fail_unless(pkg_manifest_item_get_data(item) == NULL, NULL);
	fail_unless(item->data == NULL, NULL);

	/* Check each attribute */
	for (i = 0; all_attrs[i] != pmia_max; i++) {
		/* Test if the attribute is empty to begin with */
		fail_unless(pkg_manifest_item_get_attr(item, all_attrs[i]) ==
		    NULL, NULL);

		/* Test if setting attributes works */
		fail_unless(pkg_manifest_item_set_attr(item, all_attrs[i],
		    "data") == 0, NULL);
		fail_unless(strcmp(pkg_manifest_item_get_attr(item,
		    all_attrs[i]), "data") == 0, NULL);

		/* Test if clearing the attributes works */
		fail_unless(pkg_manifest_item_set_attr(item, all_attrs[i], NULL)
		    == 0, NULL);
		fail_unless(pkg_manifest_item_get_attr(item, all_attrs[i]) ==
		    NULL, NULL);
	}

	fail_unless(pkg_manifest_item_free(item) == 0, NULL);
}

START_TEST(pkg_manifest_item_bad)
{
	fail_unless(pkg_manifest_item_free(NULL) == -1, NULL);
	fail_unless(pkg_manifest_item_get_type(NULL) == pmt_error, NULL);
	fail_unless(pkg_manifest_item_get_data(NULL) == NULL, NULL);

	fail_unless(pkg_manifest_item_set_attr(NULL, pmia_other, NULL) == -1,
	    NULL);
	fail_unless(pkg_manifest_item_set_attr(NULL, pmia_ignore, NULL) == -1,
	    NULL);
	fail_unless(pkg_manifest_item_set_attr(NULL, pmia_deinstall, NULL) ==-1,
	    NULL);
	fail_unless(pkg_manifest_item_set_attr(NULL, pmia_md5, NULL) == -1,
	    NULL);
	fail_unless(pkg_manifest_item_set_attr(NULL, pmia_max, NULL) == -1,
	    NULL);

	fail_unless(pkg_manifest_item_set_attr(NULL, pmia_other, "data") == -1,
	    NULL);
	fail_unless(pkg_manifest_item_set_attr(NULL, pmia_ignore, "data") == -1,
	    NULL);
	fail_unless(pkg_manifest_item_set_attr(NULL, pmia_deinstall, "data")
	    == -1, NULL);
	fail_unless(pkg_manifest_item_set_attr(NULL, pmia_md5, "data") == -1,
	    NULL);
	fail_unless(pkg_manifest_item_set_attr(NULL, pmia_max, "data") == -1,
	    NULL);

	fail_unless(pkg_manifest_item_get_attr(NULL, pmia_other) == NULL, NULL);
	fail_unless(pkg_manifest_item_get_attr(NULL, pmia_ignore) == NULL,NULL);
	fail_unless(pkg_manifest_item_get_attr(NULL, pmia_deinstall) == NULL,
	    NULL);
	fail_unless(pkg_manifest_item_get_attr(NULL, pmia_md5) == NULL, NULL);
	fail_unless(pkg_manifest_item_get_attr(NULL, pmia_max) == NULL, NULL);

	fail_unless(pkg_manifest_item_set_data(NULL, NULL) == -1, NULL);
	fail_unless(pkg_manifest_item_set_data(NULL, "data") == -1, NULL);
}
END_TEST

START_TEST(pkg_manifest_item_other_null)
{
	test_manifest_item(pmt_other, NULL);
}
END_TEST

START_TEST(pkg_manifest_item_other_data)
{
	test_manifest_item(pmt_other, "init data");
}
END_TEST


START_TEST(pkg_manifest_item_file_null)
{
	test_manifest_item(pmt_file, NULL);
}
END_TEST

START_TEST(pkg_manifest_item_file_data)
{
	test_manifest_item(pmt_file, "init data");
}
END_TEST


START_TEST(pkg_manifest_item_dir_null)
{
	test_manifest_item(pmt_dir, NULL);
}
END_TEST

START_TEST(pkg_manifest_item_dir_data)
{
	test_manifest_item(pmt_dir, "init data");
}
END_TEST


START_TEST(pkg_manifest_item_dirlist_null)
{
	test_manifest_item(pmt_dirlist, NULL);
}
END_TEST

START_TEST(pkg_manifest_item_dirlist_data)
{
	test_manifest_item(pmt_dirlist, "init data");
}
END_TEST


START_TEST(pkg_manifest_item_chdir_null)
{
	test_manifest_item(pmt_chdir, NULL);
}
END_TEST

START_TEST(pkg_manifest_item_chdir_data)
{
	test_manifest_item(pmt_chdir, "init data");
}
END_TEST


START_TEST(pkg_manifest_item_output_null)
{
	test_manifest_item(pmt_output, NULL);
}
END_TEST

START_TEST(pkg_manifest_item_output_data)
{
	test_manifest_item(pmt_output, "init data");
}
END_TEST


START_TEST(pkg_manifest_item_comment_null)
{
	test_manifest_item(pmt_comment, NULL);
}
END_TEST

START_TEST(pkg_manifest_item_comment_data)
{
	test_manifest_item(pmt_comment, "init data");
}
END_TEST


START_TEST(pkg_manifest_item_execute_null)
{
	test_manifest_item(pmt_execute, NULL);
}
END_TEST

START_TEST(pkg_manifest_item_execute_data)
{
	test_manifest_item(pmt_execute, "init data");
}
END_TEST

Suite *
pkg_manifest_item_suite()
{
	Suite *s;
	TCase *tc;

	s = suite_create("pkg_manifest_item");

	tc = tcase_create("bad");
	tcase_add_test(tc, pkg_manifest_item_bad);
	suite_add_tcase(s, tc);


	tc = tcase_create("other");
	tcase_add_test(tc, pkg_manifest_item_other_null);
	tcase_add_test(tc, pkg_manifest_item_other_data);
	suite_add_tcase(s, tc);


	tc = tcase_create("file");
	tcase_add_test(tc, pkg_manifest_item_file_null);
	tcase_add_test(tc, pkg_manifest_item_file_data);
	suite_add_tcase(s, tc);


	tc = tcase_create("dir");
	tcase_add_test(tc, pkg_manifest_item_dir_null);
	tcase_add_test(tc, pkg_manifest_item_dir_data);
	suite_add_tcase(s, tc);


	tc = tcase_create("dirlist");
	tcase_add_test(tc, pkg_manifest_item_dirlist_null);
	tcase_add_test(tc, pkg_manifest_item_dirlist_data);
	suite_add_tcase(s, tc);


	tc = tcase_create("chdir");
	tcase_add_test(tc, pkg_manifest_item_chdir_null);
	tcase_add_test(tc, pkg_manifest_item_chdir_data);
	suite_add_tcase(s, tc);


	tc = tcase_create("output");
	tcase_add_test(tc, pkg_manifest_item_output_null);
	tcase_add_test(tc, pkg_manifest_item_output_data);
	suite_add_tcase(s, tc);


	tc = tcase_create("comment");
	tcase_add_test(tc, pkg_manifest_item_comment_null);
	tcase_add_test(tc, pkg_manifest_item_comment_data);
	suite_add_tcase(s, tc);


	tc = tcase_create("execute");
	tcase_add_test(tc, pkg_manifest_item_execute_null);
	tcase_add_test(tc, pkg_manifest_item_execute_data);
	suite_add_tcase(s, tc);

	return s;
}


