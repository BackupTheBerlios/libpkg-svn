# This is an acceptence test for pkg_delete
# It will test pkg_delete in various senarios and check if the
# output is correct and if the filesystem is correct afterwards

pwd
if [ -f ./config.sh ]; then
	. ./config.sh
else
	echo "Copy config.sh.eg to config.sh and modify to suite your environment"
	exit 1
fi

# All the tools with tests
ALL_TESTS="pkg_delete"

# Builds the chroot dir for the test
build_chroot() {
	# Remove the base dir
	rm -fr ${BASE_DIR}/base
	chflags -R noschg ${BASE_DIR}/base
	rm -fr ${BASE_DIR}/base

	# Extract a clean base dir
	tar xzf ${BASE_TARBALL} -C ${BASE_DIR}
	cp ./run.sh ${BASE_DIR}/base/run.sh
	chmod +x ${BASE_DIR}/base/run.sh

	for p in ${PACKAGES} ; do
		cp ${PACKAGE_DIR}/${p} ${BASE_DIR}/base/
	done
}

# Builds the library and pkg_delete
build_tool() {
	wd=`pwd`
	cd ../../src && make
	cd $wd && make all
}

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

# Runs the test
run_test() {
	TEST_NO=$1

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

