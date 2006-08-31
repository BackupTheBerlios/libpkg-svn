#include <check.h>
int setup_testdir(void);
int cleanup_testdir(void);

#define SETUP_TESTDIR() fail_unless(setup_testdir() == 0, "Couldn't create the test dir")
#define CLEANUP_TESTDIR() fail_unless(cleanup_testdir() == 0, "Couldn't cleanup the test dir")

Suite *pkgfile_suite(void);

