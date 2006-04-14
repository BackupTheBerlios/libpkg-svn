PROG	 = pkg_add

SRCS	 = main.c

CFLAGS	+= -I${.CURDIR}/../../src
.if defined(WITH_PROFILE)
CFLAGS	+= -ggdb -pg -lc
LDADD	 = ${.CURDIR}/../../src/libpkg_p.a
.else
LDADD	 = ${.CURDIR}/../../src/libpkg.a
.endif
LDADD	+= -lmd -larchive -lbz2 -lz -lfetch
NOMAN	 = 1
NO_MAN	 = 1

WARNS	?= 6

.include <bsd.prog.mk>
