PROG	 = pkg_add

SRCS	 = main.c

CFLAGS	+= -I${.CURDIR}/../../src
.if defined(WITH_PROFILE)
CFLAGS	+= -ggdb -pg -lc
LDADD	 = ${.CURDIR}/../../src/libpkg_p.a
LDADD	+= /usr/lib/libmd_p.a /usr/lib/libarchive_p.a /usr/lib/libbz2_p.a
LDADD	+= /usr/lib/libz_p.a /usr/lib/libfetch_p.a /usr/lib/libssl_p.a
LDADD	+= /usr/lib/libcrypto_p.a
.else
LDADD	 = ${.CURDIR}/../../src/libpkg.a 
LDADD	+= -lmd -larchive -lbz2 -lz -lfetch
.endif
NOMAN	 = 1
NO_MAN	 = 1

WARNS	?= 6

.include <bsd.prog.mk>
