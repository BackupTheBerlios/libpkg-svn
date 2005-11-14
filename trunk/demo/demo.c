#include <sys/stat.h>

#include <errno.h>
#include <pkg.h>
#include <pkg_db.h>
#include <pkg_repo.h>
#include <stdlib.h>

/* This is an internal function for the library not exported by pkg.h */
int pkg_dir_build(const char *);

void usage(const char *);

struct pkg_repo *repo_ftp = NULL;

void
usage(const char *arg)
{
	fprintf(stderr, "%s package [package ...]\n", arg);
}

int
install_package(struct pkg_db *db, struct pkg *pkg)
{
	struct pkg **pkgs;
	int is_installed;

	if (repo_ftp == NULL)
		repo_ftp = pkg_repo_new_ftp(NULL, NULL);

	pkg_repo_find_pkg(repo_ftp, pkg);
	
	is_installed = pkg_db_is_installed(db, pkg_get_name(pkg));
	if (is_installed != 0)
		return 0;
	
	pkgs = pkg_get_dependencies(pkg);
	if (pkgs != NULL) {
		unsigned int pos;

		for (pos = 0; pkgs[pos] != NULL; pos++) {
			if (install_package(db, pkgs[pos]) != 0) {
				return 1;
			}
			pkg_free(pkgs[pos]);
			pkgs[pos]=NULL;
		}
		free(pkgs);
	}
	if (pkg_db_install_pkg(db, pkg) != 0) {
		fprintf(stderr,
		    "ERROR: Couldn't install package %s\n", pkg_get_name(pkg));
		pkg_free(pkg);
		return 1;
	}
	pkg_free(pkg);

	return 0;
}

int
main (int argc, char *argv[])
{
	struct pkg_repo *repo_file;
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
	repo_file = pkg_repo_new_files();
	if (!repo_file) {
		fprintf(stderr, "ERROR: Couldn't open the package repo\n");
		return EXIT_FAILURE;
	}

	/* Try and install all the named packages */
	while(*argv) {
		struct pkg *pkg;
		int is_installed;

		pkg = pkg_repo_get_pkg(repo_file, *argv);
		if (pkg == NULL) {
			fprintf(stderr, "Package %s could not be found\n",
			    *argv);
			continue;
		}
		/* Check if the package is installed */
		is_installed = pkg_db_is_installed(pkg_db, pkg_get_name(pkg));
		if (is_installed == 0) {
			fprintf(stderr, "Package %s is already installed\n",
			    *argv);
			continue;
		} else {
			/* Install the package */
			struct pkg **pkgs;
			
			pkgs = pkg_get_dependencies(pkg);
			if (pkgs != NULL) {
				unsigned int pos;

				for (pos = 0; pkgs[pos] != NULL; pos++) {
					install_package(pkg_db, pkgs[pos]);
					pkg_free(pkgs[pos]);
					pkgs[pos]=NULL;
				}
				free(pkgs);
			}
			if (pkg_db_install_pkg(pkg_db, pkg) != 0) {
				fprintf(stderr,
				    "ERROR: Couldn't install package %s\n",
				    *argv);
			}
			pkg_free(pkg);
		}
		argv++;
	}

	pkg_repo_free(repo_file);
	if (repo_ftp != NULL)
		pkg_repo_free(repo_ftp);
	pkg_db_free(pkg_db);
}
