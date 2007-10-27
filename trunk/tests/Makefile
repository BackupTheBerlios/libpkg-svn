PROG=		tests

SRCS=		main.c pkgfile.c pkg_freebsd_contents.c pkg_manifest_item.c
SRCS+=		pkg_manifest.c
CFLAGS+=	-I/usr/local/include -I${.CURDIR}/../src
LDADD+=		-L/usr/local/lib -lcheck
LDADD+=		${.OBJDIR}/../src/libpkg.a
LDADD+=		-larchive -lmd

MAN=
WARNS=	6

CLEANFILES=	tests.core

test: tests
	cd ${.OBJDIR} && MALLOC_OPTIONS=AJ ./tests

SUBDIR=		fbsd_contents

.include <bsd.prog.mk>
