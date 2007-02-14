PROG=		tests

SRCS=		main.c pkgfile.c pkg_freebsd_contents.c
CFLAGS+=	-I/usr/local/include -I${.CURDIR}/../src
LDADD+=		-L/usr/local/lib -lcheck
LDADD+=		${.OBJDIR}/../src/libpkg.a /usr/lib/libmd.a

MAN=
WARNS=	6

test: all
	cd ${.OBJDIR} && MALLOC_OPTIONS=AJ ./tests

.include <bsd.prog.mk>
