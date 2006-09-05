#include "test.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <pkg.h>
#include <pkg_private.h>

void basic_file_tests(struct pkgfile *, pkgfile_type, pkgfile_loc);
void test_checksums(struct pkgfile *, const char *);
void empty_regular_file_tests(const char *);

/*
 * Check a pkgfile object is correct after it has been created
 */
void
basic_file_tests(struct pkgfile *file, pkgfile_type type, pkgfile_loc loc)
{
	fail_unless(file->loc == loc, NULL);
	fail_unless(file->type == type, NULL);
	fail_unless(file->fd == NULL, NULL);
	fail_unless(file->mode == 0, NULL);
	fail_unless(file->md5[0] == '\0', NULL);
}

void
test_checksums(struct pkgfile *file, const char *md5)
{
	fail_unless(strlen(md5) == 32, NULL);
	fail_unless(pkgfile_set_checksum_md5(file, md5) == 0, NULL);
	fail_unless(strcmp(file->md5, md5) == 0, NULL);
	fail_unless(pkgfile_compare_checksum_md5(file) == 0, NULL);

	/* Check this fails with bad data that is too short */
	fail_unless(pkgfile_set_checksum_md5(file, "") == -1, NULL);
	fail_unless(strcmp(file->md5, md5) == 0, NULL);

	/* Check this fails with bad data that is too long */
	fail_unless(pkgfile_set_checksum_md5(file,
	    "123456789012345678901234567890123") == -1, NULL);
	fail_unless(strcmp(file->md5, md5) == 0, NULL);

	/*
	 * Check it accepts a correct length checksum
	 * but fails to validate the file with it
	 */
	fail_unless(pkgfile_set_checksum_md5(file,
		"12345678901234567890123456789012") == 0, NULL);
	fail_unless(strcmp(file->md5, "12345678901234567890123456789012") == 0,
		NULL);
	fail_unless(pkgfile_compare_checksum_md5(file) == 1, NULL);
}

void
empty_regular_file_tests(const char *buf)
{
	struct pkgfile *file;
	FILE *fd;
	struct stat sb;

	fail_unless((file = pkgfile_new_regular("testdir/Foo", buf, 0))
	    != NULL, NULL);
	fail_unless(strcmp(pkgfile_get_name(file), "testdir/Foo") == 0, NULL);
	fail_unless(strcmp(file->name, "testdir/Foo") == 0, NULL);
	basic_file_tests(file, pkgfile_regular, pkgfile_loc_mem);

	fail_unless(pkgfile_get_size(file) == 0, NULL);
	fail_unless(pkgfile_get_data(file) == NULL, NULL);

	/* The md5 of an empty string is d41d8cd98f00b204e9800998ecf8427e */
	test_checksums(file, "d41d8cd98f00b204e9800998ecf8427e");

	SETUP_TESTDIR();
	fail_unless(pkgfile_write(file) == 0, NULL);
	fail_unless((fd = fopen("testdir/Foo", "r")) != NULL, NULL);
	fstat(fileno(fd), &sb);
	fail_unless(sb.st_size == 0, "Created file size is not zero");
	/* XXX Check the file contents are correct */
	fclose(fd);
	system("rm testdir/Foo");
	CLEANUP_TESTDIR();

	fail_unless(pkgfile_free(file) == 0, "pkg_free returned non zero");
}

/* Tests on creating a regular file from a buffer */
START_TEST(pkgfile_regular_bad_test)
{
	/* Test creating a regular file from bad data fails */
	fail_unless(pkgfile_new_regular(NULL, NULL, 0) == NULL, NULL);
	fail_unless(pkgfile_new_regular(NULL, "", 0) == NULL, NULL);
	fail_unless(pkgfile_new_regular("", NULL, 1) == NULL, NULL);
}
END_TEST

START_TEST(pkgfile_regular_empty_test)
{
	/* Create an empty file with a NULL argument */
	empty_regular_file_tests(NULL);

	/* Create an empty file with a "" argument */
	empty_regular_file_tests("");
}
END_TEST

START_TEST(pkgfile_regular_data_test)
{
	struct pkgfile *file;
	FILE *fd;
	struct stat sb;

	/* Create a file with data */
	fail_unless((file = pkgfile_new_regular("testdir/Foo2",
	    "0123456789", 10)) != NULL, NULL);
	fail_unless(strcmp(pkgfile_get_name(file), "testdir/Foo2") == 0, NULL);
	fail_unless(strcmp(file->name, "testdir/Foo2") == 0, NULL);
	basic_file_tests(file, pkgfile_regular, pkgfile_loc_mem);

	/* Test the file length */
	fail_unless(pkgfile_get_size(file) == 10, NULL);
	fail_unless(file->length == 10, NULL);

	/* Test the data */
	fail_unless(pkgfile_get_data(file) != NULL, NULL);
	fail_unless(strncmp(pkgfile_get_data(file), "0123456789", 10)== 0,NULL);
	fail_unless(strncmp(file->data, "0123456789", 10) == 0, NULL);

	/* The md5 of 0123456789 string is 781e5e245d69b566979b86e28d23f2c7 */
	test_checksums(file, "781e5e245d69b566979b86e28d23f2c7");

	SETUP_TESTDIR();
	fail_unless(pkgfile_write(file) == 0, NULL);
	fail_unless(pkgfile_write(file) == -1, NULL);
	/* Attempting to over write a file should fail */
	fail_unless(pkgfile_write(file) == -1, NULL);
	fail_unless((fd = fopen("testdir/Foo2", "r")) != NULL, NULL);
	fstat(fileno(fd), &sb);
	fail_unless(S_ISREG(sb.st_mode), NULL);
	fail_unless(sb.st_size == 10, "Created file size is not 10");
	fail_unless(sb.st_nlink == 1, NULL);
	/* XXX Check the file contents are correct */
	fclose(fd);
	system("rm testdir/Foo2");
	CLEANUP_TESTDIR();

	fail_unless(pkgfile_free(file) == 0, "pkg_free returned non zero");
}
END_TEST

START_TEST(pkgfile_regular_existing_test)
{
	struct pkgfile *file;
	FILE *fd;
	char buf[6];

	/* Test if pkgfile_write will fail with a regular file */
	file = pkgfile_new_regular("testdir/Foo", "0123456789", 10);
	SETUP_TESTDIR();
	system("echo Hello > testdir/Foo");
	fail_unless(pkgfile_write(file) == -1, NULL);
	fd = fopen("testdir/Foo", "r");
	fread(buf, 5, 1, fd);
	buf[5] = '\0';
	/* Check the file has not been touched */
	fail_unless(strcmp(buf, "Hello") == 0, NULL);
	fclose(fd);
	system("rm testdir/Foo");
	CLEANUP_TESTDIR();
	pkgfile_free(file);

	/* Test if pkgfile_write will fail with a symlink */
	file = pkgfile_new_regular("testdir/Foo", "0123456789", 10);
	SETUP_TESTDIR();
	symlink("testdir/Bar", "testdir/Foo");
	fail_unless(pkgfile_write(file) == -1, NULL);
	system("rm testdir/Foo");
	CLEANUP_TESTDIR();
	pkgfile_free(file);

	/* Test if pkgfile_write will fail with a directory */
	file = pkgfile_new_regular("testdir/Foo", "0123456789", 10);
	SETUP_TESTDIR();
	system("mkdir testdir/Foo");
	fail_unless(pkgfile_write(file) == -1, NULL);
	system("rmdir testdir/Foo");
	CLEANUP_TESTDIR();
	pkgfile_free(file);
}
END_TEST

/*
 * A test to make sure the pkgfile_write will
 * create the parent directories required
 */
START_TEST(pkgfile_regular_depth_test)
{
	struct pkgfile *file;
	FILE *fd;
	char buf[11];

	file = pkgfile_new_regular("testdir/foo/bar", "0123456789", 10);
	SETUP_TESTDIR();
	fail_unless(pkgfile_write(file) == 0, NULL);
	fd = fopen("testdir/foo/bar", "r");
	fread(buf, 10, 1, fd);
	buf[10] = '\0';
	/* Check the file has been written correctly */
	fail_unless(strcmp(buf, "0123456789") == 0, NULL);
	fclose(fd);
	system("rm testdir/foo/bar");
	system("rmdir testdir/foo");
	CLEANUP_TESTDIR();
	pkgfile_free(file);
}
END_TEST

/* Tests on creating a symlink from a buffer */
START_TEST(pkgfile_symlink_bad_test)
{
	/* Test creating a symlink from bad data fails */
	fail_unless(pkgfile_new_symlink(NULL, NULL) == NULL, NULL);
	fail_unless(pkgfile_new_symlink("", NULL) == NULL, NULL);
	fail_unless(pkgfile_new_symlink(NULL, "") == NULL, NULL);
}
END_TEST

START_TEST(pkgfile_symlink_good_test)
{
	struct pkgfile *file;
	struct stat sb;

	fail_unless((file = pkgfile_new_symlink("testdir/link", "Foo")) != NULL,
	    NULL);
	basic_file_tests(file, pkgfile_symlink, pkgfile_loc_mem);

	/* Test the file length */
	fail_unless(pkgfile_get_size(file) == 3, NULL);
	fail_unless(file->length == 3, NULL);

	/* Test the data */
	fail_unless(pkgfile_get_data(file) != NULL, NULL);
	fail_unless(strcmp(pkgfile_get_data(file), "Foo") == 0, NULL);
	fail_unless(strcmp(file->data, "Foo") == 0, NULL);

	/* The md5 of Foo is 1356c67d7ad1638d816bfb822dd2c25d */
	test_checksums(file, "1356c67d7ad1638d816bfb822dd2c25d");

	SETUP_TESTDIR();
	fail_unless(pkgfile_write(file) == 0, NULL);
	/* XXX Check the file contents are correct */
	fail_unless(lstat("testdir/link", &sb) == 0, NULL);
	fail_unless(S_ISLNK(sb.st_mode), NULL);
	fail_unless(sb.st_size == 3, "Created file size is not 3");
	system("rm testdir/link");
	CLEANUP_TESTDIR();

	fail_unless(pkgfile_free(file) == 0, NULL);
}
END_TEST

START_TEST(pkgfile_symlink_existing_test)
{
	struct pkgfile *file;
	FILE *fd;
	char buf[6];

	/* Test if pkgfile_write will fail with a regular file */
	file = pkgfile_new_symlink("testdir/Foo", "0123456789");
	SETUP_TESTDIR();
	system("echo Hello > testdir/Foo");
	fail_unless(pkgfile_write(file) == -1, NULL);
	fd = fopen("testdir/Foo", "r");
	fread(buf, 5, 1, fd);
	buf[5] = '\0';
	/* Check the file has not been touched */
	fail_unless(strcmp(buf, "Hello") == 0, NULL);
	fclose(fd);
	system("rm testdir/Foo");
	CLEANUP_TESTDIR();
	pkgfile_free(file);

	/* Test if pkgfile_write will fail with a symlink */
	file = pkgfile_new_symlink("testdir/Foo", "0123456789");
	SETUP_TESTDIR();
	symlink("testdir/Bar", "testdir/Foo");
	fail_unless(pkgfile_write(file) == -1, NULL);
	system("rm testdir/Foo");
	CLEANUP_TESTDIR();
	pkgfile_free(file);

	/* Test if pkgfile_write will fail with a directory */
	file = pkgfile_new_symlink("testdir/Foo", "0123456789");
	SETUP_TESTDIR();
	system("mkdir testdir/Foo");
	fail_unless(pkgfile_write(file) == -1, NULL);
	system("rmdir testdir/Foo");
	CLEANUP_TESTDIR();
	pkgfile_free(file);
}
END_TEST

/*
 * A test to make sure the pkgfile_write will
 * create the parent directories required
 */
START_TEST(pkgfile_symlink_depth_test)
{
	struct pkgfile *file;
	struct stat sb;

	file = pkgfile_new_symlink("testdir/foo/bar", "0123456789");
	SETUP_TESTDIR();
	fail_unless(pkgfile_write(file) == 0, NULL);
	fail_unless(lstat("testdir/foo/bar", &sb) == 0, NULL);
	fail_unless(S_ISLNK(sb.st_mode), NULL);
	system("rm testdir/foo/bar");
	system("rmdir testdir/foo");
	CLEANUP_TESTDIR();
	pkgfile_free(file);

	/*
	 * Check pkgfile_write fails when there
	 * is already a file named testdir/foo
	 */
	file = pkgfile_new_symlink("testdir/foo/bar", "0123456789");
	SETUP_TESTDIR();
	system("touch testdir/foo");
	fail_unless(pkgfile_write(file) == -1, NULL);
	system("rm testdir/foo");
	CLEANUP_TESTDIR();
	pkgfile_free(file);
}
END_TEST

/* Tests on creating a hardlink from a buffer */
START_TEST(pkgfile_hardlink_bad_test)
{
	/* Test creating a hard link from bad data fails */
	fail_unless(pkgfile_new_hardlink(NULL, NULL) == NULL, NULL);
	fail_unless(pkgfile_new_hardlink("", NULL) == NULL, NULL);
	fail_unless(pkgfile_new_hardlink(NULL, "") == NULL, NULL);
}
END_TEST

START_TEST(pkgfile_hardlink_test)
{
	struct pkgfile *file;
	struct stat sb;

	fail_unless((file = pkgfile_new_hardlink("testdir/Foo", "testdir/Bar"))
	    != NULL, NULL);
	basic_file_tests(file, pkgfile_hardlink, pkgfile_loc_mem);

	/* Test the file length */
	fail_unless(pkgfile_get_size(file) == 11, NULL);
	fail_unless(file->length == 11, NULL);

	/* Test the data */
	fail_unless(pkgfile_get_data(file) != NULL, NULL);
	fail_unless(strcmp(pkgfile_get_data(file), "testdir/Bar") == 0, NULL);
	fail_unless(strcmp(file->data, "testdir/Bar") == 0, NULL);

	SETUP_TESTDIR();
	system("touch testdir/Bar");	
	/*
	 * pkgfile_compare_checksum_md5 will compare
	 * against the file pointed to by the hardlink
	 */
	test_checksums(file, "d41d8cd98f00b204e9800998ecf8427e");

	fail_unless(pkgfile_write(file) == 0, NULL);
	fail_unless(stat("testdir/Foo", &sb) == 0, NULL);
	fail_unless(S_ISREG(sb.st_mode), NULL);
	fail_unless(sb.st_nlink == 2, NULL);
	system("rm testdir/Foo");
	system("rm testdir/Bar");
	CLEANUP_TESTDIR();

	fail_unless(pkgfile_free(file) == 0, NULL);
}
END_TEST

START_TEST(pkgfile_hardlink_existing_test)
{
	struct pkgfile *file;
	FILE *fd;
	char buf[6];

	/* Test if pkgfile_write will fail with a regular file */
	file = pkgfile_new_hardlink("testdir/Foo", "testdir/Bar");
	SETUP_TESTDIR();
	system("touch testdir/Bar");
	system("echo Hello > testdir/Foo");
	fail_unless(pkgfile_write(file) == -1, NULL);
	fd = fopen("testdir/Foo", "r");
	fread(buf, 5, 1, fd);
	buf[5] = '\0';
	/* Check the file has not been touched */
	fail_unless(strcmp(buf, "Hello") == 0, NULL);
	fclose(fd);
	system("rm testdir/Foo");
	system("rm testdir/Bar");
	CLEANUP_TESTDIR();
	pkgfile_free(file);

	/* Test if pkgfile_write will fail with a symlink */
	file = pkgfile_new_hardlink("testdir/Foo", "testdir/Bar");
	SETUP_TESTDIR();
	system("touch testdir/Bar");
	symlink("testdir/Baz", "testdir/Foo");
	fail_unless(pkgfile_write(file) == -1, NULL);
	system("rm testdir/Foo");
	system("rm testdir/Bar");
	CLEANUP_TESTDIR();
	pkgfile_free(file);

	/* Test if pkgfile_write will fail with a directory */
	file = pkgfile_new_hardlink("testdir/Foo", "testdir/Bar");
	SETUP_TESTDIR();
	system("touch testdir/Bar");
	system("mkdir testdir/Foo");
	fail_unless(pkgfile_write(file) == -1, NULL);
	system("rmdir testdir/Foo");
	system("rm testdir/Bar");
	CLEANUP_TESTDIR();
	pkgfile_free(file);
}
END_TEST

/*
 * A test to make sure the pkgfile_write will
 * create the parent directories required
 */
START_TEST(pkgfile_hardlink_depth_test)
{
	struct pkgfile *file;
	struct stat sb;

	file = pkgfile_new_hardlink("testdir/foo/bar", "testdir/Bar");
	SETUP_TESTDIR();
	system("touch testdir/Bar");
	fail_unless(pkgfile_write(file) == 0, NULL);
	fail_unless(lstat("testdir/foo/bar", &sb) == 0, NULL);
	fail_unless(S_ISREG(sb.st_mode), NULL);
	system("rm testdir/foo/bar");
	system("rmdir testdir/foo");
	system("rm testdir/Bar");
	CLEANUP_TESTDIR();
	pkgfile_free(file);

	/*
	 * Check pkgfile_write fails when there
	 * is already a file named testdir/foo
	 */
	file = pkgfile_new_hardlink("testdir/foo/bar", "testdir/Bar");
	SETUP_TESTDIR();
	system("touch testdir/Bar");
	system("touch testdir/foo");
	fail_unless(pkgfile_write(file) == -1, NULL);
	system("rm testdir/foo");
	system("rm testdir/Bar");
	CLEANUP_TESTDIR();
	pkgfile_free(file);
}
END_TEST

/* Tests on creating a directory from a buffer */
START_TEST(pkgfile_directory_bad_test)
{
	/* Test creating a directory from bad data fails */
	fail_unless(pkgfile_new_directory(NULL) == NULL, NULL);
}
END_TEST

START_TEST(pkgfile_directory_test)
{
	struct pkgfile *file;
	struct stat sb;

	fail_unless((file = pkgfile_new_directory("testdir/newdir")) != NULL,
	    NULL);
	basic_file_tests(file, pkgfile_dir, pkgfile_loc_mem);

	/* Test the file length */
	fail_unless(pkgfile_get_size(file) == 14, NULL);
	fail_unless(file->length == 14, NULL);

	/* Test the data */
	fail_unless(pkgfile_get_data(file) != NULL, NULL);
	fail_unless(strcmp(pkgfile_get_data(file), "testdir/newdir") ==0, NULL);

	SETUP_TESTDIR();
	fail_unless(pkgfile_write(file) == 0, NULL);
	/*
	 * A directory should only fail if it is being written
	 * with different permissions than the existing one
	 */
	fail_unless(pkgfile_write(file) == 0, NULL);
	fail_unless(stat("testdir/newdir", &sb) == 0, NULL);
	fail_unless(S_ISDIR(sb.st_mode), NULL);
	system("rmdir testdir/newdir");
	CLEANUP_TESTDIR();
	fail_unless(pkgfile_free(file) == 0, NULL);
}
END_TEST

START_TEST(pkgfile_directory_existing_test)
{
	struct pkgfile *file;
	FILE *fd;
	char buf[6];

	/* Test if pkgfile_write will fail with a regular file */
	file = pkgfile_new_directory("testdir/Foo");
	SETUP_TESTDIR();
	system("echo Hello > testdir/Foo");
	fail_unless(pkgfile_write(file) == -1, NULL);
	fd = fopen("testdir/Foo", "r");
	fread(buf, 5, 1, fd);
	buf[5] = '\0';
	/* Check the file has not been touched */
	fail_unless(strcmp(buf, "Hello") == 0, NULL);
	fclose(fd);
	system("rm testdir/Foo");
	CLEANUP_TESTDIR();
	pkgfile_free(file);

	/* Test if pkgfile_write will fail with a symlink */
	file = pkgfile_new_directory("testdir/Foo");
	SETUP_TESTDIR();
	symlink("testdir/Bar", "testdir/Foo");
	fail_unless(pkgfile_write(file) == -1, NULL);
	system("rm testdir/Foo");
	CLEANUP_TESTDIR();
	pkgfile_free(file);
}
END_TEST

/*
 * A test to make sure the pkgfile_write will
 * create the parent directories required
 */
START_TEST(pkgfile_directory_depth_test)
{
	struct pkgfile *file;
	struct stat sb;

	file = pkgfile_new_directory("testdir/foo/bar");
	SETUP_TESTDIR();
	fail_unless(pkgfile_write(file) == 0, NULL);
	fail_unless(lstat("testdir/foo/bar", &sb) == 0, NULL);
	fail_unless(S_ISDIR(sb.st_mode), NULL);
	system("rmdir testdir/foo/bar");
	system("rmdir testdir/foo");
	CLEANUP_TESTDIR();
	pkgfile_free(file);

	/*
	 * Check pkgfile_write fails when there
	 * is already a file named testdir/foo
	 */
	file = pkgfile_new_directory("testdir/foo/bar");
	SETUP_TESTDIR();
	system("touch testdir/foo");
	fail_unless(pkgfile_write(file) == -1, NULL);
	system("rm testdir/foo");
	CLEANUP_TESTDIR();
	pkgfile_free(file);
}
END_TEST

Suite *
pkgfile_suite()
{
	Suite *s;
	TCase *tc_regular, *tc_symlink, *tc_hardlink, *tc_dir;

	s = suite_create("pkgfile");
	tc_regular = tcase_create("regular");
	tc_symlink = tcase_create("symlink");
	tc_hardlink = tcase_create("hardlink");
	tc_dir = tcase_create("directory");

	suite_add_tcase(s, tc_regular);
	suite_add_tcase(s, tc_symlink);
	suite_add_tcase(s, tc_hardlink);
	suite_add_tcase(s, tc_dir);

	tcase_add_test(tc_regular, pkgfile_regular_bad_test);
	tcase_add_test(tc_regular, pkgfile_regular_empty_test);
	tcase_add_test(tc_regular, pkgfile_regular_data_test);
	tcase_add_test(tc_regular, pkgfile_regular_existing_test);
	tcase_add_test(tc_regular, pkgfile_regular_depth_test);

	tcase_add_test(tc_symlink, pkgfile_symlink_bad_test);
	tcase_add_test(tc_symlink, pkgfile_symlink_good_test);
	tcase_add_test(tc_symlink, pkgfile_symlink_existing_test);
	tcase_add_test(tc_symlink, pkgfile_symlink_depth_test);

	tcase_add_test(tc_hardlink, pkgfile_hardlink_bad_test);
	tcase_add_test(tc_hardlink, pkgfile_hardlink_test);
	tcase_add_test(tc_hardlink, pkgfile_hardlink_depth_test);
	tcase_add_test(tc_hardlink, pkgfile_hardlink_existing_test);

	tcase_add_test(tc_dir, pkgfile_directory_bad_test);
	tcase_add_test(tc_dir, pkgfile_directory_test);
	tcase_add_test(tc_dir, pkgfile_directory_existing_test);
	tcase_add_test(tc_dir, pkgfile_directory_depth_test);

	return s;
}
