/*
 * Copyright (C) 2006, 2007 Andrew Turner, All rights reserved.
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

#include "test.h"

#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <pkg.h>
#include <pkg_private.h>

#define BASIC_FILE "testdir/BASIC"
#define BASIC_FILE_LENGTH 13
#define LINK_TARGET "testdir/TARGET"
#define LINK_TARGET_LENGTH 14
#define LINK_TARGET_MD5 "d544f30242ff0dab40727ba1acc0751a"
#define DEPTH_DIR "testdir/dir"
#define DEPTH_FILE DEPTH_DIR "/DEPTH"

static void basic_file_tests(struct pkgfile *, const char *, pkgfile_type,
	pkgfile_loc, const char *, unsigned int);
static void test_checksums(struct pkgfile *, const char *);
static void existing_regular_test(struct pkgfile *);
static void existing_symlink_test(struct pkgfile *);
static void existing_directory_test(struct pkgfile *);
static void depth_test_fail_write(struct pkgfile *);
static void empty_regular_file_tests(const char *);
static void check_regular_file_data(const char *, const char *, int, int);
static void check_symlink_data(const char *, const char *);
static void check_directory_data(const char *);

/*
 * Check a pkgfile object is correct after it has been created
 */
static void
basic_file_tests(struct pkgfile *file, const char *filename, pkgfile_type type,
	pkgfile_loc loc, const char *data, unsigned int length)
{
	FILE *fd;
	const char *file_type;

	fail_unless(file->loc == loc, NULL);
	fail_unless(file->type == type, NULL);
	fail_unless(file->fd == NULL, NULL);
	fail_unless(file->mode == 0, NULL);
	fail_unless(file->md5[0] == '\0', NULL);

	fail_unless(strcmp(pkgfile_get_name(file), filename) == 0, NULL);
	fail_unless(strcmp(file->name, filename) == 0, NULL);

	/* Basic bad data test */
	fail_unless(pkgfile_append(file, NULL, 5) == -1, NULL);
	fail_unless(pkgfile_append_string(file, NULL) == -1, NULL);
	fail_unless(pkgfile_remove_line(file, NULL) == -1, NULL);
	fail_unless(pkgfile_set_checksum_md5(file, NULL) == -1, NULL);
	fail_unless(pkgfile_set_checksum_md5(file, "12345") == -1, NULL);
	fail_unless(pkgfile_set_checksum_md5(NULL, "1234567890123456789012") ==
	    -1, NULL);
	fail_unless(pkgfile_set_cwd(file, NULL) == -1, NULL);

	/* Test the file length */
	fail_unless(pkgfile_get_size(file) == length, NULL);
	fail_unless(file->length == length, NULL);

	/* Test the data */
	if (data != NULL) {
		fail_unless(strncmp(pkgfile_get_data(file), data, length) == 0,
		    NULL);
		if (length > 0) {
			fail_unless(pkgfile_get_data(file) != NULL, NULL);

			if (file->type != pkgfile_dir) {
				fail_unless(file->data != NULL, NULL);
				fail_unless(strncmp(file->data, data, length)
				    == 0, NULL);
			} else {
				fail_unless(file->data == NULL, NULL);
				fail_unless(strncmp(file->name, data, length)
				    == 0, NULL);
			}
		} else {
			fail_unless(pkgfile_get_data(file) == NULL, NULL);
			fail_unless(file->data == NULL, NULL);
		}
	}

	fd = pkgfile_get_fileptr(file);
	fail_unless(fd != NULL, NULL);

	fail_unless((file_type = pkgfile_get_type_string(file)) != NULL, NULL);
	switch (type) {
	case pkgfile_none:
		fail_unless(strcmp(file_type, "none") == 0, NULL);
		break;
	case pkgfile_regular:
		fail_unless(strcmp(file_type, "file") == 0, NULL);
		break;
	case pkgfile_hardlink:
		fail_unless(strcmp(file_type, "hardlink") == 0, NULL);
		break;
	case pkgfile_symlink:
		fail_unless(strcmp(file_type, "symlink") == 0, NULL);
		break;
	case pkgfile_dir:
		fail_unless(strcmp(file_type, "directory") == 0, NULL);
		break;
	}

	/* Test setting the file's mode */
	fail_unless(pkgfile_set_mode(file, 100) == 0, NULL);
	fail_unless(file->mode == (100 & ALLPERMS), NULL);
	/* Reset it for later operations */
	fail_unless(pkgfile_set_mode(file, 0) == 0, NULL);

	if (type == pkgfile_regular && length == 0) {
		/* Not all seek tests will work with an empty file */
		fail_unless(pkgfile_seek(file, 0, SEEK_SET) == 0, NULL);
		fail_unless(file->offset == 0, NULL);
		fail_unless(pkgfile_seek(file, 1, SEEK_SET) == 0, NULL);
		fail_unless(file->offset == 0, NULL);
	} else if (type == pkgfile_regular) {
		int pos;
		/* Test to see if seting a valid position works */
		fail_unless(pkgfile_seek(file, 0, SEEK_SET) == 0, NULL);
		fail_unless(file->offset == 0, NULL);
		fail_unless(pkgfile_seek(file, 1, SEEK_SET) == 0, NULL);
		fail_unless(file->offset == 1, NULL);
		/* Test an invalid position is ignored */
		errno = 0;
		fail_unless(pkgfile_seek(file, -1, SEEK_SET) == -1, NULL);
		fail_unless(errno == EINVAL, NULL);
		fail_unless(file->offset == 1, NULL);
		/* Test positive movement is correct */
		fail_unless(pkgfile_seek(file, 1, SEEK_CUR) == 0, NULL);
		fail_unless(file->offset == 2, NULL);
		/* Test a valid negative is correct */
		fail_unless(pkgfile_seek(file, -1, SEEK_CUR) == 0, NULL);
		fail_unless(file->offset == 1, NULL);

		/* Test if a negative large value will move to offset 0 */
		errno = 0;
		fail_unless(pkgfile_seek(file, -10, SEEK_CUR) == -1, NULL);
		fail_unless(errno == EINVAL, NULL);
		fail_unless(file->offset == 0, NULL);

		/* Test seeking relative to the end */
		fail_unless(pkgfile_seek(file, 0, SEEK_END) == 0, NULL);
		fail_unless(file->offset == length, NULL);
		fail_unless(pkgfile_seek(file, -1, SEEK_END) == 0, NULL);
		fail_unless(file->offset == (length - 1), NULL);

		/* Test if a negative large value will move to offset 0 */
		errno = 0;
		pos = length * -2;
		fail_unless(pkgfile_seek(file, pos, SEEK_END) == -1,
		    NULL);
		fail_unless(errno == EINVAL, NULL);
		fail_unless(file->offset == 0, NULL);
	} else {
		fail_unless(pkgfile_seek(file, 0, SEEK_SET) == -1, NULL);
	}
	fail_unless(pkgfile_unlink(file) == -1, NULL);
}

static void
test_checksums(struct pkgfile *file, const char *md5)
{
	/* Check if it fails with no md5 set */
	fail_unless(pkgfile_compare_checksum_md5(file) == -1, NULL);

	fail_unless(strlen(md5) == 32, NULL);
	fail_unless(pkgfile_set_checksum_md5(file, md5) == 0, NULL);
	fail_unless(strcmp(file->md5, md5) == 0, NULL);
	fail_unless(pkgfile_compare_checksum_md5(file) == 0, NULL);

	/* Check this fails with bad data that is too short */
	fail_unless(pkgfile_set_checksum_md5(file, "") == -1, NULL);
	fail_unless(strcmp(file->md5, md5) == 0, NULL);
	fail_unless(pkgfile_compare_checksum_md5(file) == 0, NULL);

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

/* Tests if pkgfile_write fails when BASIC_FILE already exists and is regular*/
static void
existing_regular_test(struct pkgfile *file)
{
	fail_unless(strcmp(file->name, BASIC_FILE) == 0, NULL);

	SETUP_TESTDIR();
	system("touch " LINK_TARGET);
	system("echo -n Hello > " BASIC_FILE);

	/* This should fail as BASIC_FILE already exists */
	fail_unless(pkgfile_write(file) == -1, NULL);
	check_regular_file_data(BASIC_FILE, "Hello", 5, 1);
	system("rm " BASIC_FILE);
	system("rm " LINK_TARGET);
	CLEANUP_TESTDIR();
}

/* Tests if pkgfile_write fails if BASIC_FILE exists and is a symlink */
static void
existing_symlink_test(struct pkgfile *file)
{
	SETUP_TESTDIR();
	system("touch " LINK_TARGET);
	symlink("testdir/Bar", BASIC_FILE);
	fail_unless(pkgfile_write(file) == -1, NULL);
	check_symlink_data(BASIC_FILE, "testdir/Bar");
	system("rm " BASIC_FILE);
	system("rm " LINK_TARGET);
	CLEANUP_TESTDIR();
}

/* Tests if pkgfile_write fails if BASIC_FILE exiasts and is a directory */
static void
existing_directory_test(struct pkgfile *file)
{
	SETUP_TESTDIR();
	system("touch " LINK_TARGET);
	system("mkdir " BASIC_FILE);
	fail_unless(pkgfile_write(file) == -1, NULL);
	system("rmdir " BASIC_FILE);
	system("rm " LINK_TARGET);
	CLEANUP_TESTDIR();
}

static void
depth_test_fail_write(struct pkgfile *file)
{
	fail_unless(strcmp(file->name, DEPTH_FILE) == 0, NULL);
	SETUP_TESTDIR();
	/*
	 * Make sure the hardlink is failing because DEPTH_DIR
	 * exists not because it can't find LINK_TARGET
	 */
	system("touch " LINK_TARGET);
	system("touch " DEPTH_DIR);
	fail_unless(pkgfile_write(file) == -1, NULL);
	system("rm " DEPTH_DIR);
	system("rm " LINK_TARGET);
	CLEANUP_TESTDIR();
}

static void
empty_regular_file_tests(const char *buf)
{
	struct pkgfile *file;
	FILE *fd;
	struct stat sb;

	fail_unless((file = pkgfile_new_regular(BASIC_FILE, buf, 0))
	    != NULL, NULL);
	basic_file_tests(file, BASIC_FILE, pkgfile_regular, pkgfile_loc_mem,
	    NULL, 0);

	fail_unless(pkgfile_get_data(file) == NULL, NULL);

	/* The md5 of an empty string is d41d8cd98f00b204e9800998ecf8427e */
	test_checksums(file, "d41d8cd98f00b204e9800998ecf8427e");

	SETUP_TESTDIR();
	fail_unless(pkgfile_write(file) == 0, NULL);
	fail_unless((fd = fopen(BASIC_FILE, "r")) != NULL, NULL);
	fstat(fileno(fd), &sb);
	fail_unless(sb.st_size == 0, NULL);
	/* XXX Check the file contents are correct */
	fclose(fd);
	system("rm " BASIC_FILE);
	CLEANUP_TESTDIR();

	fail_unless(pkgfile_free(file) == 0, NULL);
}

static void
check_regular_file_data(const char *filename, const char *expected_data,
    int length, int link_count)
{
	struct stat sb;
	FILE *fd;
	char *buf;

	fail_unless((fd = fopen(filename, "r")) != NULL, NULL);

	/* Check the file looks correct */
	fstat(fileno(fd), &sb);
	fail_unless(S_ISREG(sb.st_mode), NULL);
	fail_unless(sb.st_size == length, NULL);
	fail_unless(sb.st_nlink == link_count, NULL);

	fail_unless((buf = calloc(length + 1, 1)) != NULL, NULL);
	fread(buf, length, 1, fd);
	/* Check the file has been written correctly */
	fail_unless(strcmp(buf, expected_data) == 0, NULL);
	free(buf);

	fclose(fd);
}

static void
check_symlink_data(const char *filename, const char *expected_data)
{
	struct stat sb;
	char buf[MAXPATHLEN];
	int len;

	/* XXX Check the file contents are correct */
	fail_unless(lstat(filename, &sb) == 0, NULL);
	fail_unless(S_ISLNK(sb.st_mode), NULL);
	fail_unless(sb.st_size == strlen(expected_data), NULL);

	fail_unless((len = readlink(filename, buf, MAXPATHLEN)) != -1, NULL);
	fail_unless(len <= MAXPATHLEN, NULL);
	buf[len] = '\0';
	fail_unless(strcmp(buf, expected_data) == 0, NULL);
}

static void
check_directory_data(const char *directory)
{
	struct stat sb;

	fail_unless(stat(directory, &sb) == 0, NULL);
	fail_unless(S_ISDIR(sb.st_mode), NULL);
}

/* Tests on creating a regular file from a buffer */
START_TEST(pkgfile_regular_bad_test)
{
	/* Test creating a regular file from bad data fails */
	fail_unless(pkgfile_new_regular(NULL, NULL, 0) == NULL, NULL);
	fail_unless(pkgfile_new_regular(NULL, "", 0) == NULL, NULL);
	fail_unless(pkgfile_new_regular(BASIC_FILE, NULL, 1) == NULL, NULL);
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

	/* Create a file with data */
	fail_unless((file = pkgfile_new_regular(BASIC_FILE, "0123456789", 10))
	    != NULL, NULL);
	basic_file_tests(file, BASIC_FILE, pkgfile_regular, pkgfile_loc_mem,
	    "0123456789", 10);

	/* The md5 of 0123456789 string is 781e5e245d69b566979b86e28d23f2c7 */
	test_checksums(file, "781e5e245d69b566979b86e28d23f2c7");

	SETUP_TESTDIR();
	fail_unless(pkgfile_write(file) == 0, NULL);
	/* Attempting to over write a file should fail */
	fail_unless(pkgfile_write(file) == -1, NULL);
	check_regular_file_data(BASIC_FILE, "0123456789", 10, 1);
	system("rm " BASIC_FILE);
	CLEANUP_TESTDIR();

	fail_unless(pkgfile_free(file) == 0, NULL);
}
END_TEST

START_TEST(pkgfile_regular_existing_regular_test)
{
	struct pkgfile *file;

	/* Test if pkgfile_write will fail with a regular file */
	file = pkgfile_new_regular(BASIC_FILE, "0123456789", 10);
	fail_unless(strcmp(file->name, BASIC_FILE) == 0, NULL);
	existing_regular_test(file);
	pkgfile_free(file);
}
END_TEST

START_TEST(pkgfile_regular_existing_symlink_test)
{
	struct pkgfile *file;

	/* Test if pkgfile_write will fail with a symlink */
	file = pkgfile_new_regular(BASIC_FILE, "0123456789", 10);
	fail_unless(strcmp(file->name, BASIC_FILE) == 0, NULL);
	existing_symlink_test(file);
	pkgfile_free(file);
}
END_TEST

START_TEST(pkgfile_regular_existing_directory_test)
{
	struct pkgfile *file;

	/* Test if pkgfile_write will fail with a directory */
	file = pkgfile_new_regular(BASIC_FILE, "0123456789", 10);
	fail_unless(strcmp(file->name, BASIC_FILE) == 0, NULL);
	existing_directory_test(file);
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

	file = pkgfile_new_regular(DEPTH_FILE, "0123456789", 10);
	fail_unless(strcmp(file->name, DEPTH_FILE) == 0, NULL);
	SETUP_TESTDIR();
	fail_unless(pkgfile_write(file) == 0, NULL);
	check_regular_file_data(DEPTH_FILE, "0123456789", 10, 1);
	system("rm " DEPTH_FILE);
	system("rmdir " DEPTH_DIR);
	CLEANUP_TESTDIR();
	pkgfile_free(file);
}
END_TEST

START_TEST(pkgfile_regular_depth_exists_test)
{
	struct pkgfile *file;

	/* Test pkg_write will fail when it can't create a parent directory */
	file = pkgfile_new_regular(DEPTH_FILE, "0123456789", 10);
	fail_unless(strcmp(file->name, DEPTH_FILE) == 0, NULL);
	depth_test_fail_write(file);
	pkgfile_free(file);
}
END_TEST

START_TEST(pkgfile_regular_modify_test)
{
	struct pkgfile *file;
	char data[12];

	sprintf(data, "12345\n");
	file = pkgfile_new_regular(DEPTH_FILE, data, 6);
	basic_file_tests(file, DEPTH_FILE, pkgfile_regular, pkgfile_loc_mem,
	    data, 6);
	fail_unless(pkgfile_append(file, "67890", 5) == 0, NULL);
	sprintf(data, "12345\n67890");
	basic_file_tests(file, DEPTH_FILE, pkgfile_regular, pkgfile_loc_mem,
	    data, 11);
	pkgfile_free(file);

	/* Remove the first line in a file */
	sprintf(data, "12345\n67890");
	file = pkgfile_new_regular(DEPTH_FILE, data, 11);
	fail_unless(pkgfile_remove_line(file, "12345") == 0, NULL);
	sprintf(data, "67890");
	basic_file_tests(file, DEPTH_FILE, pkgfile_regular, pkgfile_loc_mem,
	    data, 5);
	pkgfile_free(file);

	/* Remove a middle line from a file */
	sprintf(data, "12345\n67\n89");
	file = pkgfile_new_regular(DEPTH_FILE, data, 11);
	fail_unless(pkgfile_remove_line(file, "67") == 0, NULL);
	sprintf(data, "12345\n89");
	basic_file_tests(file, DEPTH_FILE, pkgfile_regular, pkgfile_loc_mem,
	    data, 8);
	pkgfile_free(file);

	/* Remove the Last line in a file */
	sprintf(data, "12345\n67890");
	file = pkgfile_new_regular(DEPTH_FILE, data, 11);
	fail_unless(pkgfile_remove_line(file, "12345") == 0, NULL);
	sprintf(data, "67890");
	basic_file_tests(file, DEPTH_FILE, pkgfile_regular, pkgfile_loc_mem,
	    data, 5);
	pkgfile_free(file);
}
END_TEST

START_TEST(pkgfile_regular_modify_empty_test)
{
	/* Test if an empty file can be modified */
	struct pkgfile *file;
	char data[6];

	file = pkgfile_new_regular(DEPTH_FILE, "", 0);
	basic_file_tests(file, DEPTH_FILE, pkgfile_regular, pkgfile_loc_mem,
	    "", 0);
	fail_unless(pkgfile_append(file, "67890", 5) == 0, NULL);
	sprintf(data, "67890");
	basic_file_tests(file, DEPTH_FILE, pkgfile_regular, pkgfile_loc_mem,
	    data, 5);
	pkgfile_free(file);
}
END_TEST

/* Tests on creating a symlink from a buffer */
START_TEST(pkgfile_symlink_bad_test)
{
	/* Test creating a symlink from bad data fails */
	fail_unless(pkgfile_new_symlink(NULL, NULL) == NULL, NULL);
	fail_unless(pkgfile_new_symlink(BASIC_FILE, NULL) == NULL, NULL);
	fail_unless(pkgfile_new_symlink(NULL, LINK_TARGET) == NULL, NULL);
}
END_TEST

START_TEST(pkgfile_symlink_good_test)
{
	struct pkgfile *file;

	fail_unless((file = pkgfile_new_symlink(BASIC_FILE, LINK_TARGET))
	    != NULL, NULL);
	basic_file_tests(file, BASIC_FILE, pkgfile_symlink, pkgfile_loc_mem,
	    LINK_TARGET, LINK_TARGET_LENGTH);
	test_checksums(file, LINK_TARGET_MD5);

	SETUP_TESTDIR();
	fail_unless(pkgfile_write(file) == 0, NULL);
	check_symlink_data(BASIC_FILE, LINK_TARGET);
	system("rm " BASIC_FILE);
	CLEANUP_TESTDIR();

	fail_unless(pkgfile_append(file, "1234567890", 10) == -1, NULL);
	fail_unless(pkgfile_remove_line(file, "1234567890") == -1, NULL);
	fail_unless(pkgfile_seek(file, 0, SEEK_SET) == -1, NULL);
	fail_unless(pkgfile_unlink(file) == -1, NULL);

	fail_unless(pkgfile_free(file) == 0, NULL);
}
END_TEST

START_TEST(pkgfile_symlink_existing_regular_test)
{
	struct pkgfile *file;

	/* Test if pkgfile_write will fail with a regular file */
	file = pkgfile_new_symlink(BASIC_FILE, LINK_TARGET);
	fail_unless(strcmp(file->name, BASIC_FILE) == 0, NULL);
	existing_regular_test(file);
	pkgfile_free(file);
}
END_TEST

START_TEST(pkgfile_symlink_existing_symlink_test)
{
	struct pkgfile *file;

	/* Test if pkgfile_write will fail with a symlink */
	file = pkgfile_new_symlink(BASIC_FILE, LINK_TARGET);
	fail_unless(strcmp(file->name, BASIC_FILE) == 0, NULL);
	existing_symlink_test(file);
	pkgfile_free(file);
}
END_TEST

START_TEST(pkgfile_symlink_existing_directory_test)
{
	struct pkgfile *file;

	/* Test if pkgfile_write will fail with a directory */
	file = pkgfile_new_symlink(BASIC_FILE, LINK_TARGET);
	fail_unless(strcmp(file->name, BASIC_FILE) == 0, NULL);
	existing_directory_test(file);
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

	file = pkgfile_new_symlink(DEPTH_FILE, LINK_TARGET);
	fail_unless(strcmp(file->name, DEPTH_FILE) == 0, NULL);
	SETUP_TESTDIR();
	fail_unless(pkgfile_write(file) == 0, NULL);
	check_symlink_data(DEPTH_FILE, LINK_TARGET);
	system("rm " DEPTH_FILE);
	system("rmdir " DEPTH_DIR);
	CLEANUP_TESTDIR();
	pkgfile_free(file);
}
END_TEST

START_TEST(pkgfile_symlink_depth_exists_test)
{
	struct pkgfile *file;

	/*
	 * Check pkgfile_write fails when there
	 * is already a file named testdir/foo
	 */
	file = pkgfile_new_symlink(DEPTH_FILE, LINK_TARGET);
	fail_unless(strcmp(file->name, DEPTH_FILE) == 0, NULL);
	depth_test_fail_write(file);
	pkgfile_free(file);
}
END_TEST

/* Tests on creating a hardlink from a buffer */
START_TEST(pkgfile_hardlink_bad_test)
{
	/* Test creating a hard link from bad data fails */
	fail_unless(pkgfile_new_hardlink(NULL, NULL) == NULL, NULL);
	fail_unless(pkgfile_new_hardlink(BASIC_FILE, NULL) == NULL, NULL);
	fail_unless(pkgfile_new_hardlink(NULL, LINK_TARGET) == NULL, NULL);
}
END_TEST

START_TEST(pkgfile_hardlink_test)
{
	struct pkgfile *file;

	fail_unless((file = pkgfile_new_hardlink(BASIC_FILE, LINK_TARGET))
	    != NULL, NULL);
	basic_file_tests(file, BASIC_FILE, pkgfile_hardlink, pkgfile_loc_mem,
	    LINK_TARGET, LINK_TARGET_LENGTH);

	SETUP_TESTDIR();
	system("echo -n 0123456789 > " LINK_TARGET);
	/*
	 * pkgfile_compare_checksum_md5 will compare
	 * against the file pointed to by the hardlink
	 */
	test_checksums(file, "781e5e245d69b566979b86e28d23f2c7");

	/* Test the file is correct before writing to it */
	check_regular_file_data(LINK_TARGET, "0123456789", 10, 1);

	/* Write to the file then test both link points */
	fail_unless(pkgfile_write(file) == 0, NULL);
	check_regular_file_data(BASIC_FILE, "0123456789", 10, 2);
	check_regular_file_data(LINK_TARGET, "0123456789", 10, 2);
	system("rm " BASIC_FILE);
	system("rm " LINK_TARGET);
	CLEANUP_TESTDIR();

	fail_unless(pkgfile_free(file) == 0, NULL);
}
END_TEST

START_TEST(pkgfile_hardlink_existing_regular_test)
{
	struct pkgfile *file;

	/* Test if pkgfile_write will fail with a regular file */
	file = pkgfile_new_hardlink(BASIC_FILE, LINK_TARGET);
	fail_unless(strcmp(file->name, BASIC_FILE) == 0, NULL);
	existing_regular_test(file);
	pkgfile_free(file);
}
END_TEST

START_TEST(pkgfile_hardlink_existing_symlink_test)
{
	struct pkgfile *file;

	/* Test if pkgfile_write will fail with a symlink */
	file = pkgfile_new_hardlink(BASIC_FILE, LINK_TARGET);
	fail_unless(strcmp(file->name, BASIC_FILE) == 0, NULL);
	existing_symlink_test(file);
	pkgfile_free(file);
}
END_TEST

START_TEST(pkgfile_hardlink_existing_directory_test)
{
	struct pkgfile *file;

	/* Test if pkgfile_write will fail with a directory */
	file = pkgfile_new_hardlink(BASIC_FILE, LINK_TARGET);
	fail_unless(strcmp(file->name, BASIC_FILE) == 0, NULL);
	existing_directory_test(file);
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

	file = pkgfile_new_hardlink(DEPTH_FILE, LINK_TARGET);
	fail_unless(strcmp(file->name, DEPTH_FILE) == 0, NULL);
	SETUP_TESTDIR();
	system("echo -n 0123456789 > " LINK_TARGET);
	fail_unless(pkgfile_write(file) == 0, NULL);
	check_regular_file_data(LINK_TARGET, "0123456789", 10, 2);
	system("rm " DEPTH_FILE);
	system("rmdir " DEPTH_DIR);
	system("rm " LINK_TARGET);
	CLEANUP_TESTDIR();
	pkgfile_free(file);
}
END_TEST

START_TEST(pkgfile_hardlink_depth_exists_test)
{
	struct pkgfile *file;

	/*
	 * Check pkgfile_write fails when there
	 * is already a file named testdir/foo
	 */
	file = pkgfile_new_hardlink(DEPTH_FILE, LINK_TARGET);
	fail_unless(strcmp(file->name, DEPTH_FILE) == 0, NULL);
	depth_test_fail_write(file);
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

	fail_unless((file = pkgfile_new_directory(BASIC_FILE)) != NULL, NULL);
	basic_file_tests(file, BASIC_FILE, pkgfile_dir, pkgfile_loc_mem,
	    BASIC_FILE, BASIC_FILE_LENGTH);

	/* Test the file length */
	fail_unless(pkgfile_get_size(file) == BASIC_FILE_LENGTH, NULL);
	fail_unless(file->length == BASIC_FILE_LENGTH, NULL);

	SETUP_TESTDIR();
	fail_unless(pkgfile_write(file) == 0, NULL);
	/*
	 * A directory should only fail if it is being written
	 * with different permissions than the existing one
	 */
	fail_unless(pkgfile_write(file) == 0, NULL);
	check_directory_data(BASIC_FILE);
	system("rmdir " BASIC_FILE);
	CLEANUP_TESTDIR();
	fail_unless(pkgfile_free(file) == 0, NULL);
}
END_TEST

START_TEST(pkgfile_directory_existing_regular_test)
{
	struct pkgfile *file;

	/* Test if pkgfile_write should fail with a regular file */
	file = pkgfile_new_directory(BASIC_FILE);
	fail_unless(strcmp(file->name, BASIC_FILE) == 0, NULL);
	existing_regular_test(file);
	pkgfile_free(file);
}
END_TEST

START_TEST(pkgfile_directory_existing_symlink_test)
{
	struct pkgfile *file;

	/* Test if pkgfile_write should fail with a symlink */
	file = pkgfile_new_directory(BASIC_FILE);
	fail_unless(strcmp(file->name, BASIC_FILE) == 0, NULL);
	existing_symlink_test(file);
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

	file = pkgfile_new_directory(DEPTH_FILE);
	fail_unless(strcmp(file->name, DEPTH_FILE) == 0, NULL);
	SETUP_TESTDIR();
	fail_unless(pkgfile_write(file) == 0, NULL);
	check_directory_data(DEPTH_FILE);
	system("rmdir " DEPTH_FILE);
	system("rmdir " DEPTH_DIR);
	CLEANUP_TESTDIR();
	pkgfile_free(file);
}
END_TEST

START_TEST(pkgfile_directory_depth_exists_test)
{
	struct pkgfile *file;

	/*
	 * Check pkgfile_write fails when there
	 * is already a file named testdir/foo
	 */
	file = pkgfile_new_directory(DEPTH_FILE);
	fail_unless(strcmp(file->name, DEPTH_FILE) == 0, NULL);
	depth_test_fail_write(file);
	pkgfile_free(file);
}
END_TEST

START_TEST(pkgfile_misc_bad_args)
{
	fail_unless(pkgfile_append(NULL, NULL, 0) == -1, NULL);
	fail_unless(pkgfile_append(NULL, NULL, 1) == -1, NULL);
	fail_unless(pkgfile_append(NULL, "1234567890", 10) == -1, NULL);
	fail_unless(pkgfile_append_string(NULL, NULL) == -1, NULL);
	fail_unless(pkgfile_append_string(NULL, "") == -1, NULL);
	fail_unless(pkgfile_append_string(NULL, "%s", "string") == -1, NULL);
	fail_unless(pkgfile_compare_checksum_md5(NULL) == -1, NULL);
	fail_unless(pkgfile_free(NULL) == -1, NULL);
	fail_unless(pkgfile_get_data(NULL) == NULL, NULL);
	fail_unless(pkgfile_get_fileptr(NULL) == NULL, NULL);
	fail_unless(pkgfile_get_name(NULL) == NULL, NULL);
	fail_unless(pkgfile_get_size(NULL) == 0, NULL);
	fail_unless(pkgfile_get_type_string(NULL) == NULL, NULL);
	fail_unless(pkgfile_remove_line(NULL, NULL) == -1, NULL);
	fail_unless(pkgfile_remove_line(NULL, "") == -1, NULL);
	fail_unless(pkgfile_seek(NULL, 0, SEEK_SET) == -1, NULL);
	fail_unless(pkgfile_set_checksum_md5(NULL, NULL) == -1, NULL);
	fail_unless(pkgfile_set_checksum_md5(NULL, "1234567890123456789012") ==
	    -1, NULL);
	fail_unless(pkgfile_set_cwd(NULL, NULL) == -1, NULL);
	fail_unless(pkgfile_set_cwd(NULL, "/a/dir") == -1, NULL);
	fail_unless(pkgfile_set_mode(NULL, 1) == -1, NULL);
	fail_unless(pkgfile_unlink(NULL) == -1, NULL);
	fail_unless(pkgfile_write(NULL) == -1, NULL);
}
END_TEST

Suite *
pkgfile_suite()
{
	Suite *s;
	TCase *tc;

	s = suite_create("pkgfile");

	tc = tcase_create("regular");
	tcase_add_test(tc, pkgfile_regular_bad_test);
	tcase_add_test(tc, pkgfile_regular_empty_test);
	tcase_add_test(tc, pkgfile_regular_data_test);

	tcase_add_test(tc, pkgfile_regular_existing_regular_test);
	tcase_add_test(tc, pkgfile_regular_existing_symlink_test);
	tcase_add_test(tc, pkgfile_regular_existing_directory_test);

	tcase_add_test(tc, pkgfile_regular_depth_test);
	tcase_add_test(tc, pkgfile_regular_depth_exists_test);

	tcase_add_test(tc, pkgfile_regular_modify_test);
	tcase_add_test(tc, pkgfile_regular_modify_empty_test);
	suite_add_tcase(s, tc);


	tc = tcase_create("symlink");
	tcase_add_test(tc, pkgfile_symlink_bad_test);
	tcase_add_test(tc, pkgfile_symlink_good_test);

	tcase_add_test(tc, pkgfile_symlink_existing_regular_test);
	tcase_add_test(tc, pkgfile_symlink_existing_symlink_test);
	tcase_add_test(tc, pkgfile_symlink_existing_directory_test);

	tcase_add_test(tc, pkgfile_symlink_depth_test);
	tcase_add_test(tc, pkgfile_symlink_depth_exists_test);
	suite_add_tcase(s, tc);


	tc = tcase_create("hardlink");
	tcase_add_test(tc, pkgfile_hardlink_bad_test);
	tcase_add_test(tc, pkgfile_hardlink_test);

	tcase_add_test(tc, pkgfile_hardlink_existing_regular_test);
	tcase_add_test(tc, pkgfile_hardlink_existing_symlink_test);
	tcase_add_test(tc, pkgfile_hardlink_existing_directory_test);

	tcase_add_test(tc, pkgfile_hardlink_depth_test);
	tcase_add_test(tc, pkgfile_hardlink_depth_exists_test);
	suite_add_tcase(s, tc);


	tc = tcase_create("directory");
	tcase_add_test(tc, pkgfile_directory_bad_test);
	tcase_add_test(tc, pkgfile_directory_test);

	tcase_add_test(tc, pkgfile_directory_existing_regular_test);
	tcase_add_test(tc, pkgfile_directory_existing_symlink_test);

	tcase_add_test(tc, pkgfile_directory_depth_test);
	tcase_add_test(tc, pkgfile_directory_depth_exists_test);
	suite_add_tcase(s, tc);


	tc = tcase_create("misc");
	tcase_add_test(tc, pkgfile_misc_bad_args);
	suite_add_tcase(s, tc);

	return s;
}
