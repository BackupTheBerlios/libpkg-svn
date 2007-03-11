#!/bin/sh

cd `dirname $0`
DIR=`pwd`

cd ..
. ${DIR}/../test.sh
cd ${DIR}

TEST_NO=$1
shift

TEST_MAX=6
TOOL_NAME=pkg_info

# Runs the tests for either the libpkg or cvs version of pkg_add
do_tests() {
	RUN=$1
	LIBPKG=$2

	i=1
	while [ $i -le $TEST_MAX ] ; do
		do_test $i $RUN $LIBPKG
		i=$((i+1))
	done
}

# Runs the test
run_test() {

	build_tool

	if [ "X${TEST_NO}" != "X" ] ; then
		do_test $TEST_NO cvs
		do_test $TEST_NO libpkg true
	else
		# Get the reference data from the FreeBSD cvs pkg_add
		do_tests cvs
		# Get the test data from out pkg_add
		do_tests libpkg true
	fi
}

CWD=`pwd`
cd `dirname $0`

run_test

cd $CWD
