#include "test.h"
#include <stdlib.h>

int
main(int argc, char *argv[])
{
	int fail_count;
	Suite *s;
	SRunner *sr;

	sr = srunner_create(pkgfile_suite());
	srunner_run_all(sr, CK_NORMAL);
	fail_count = srunner_ntests_failed(sr);
	srunner_free(sr);

	return (fail_count == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
