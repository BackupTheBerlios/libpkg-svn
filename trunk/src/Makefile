LIB		 = pkg

SRCS		 = archive_read_open_stream.c

# Package handling
SRCS		+= pkg.c pkg_freebsd.c

# Handle FreeBSD +CONTENTS files
SRCS		+= pkg_freebsd_contents.c

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

CFLAGS		+= -O0
DEBUG_FLAGS	= -ggdb
WARNS		?= 6

.include <bsd.lib.mk>
