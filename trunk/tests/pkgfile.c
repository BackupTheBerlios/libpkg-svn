#include "test.h"

#include <sys/types.h>
#include <sys/stat.h>

#include <stdlib.h>

#include <pkg.h>

int
setup_testdir()
{
	system("rm -fr testdir");
	return system("mkdir testdir");
}

int
cleanup_testdir()
{
	if (system("rmdir testdir") == 0)
		return 0;

	system("rm -fr testdir");
	return 1;
}

#define SETUP_TESTDIR() fail_unless(setup_testdir() == 0, "Couldn't create the test dir")
#define CLEANUP_TESTDIR() fail_unless(cleanup_testdir() == 0, "Couldn't cleanup the test dir")

/* Tests on creating a regular file from a buffer */
START_TEST(pkgfile_regular)
{
	struct pkgfile *file;
	FILE *fd;
	struct stat sb;

	/* Test creating a regular file from bad data fails */
	fail_unless(pkgfile_new_regular(NULL, NULL, 0) == NULL, NULL);
	fail_unless(pkgfile_new_regular(NULL, "", 0) == NULL, NULL);
	fail_unless(pkgfile_new_regular("", NULL, 1) == NULL, NULL);

	/* Create an empty file */
	file = pkgfile_new_regular("testdir/Foo", NULL, 0);
	fail_unless(file != NULL, "pkgfile_new_regular(\"\", NULL, 0) "
	    "returned NULL");
	fail_unless(strcmp(pkgfile_get_name(file), "testdir/Foo") == 0,
	    "pkgfile_get_name didn't return \"testdir/Foo\"");
	fail_unless(pkgfile_get_size(file) == 0,
	    "pkgfile_get_size didn't return 0");
	fail_unless(pkgfile_get_data(file) != NULL,
	    "pkgfile_get_data returned NULL");

	SETUP_TESTDIR();
	fail_unless(pkgfile_write(file) == 0, "pkfile_write returned non zero");
	fd = fopen("testdir/Foo", "r");
	fail_unless(fd != NULL, "Could not open testdir/Foo");
	fstat(fileno(fd), &sb);
	fail_unless(sb.st_size == 0, "Created file size os not zero");
	fclose(fd);
	system("rm testdir/Foo");
	CLEANUP_TESTDIR();

	fail_unless(pkgfile_free(file) == 0, "pkg_free returned non zero");
}
END_TEST

/* Tests on creating a symlink from a buffer */
START_TEST(pkgfile_symlink)
{
	struct pkgfile *file;

	/* Test creating a symlink from bad data fails */
	file = pkgfile_new_symlink(NULL, NULL);
	fail_unless(file == NULL, "pkgfile_new_symlink(NULL, NULL) didn't "
	    "return NULL");

	file = pkgfile_new_symlink("", NULL);
	fail_unless(file == NULL, "pkgfile_new_symlink(\"\", NULL) didn't "
	    "return NULL");

	file = pkgfile_new_symlink(NULL, "");
	fail_unless(file == NULL, "pkgfile_new_symlink(NULL, \"\") didn't "
	    "return NULL");
}
END_TEST

/* Tests on creating a hardlink from a buffer */
START_TEST(pkgfile_hardlink)
{
	struct pkgfile *file;

	/* Test creating a hard link from bad data fails */
	file = pkgfile_new_hardlink(NULL, NULL);
	fail_unless(file == NULL, "pkgfile_new_hardlink(NULL, NULL) didn't "
	    "return NULL");

	file = pkgfile_new_hardlink("", NULL);
	fail_unless(file == NULL, "pkgfile_new_hardlink(\"\", NULL) didn't "
	    "return NULL");

	file = pkgfile_new_hardlink(NULL, "");
	fail_unless(file == NULL, "pkgfile_new_hardlink(NULL, \"\") didn't "
	    "return NULL");
}
END_TEST

/* Tests on creating a directory from a buffer */
START_TEST(pkgfile_directory)
{
	struct pkgfile *file;

	/* Test creating a symlink from bad data fails */
	file = pkgfile_new_directory(NULL);
	fail_unless(file == NULL, "pkgfile_new_directory(NULL) didn't "
	    "return NULL");
}
END_TEST

Suite *
pkgfile_suite()
{
	Suite *s;
	TCase *tc_core;

	s = suite_create("pkgfile");
	tc_core = tcase_create("base");

	suite_add_tcase (s, tc_core);
	tcase_add_test(tc_core, pkgfile_regular);
	tcase_add_test(tc_core, pkgfile_symlink);
	tcase_add_test(tc_core, pkgfile_hardlink);
	tcase_add_test(tc_core, pkgfile_directory);

	return s;
}
