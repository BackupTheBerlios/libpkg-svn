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

DPADD	+= ${.CURDIR}/../../src/libpkg.a
DPADD	+= ${LIBMD} ${LIBARCHIVE} ${LIBBZ2} ${LIBZ}

NOMAN	 = 1
NO_MAN	 = 1

WARNS	?= 6

.for run in libpkg cvs
. for type in mtree stdout stderr
.  for i in 1 2 3 4 5 6 7 8 9
CLEANFILES	+= ${run}.${type}.${i}
.  endfor
. endfor
.endfor

.include <bsd.prog.mk>
