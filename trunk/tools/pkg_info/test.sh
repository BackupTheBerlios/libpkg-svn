#!/bin/sh

cd `dirname $0`
DIR=`pwd`

cd ..
. ${DIR}/../test.sh
cd ${DIR}

TEST_NO=$1
shift

TEST_MAX=12
TOOL_NAME=pkg_info

# Runs the tests for either the libpkg or FreeBSD version of pkg_add
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
