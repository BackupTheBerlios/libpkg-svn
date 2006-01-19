PROG=	pkg_info

SRCS=	main.c match.c show.c

CFLAGS+=-I${.CURDIR}/../../src
LDADD=	${.CURDIR}/../../src/libpkg.a -lmd -larchive -lbz2 -lz
NOMAN=	1

WARNS?=	6

.include <bsd.prog.mk>
