PROG=		test

SRCS=		main.c pkgfile.c
CFLAGS+=	-I/usr/local/include -I${.CURDIR}/../src
LDADD+=		-L/usr/local/lib -lcheck
LDADD+=		${.OBJDIR}/../src/libpkg.a /usr/lib/libmd.a

MAN=
WARNS=	6

run_test: all
	cd ${.OBJDIR} && ./test

.include <bsd.prog.mk>
