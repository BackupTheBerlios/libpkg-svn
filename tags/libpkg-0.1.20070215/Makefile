SUBDIR=src tools tests

docs:
	/usr/local/bin/doxygen Doxyfile

test:
	cd ${.CURDIR}/tests && make run_test

.PHONEY: test

.include <bsd.subdir.mk>
