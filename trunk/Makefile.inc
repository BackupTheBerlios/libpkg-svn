.if defined(WITH_COVERAGE)
CFLAGS		+= -fprofile-arcs -ftest-coverage
NO_PROFILE	 =
.endif

