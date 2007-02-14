#!/bin/sh

. `dirname $0`/../test.sh

TEST_NO=$1
shift

TEST_MAX=9
TOOL_NAME=pkg_add

do_test() {
	i=$1
	RUN=$2
	LIBPKG=$3

	build_chroot
	if [ "X$LIBPKG" != "X" ]; then
		cp ${TOOL_NAME} ${BASE_DIR}/base/usr/sbin/${TOOL_NAME}
	fi
	chroot ${BASE_DIR}/base /run.sh ${PACKAGE} ${i} > ${RUN}.stdout.${i} 2> ${RUN}.stderr.${i}
	# Get the mtree file to use to compare the filesystems
	mtree -c -p ${BASE_DIR}/base | grep -v "^\#[[:space:]]*date:" | sed "s/time=[^ ]*//" | grep -v "^[ ]*${TOOL_NAME}[^\.]" > ${RUN}.mtree.${i}
	# Create a tarball of the important dir's to compare later
	rm ${BASE_DIR}/${RUN}-${i}.tar
	tar -cf ${BASE_DIR}/${RUN}-${i}.tar ${BASE_DIR}/base/var/db/pkg ${BASE_DIR}/base/usr/local ${BASE_DIR}/base/usr/pkg
	# Cleanup the package directories
	rm -fr ${BASE_DIR}/base/var/db/pkg ${BASE_DIR}/base/usr/local ${BASE_DIR}/base/usr/pkg
}

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
