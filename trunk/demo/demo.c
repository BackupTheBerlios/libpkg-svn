#include <sys/stat.h>

#include <errno.h>
#include <pkg.h>
#include <pkg_db.h>
#include <pkg_repo.h>
#include <stdlib.h>

/* This is an internal function for the library not exported by pkg.h */
int pkg_dir_build(const char *);

void usage(const char *);

void
usage(const char *arg)
{
	fprintf(stderr, "%s package [package ...]\n", arg);
}

int
main (int argc, char *argv[])
{
	struct pkg_repo *repo;
	struct pkg_db *pkg_db;

	if(!argv[1]) {
		warnx("missing package name(s)");
		usage(argv[0]);
		return EXIT_FAILURE;
	}

	argv++;
	
	if (pkg_dir_build("fakeroot/var/db/pkg") != 0) {
		fprintf(stderr, "ERROR: pkg_dir_build failed\n");
		return EXIT_FAILURE;
	}
	pkg_dir_build("fakeroot/usr/local");
	pkg_dir_build("fakeroot/usr/X11R6");

	/* Open the package database and repo */
	pkg_db = pkg_db_open_freebsd("fakeroot");
	if (!pkg_db) {
		fprintf(stderr, "ERROR: Couldn't open the package database\n");
		return EXIT_FAILURE;
	}
	repo = pkg_repo_new_files();
	if (!repo) {
		fprintf(stderr, "ERROR: Couldn't open the package repo\n");
		return EXIT_FAILURE;
	}

	/* Try and install all the named packages */
	while(*argv) {
		struct pkg *pkg;
		int is_installed;

		/* Check if the package is installed */
		is_installed = pkg_db_is_installed(pkg_db, *argv);
		if (is_installed == 0) {
			fprintf(stderr, "Package %s is already installed\n",
			    *argv);
			continue;
		/* Get the package from the repo */
		} else if ((pkg = pkg_repo_get_pkg(repo, *argv)) == NULL) {
			fprintf(stderr, "Package %s could not be found\n",
			    *argv);
		} else {
			/* Install the package */
			if (pkg_db_install_pkg(pkg_db, pkg) != 0) {
				fprintf(stderr,
				    "ERROR: Couldn't install package %s\n",
				    *argv);
			}
			pkg_free(pkg);
		}
		argv++;
	}

	pkg_repo_free(repo);
	pkg_db_free(pkg_db);
}
