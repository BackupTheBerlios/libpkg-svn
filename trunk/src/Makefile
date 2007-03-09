LIB		 = pkg

SRCS		 = archive_read_open_stream.c

# Package handeling
SRCS		+= pkg.c pkg_freebsd.c

# Package Manifest handeling
SRCS		+= pkg_manifest.c pkg_manifest_freebsd.c

# Handle FreeBSD +CONTENTS files
SRCS		+= pkg_freebsd_contents.c pkg_freebsd_parser.c pkg_freebsd_lexer.c

# Package files
SRCS		+= pkgfile.c

# Package Database
SRCS		+= pkg_db.c pkg_db_freebsd.c

# Package Repository
SRCS		+= pkg_repo.c pkg_repo_files.c pkg_repo_ftp.c \
			pkg_repo_local_freebsd.c pkg_db_match.c

# Misc functions
SRCS		+= pkg_util.c

INC		 = pkg.h

pkg_freebsd_lexer.c: pkg_freebsd_lexer.l pkg_freebsd_parser.h
	${LEX} -t ${LFLAGS} -Ppkg_freebsd_ pkg_freebsd_lexer.l > ${.TARGET}

pkg_freebsd_parser.c pkg_freebsd_parser.h: pkg_freebsd_parser.y
	${YACC} -v -d -p pkg_freebsd_ -o pkg_freebsd_parser.c ${.ALLSRC}


CLEANFILES	+= pkg_freebsd_lexer.c pkg_freebsd_lexer.h
CLEANFILES	+= pkg_freebsd_parser.c pkg_freebsd_parser.h

CFLAGS		+= -O0
DEBUG_FLAGS	= -ggdb
WARNS		?= 6

.include <bsd.lib.mk>
