PROG	 = pkg_info

SRCS	 = main.c match.c show.c

CFLAGS	+= -I${.CURDIR}/../../src
.if defined(WITH_PROFILE)
CFLAGS	+= -pg
LDADD	 = ${.CURDIR}/../../src/libpkg_p.a
.else
LDADD	 = ${.CURDIR}/../../src/libpkg.a
.endif
LDADD	+= -lmd -larchive -lbz2 -lz
NOMAN	 = 1
NO_MAN	 = 1

WARNS	?= 6

.include <bsd.prog.mk>
