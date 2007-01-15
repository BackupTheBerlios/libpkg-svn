#!/bin/sh

. `dirname $0`/../test.sh

# Rins the tests for either the libpkg or cvs version of pkg_add
do_test() {
	RUN=$1

	i=1
	while [ $i -le 2 ] ; do
		chroot ${BASE_DIR}/base /run.sh ${PACKAGE} ${i} > ${RUN}.stdout.${i} 2> ${RUN}.stderr.${i}
		# Get the mtree file to use to compare the filesystems
		mtree -c -p ${BASE_DIR}/base | grep -v "^\#[[:space:]]*date:" | sed "s/time=[^ ]*//" | grep -v "^[ ]*pkg_add" > ${RUN}.mtree.${i}
		# Create a tarball of the important dir's to compare later
		rm ${BASE_DIR}/${RUN}-${i}.tar
		tar -cf ${BASE_DIR}/${RUN}-${i}.tar ${BASE_DIR}/base/var/db/pkg ${BASE_DIR}/base/usr/local
		# Cleanup the package directories
		rm -fr ${BASE_DIR}/base/var/db/pkg ${BASE_DIR}/base/usr/local

		i=$((i+1))
	done
}

# Runs the test
run_test() {

	# Get the reference data from the FreeBSD cvs pkg_add
	build_chroot
	do_test cvs

	# Get the test data from out pkg_add
	build_chroot
	build_tool
	cp pkg_add ${BASE_DIR}/base/usr/sbin/pkg_add
	do_test libpkg
}

CWD=`pwd`
cd `dirname $0`

run_test

cd $CWD
