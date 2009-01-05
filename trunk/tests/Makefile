PROG=		tests

SRCS=		main.c pkgfile.c
SRCS+=		pkg_manifest.c pkg_manifest_item.c pkg_manifest_freebsd.c

CFLAGS+=	-I/usr/local/include -I${.CURDIR}/../src
LDADD+=		-L/usr/local/lib -lcheck
LDADD+=		${.OBJDIR}/../src/libpkg.a
LDADD+=		-larchive -lmd

DPADD+=		${.CURDIR}/../src/libpkg.a
DPADD+=		${LIBMD} ${LIBARCHIVE}

MAN=
WARNS=	6

CLEANFILES=	tests.core

test: tests
	cd ${.OBJDIR} && MALLOC_OPTIONS=AJ ./tests

SUBDIR=		fbsd_contents

.include <bsd.prog.mk>
