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

#include <sys/types.h>

#include <dirent.h>
#include <err.h>
#include <pkg.h>
#include <pkg_db.h>
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
		char file[FILENAME_MAX];
		struct pkg_manifest *manifest;
		struct pkgfile *pfile;

		if (de->d_name[0] == '.')
			continue;

		if (de->d_type != DT_DIR)
			continue;

		snprintf(file, FILENAME_MAX, "/var/db/pkg/%s/+CONTENTS",
		    de->d_name);

		pfile = pkgfile_new_from_disk(file, 0);
		if (!pfile)
			continue;

		printf("%s ", de->d_name);
		manifest = pkg_manifest_new_freebsd_pkgfile(pfile);
		if (manifest == NULL) {
			printf("FAILED\n");
		} else {
			printf("Ok\n");
		}

		pkg_manifest_free(manifest);
		pkgfile_free(pfile);
	}

	closedir(d);
	return 0;
}
