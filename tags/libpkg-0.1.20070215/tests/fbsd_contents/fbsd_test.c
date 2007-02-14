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

#include <sys/types.h>

#include <dirent.h>
#include <err.h>
#include <pkg.h>
#include <pkg_db.h>
#include <pkg_freebsd.h>
#include <stdlib.h>

int
main(int argc __unused, char *argv[] __unused)
{
	DIR *d;
	struct dirent *de;

	d = opendir("/var/db/pkg");
	if (d == NULL) {
		errx(1, "Could not open /var/db/pkg");
	}

	while((de = readdir(d)) != NULL) {
		char *buf;
		struct pkg_freebsd_contents *contents;
		char file[FILENAME_MAX];
		struct stat sb;
		FILE *fd;

		if (de->d_name[0] == '.')
			continue;

		if (de->d_type != DT_DIR)
			continue;

		printf("%s ", de->d_name);
		snprintf(file, FILENAME_MAX, "/var/db/pkg/%s/+CONTENTS",
		    de->d_name);

		stat(file, &sb);
		buf = malloc(sb.st_size);

		fd = fopen(file, "r");
		fread(buf, sb.st_size, 1, fd);
		fclose(fd);

		contents = pkg_freebsd_contents_new(buf, sb.st_size);
		if (contents == NULL) {
			printf("FAILED\n");
		} else {
			printf("Ok\n");
		}
		pkg_freebsd_contents_free(contents);
		free(buf);
	}

	closedir(d);
	return 0;
}
