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
SRCS+=	pkg_util.c

INC=	pkg.h

MAN=	libpkg.3 pkg.3 pkg_repo.3 pkg_db.3

CFLAGS+=-O0
DEBUG_FLAGS=-ggdb
WARNS?=3

.include <bsd.lib.mk>
