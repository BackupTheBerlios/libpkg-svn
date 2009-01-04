/*
 * Copyright (C) 2006, 2007 Andrew Turner, All rights reserved.
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
	srunner_add_suite(sr, pkg_manifest_item_suite());
	srunner_add_suite(sr, pkg_manifest_suite());
	srunner_add_suite(sr, pkg_manifest_freebsd_suite());

	srunner_run_all(sr, CK_NORMAL);
	fail_count = srunner_ntests_failed(sr);
	srunner_free(sr);

	return (fail_count == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
