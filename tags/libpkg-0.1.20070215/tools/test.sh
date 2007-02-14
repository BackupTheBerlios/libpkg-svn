# This is an acceptence test for pkg_delete
# It will test pkg_delete in various senarios and check if the
# output is correct and if the filesystem is correct afterwards

# Set this to the parent directory to extract the chroot environment to
#BASE_DIR=/a
# The file to extract to create the chroot environment
# This must extract the the files to base/*
BASE_TARBALL=${BASE_DIR}/base.tar

# The location of the package files to copy to the chroot
#PACKAGE_DIR=/some/dir/containing/packages
# The packages to copy
#PACKAGES="bash-3.0.16_1.tbz gettext-0.14.5.tbz libiconv-1.9.2_1.tbz"
#the package to install
#PACKAGE=bash-3.0.16_1

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

