#!/bin/sh

PACKAGE=$1
shift
TESTNO=$1

if [ ${TESTNO} -eq 1 ] ; then
	pkg_add ./${PACKAGE}.tbz
	pkg_info
	if [ $? -ne 0 ] ; then
		exit 1
	fi
elif [ ${TESTNO} -eq 2 ] ; then
	pkg_add -p /usr/pkg ./${PACKAGE}.tbz
	pkg_info
	if [ $? -ne 0 ] ; then
		exit 1
	fi
elif [ ${TESTNO} -eq 3 ] ; then
	pkg_add ./${PACKAGE}.tbz
	pkg_info ${PACKAGE}
	if [ $? -ne 0 ] ; then
		exit 1
	fi
elif [ ${TESTNO} -eq 4 ] ; then
	pkg_add -p /usr/pkg ./${PACKAGE}.tbz
	pkg_info ${PACKAGE}
	if [ $? -ne 0 ] ; then
		exit 1
	fi
elif [ ${TESTNO} -eq 5 ] ; then
	pkg_add ./${PACKAGE}.tbz
	pkg_info -f ${PACKAGE}
	if [ $? -ne 0 ] ; then
		exit 1
	fi
elif [ ${TESTNO} -eq 6 ] ; then
	pkg_add -p /usr/pkg ./${PACKAGE}.tbz
	pkg_info -f ${PACKAGE}
	if [ $? -ne 0 ] ; then
		exit 1
	fi
fi
