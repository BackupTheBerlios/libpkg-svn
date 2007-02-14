/*
 * Copyright (C) 2007, Andrew Turner, All rights reserved.
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

#include <pkg.h>
#include <pkg_freebsd.h>

#include <string.h>

START_TEST(pkg_freebsd_contents_empty_test)
{
	struct pkg_freebsd_contents *contents;

	contents = pkg_freebsd_contents_new("", 0);
	fail_unless(contents == NULL, NULL);
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
	pkg_freebsd_contents_free(contents);
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
	tcase_add_test(tc, pkg_freebsd_contents_good_basic_test);
	suite_add_tcase(s, tc);

	return s;
}


