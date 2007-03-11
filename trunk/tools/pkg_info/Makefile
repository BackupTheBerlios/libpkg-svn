PROG	 = pkg_info

SRCS	 = main.c match.c show.c

DEBUG_FLAGS?=-ggdb

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

.for run in libpkg cvs
. for type in mtree stdout stderr
.  for i in 1 2 3 4 5 6
CLEANFILES	+= ${run}.${type}.${i}
.  endfor
. endfor
.endfor

.include <bsd.prog.mk>
