#include "test.h"
#include <stdlib.h>

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

int
main(int argc __unused, char *argv[] __unused)
{
	int fail_count;
	SRunner *sr;

	sr = srunner_create(pkgfile_suite());
	srunner_run_all(sr, CK_NORMAL);
	fail_count = srunner_ntests_failed(sr);
	srunner_free(sr);

	return (fail_count == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
