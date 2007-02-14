#!/bin/sh

PACKAGE=$1
shift
TESTNO=$1

if [ ${TESTNO} -eq 1 ] ; then
	pkg_add ./${PACKAGE}.tbz
	if [ $? -ne 0 ] ; then
		exit 1
	fi
elif [ ${TESTNO} -eq 2 ] ; then
	pkg_add -v ./${PACKAGE}.tbz
	if [ $? -ne 0 ] ; then
		exit 1
	fi
elif [ ${TESTNO} -eq 3 ] ; then
	pkg_add -n ./${PACKAGE}.tbz
	if [ $? -ne 0 ] ; then
		exit 1
	fi
elif [ ${TESTNO} -eq 4 ] ; then
	pkg_add -nv ./${PACKAGE}.tbz
	if [ $? -ne 0 ] ; then
		exit 1
	fi
elif [ ${TESTNO} -eq 5 ] ; then
	pkg_add -I ./${PACKAGE}.tbz
	if [ $? -ne 0 ] ; then
		exit 1
	fi
elif [ ${TESTNO} -eq 6 ] ; then
	pkg_add -R ./${PACKAGE}.tbz
	if [ $? -ne 0 ] ; then
		exit 1
	fi
elif [ ${TESTNO} -eq 7 ] ; then
	pkg_add -f ./${PACKAGE}.tbz
	if [ $? -ne 0 ] ; then
		exit 1
	fi
elif [ ${TESTNO} -eq 8 ] ; then
	pkg_add -p /usr/pkg ./${PACKAGE}.tbz
	if [ $? -ne 0 ] ; then
		exit 1
	fi
elif [ ${TESTNO} -eq 9 ] ; then
	pkg_add -P /usr/pkg ./${PACKAGE}.tbz
	if [ $? -ne 0 ] ; then
		exit 1
	fi
fi
