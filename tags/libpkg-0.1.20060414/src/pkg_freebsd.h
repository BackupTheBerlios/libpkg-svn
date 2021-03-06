/*
 * Copyright (C) 2005, Andrew Turner, All rights reserved.
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

#ifndef __LIBPKG_PKG_FREEBSD_H__
#define __LIBPKG_PKG_FREEBSD_H__

struct pkg_freebsd_contents;

enum {
	PKG_LINE_UNKNOWN = 0,
	PKG_LINE_COMMENT = 1,
	PKG_LINE_NAME = 2,
	PKG_LINE_CWD = 3,
	PKG_LINE_PKGDEP = 4,
	PKG_LINE_CONFLICTS = 5,
	PKG_LINE_EXEC = 6,
	PKG_LINE_UNEXEC = 7,
	PKG_LINE_IGNORE = 8,
	PKG_LINE_DIRRM = 9,
	PKG_LINE_MTREE = 10,
	PKG_LINE_FILE = 11
};

struct pkg_freebsd_contents_line {
	int	 line_type;
	char	*line;
	char	*data;
};

extern const char *pkg_freebsd_contents_line_str[];

struct pkg_freebsd_contents *pkg_freebsd_get_contents(struct pkg *);

struct pkg_freebsd_contents *pkg_freebsd_contents_new(const char *);
int	 pkg_freebsd_contents_add_line(struct pkg_freebsd_contents *, int,
	    const char *);
int	 pkg_freebsd_contents_add_dependency(struct pkg_freebsd_contents *,
	    struct pkg *);
int	 pkg_freebsd_contents_add_file(struct pkg_freebsd_contents *,
	    struct pkg_file *);
struct pkg_freebsd_contents_line *pkg_freebsd_contents_get_line
					(struct pkg_freebsd_contents *,
					 unsigned int);
int	 pkg_freebsd_contents_free(struct pkg_freebsd_contents *);

#endif /* __LIBPKG_PKG_FREEBSD_H__ */
