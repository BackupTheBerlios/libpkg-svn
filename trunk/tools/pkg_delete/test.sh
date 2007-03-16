#!/bin/sh

. `dirname $0`/../test.sh

TEST_NO=$1
shift

TEST_MAX=7
TOOL_NAME=pkg_delete

# Runs the tests for either the libpkg or cvs version of pkg_delete
do_tests() {
	RUN=$1
	LIBPKG=$2

	i=1
	while [ $i -le $TEST_MAX ] ; do
		do_test $i $RUN $LIBPKG
		i=$((i+1))
	done
}

CWD=`pwd`
cd `dirname $0`

run_test ${TEST_NO}

cd $CWD
