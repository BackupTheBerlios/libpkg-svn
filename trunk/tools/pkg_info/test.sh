#!/bin/sh

cd `dirname $0`
DIR=`pwd`

cd ..
. ${DIR}/../test.sh
cd ${DIR}

TEST_NO=$1
shift

TEST_MAX=10
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
		for file in stdout stderr mtree; do
			diff cvs.${file}.${TEST_NO} libpkg.${file}.${TEST_NO} > /dev/null 2>&1
			if [ $? -eq 0 ]; then
				rm cvs.${file}.${TEST_NO} libpkg.${file}.${TEST_NO}
			fi
		done
	else
		# Get the reference data from the FreeBSD cvs pkg_info
		do_tests cvs
		# Get the test data from out pkg_info
		do_tests libpkg true
		i=1
		while [ $i -le $TEST_MAX ] ; do
			for file in stdout stderr mtree; do
				diff cvs.${file}.${i} libpkg.${file}.${i} > /dev/null 2>&1
				if [ $? -eq 0 ]; then
					rm cvs.${file}.${i} libpkg.${file}.${i}
				fi
			done
			i=$((i+1))
		done
	fi
}

CWD=`pwd`
cd `dirname $0`

run_test

cd $CWD
