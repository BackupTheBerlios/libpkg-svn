PROG=demo

CFLAGS+=-I${.CURDIR}/../src
LDADD= ${.CURDIR}/../src/libpkg.a
LDADD+=	-larchive -lbz2 -lz
LDADD+=	-lfetch
LDADD+= -lmd

NO_MAN=1
NOMAN=1

.include <bsd.prog.mk>
