#!/bin/sh

PACKAGE=$1
shift
TESTNO=$1

if [ ${TESTNO} -eq 1 ] ; then
	pkg_add ./${PACKAGE}.tbz
	if [ $? -ne 0 ] ; then
		exit 1
	fi
	pkg_delete ${PACKAGE}
	exit $?
elif [ ${TESTNO} -eq 2 ] ; then
	pkg_add ./${PACKAGE}.tbz
	if [ $? -ne 0 ] ; then
		exit 1
	fi
	pkg_delete -v ${PACKAGE}
	exit $?
elif [ ${TESTNO} -eq 3 ] ; then
	pkg_add ./${PACKAGE}.tbz
	if [ $? -ne 0 ] ; then
		exit 1
	fi
	pkg_delete -n ${PACKAGE}
	exit $?
elif [ ${TESTNO} -eq 4 ] ; then
	pkg_add ./${PACKAGE}.tbz
	if [ $? -ne 0 ] ; then
		exit 1
	fi
	pkg_delete -D ${PACKAGE}
	exit $?
elif [ ${TESTNO} -eq 5 ] ; then
	pkg_add ./${PACKAGE}.tbz
	if [ $? -ne 0 ] ; then
		exit 1
	fi
	pkg_delete -r libiconv-1.9.2_1
	exit $?
elif [ ${TESTNO} -eq 6 ] ; then
	pkg_add ./${PACKAGE}.tbz
	if [ $? -ne 0 ] ; then
		exit 1
	fi
	pkg_delete -d ${PACKAGE}
	exit $?
elif [ ${TESTNO} -eq 7 ] ; then
	pkg_add ./${PACKAGE}.tbz
	if [ $? -ne 0 ] ; then
		exit 1
	fi
	pkg_delete -rd libiconv-1.9.2_1
	exit $?
fi
