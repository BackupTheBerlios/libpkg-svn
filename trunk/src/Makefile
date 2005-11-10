LIB=	pkg

SRCS=	archive_read_open_stream.c

# Package handling
SRCS+=	pkg.c pkg_freebsd.c

# Handle FreeBSD +CONTENTS files
SRCS+=	pkg_freebsd_contents.c

# Package files
SRCS+=	pkg_file.c

# Package Database
SRCS+=	pkg_db.c pkg_db_freebsd.c

# Package Repository
SRCS+=	pkg_repo.c pkg_repo_files.c pkg_repo_ftp.c

# Misc functions
SRCS+=	pkg_error.c pkg_util.c pkg_list.c

INC=	pkg.h

MAN=	libpkg.3

CFLAGS+=-O0
DEBUG_FLAGS=-ggdb
WARNS?=6
NOPROFILE=1

.include <bsd.lib.mk>
