%{
#include "pkg.h"

#include <assert.h>
#include <errno.h>
#include <stdio.h>

#define YYPARSE_PARAM param
#define YYPARSE_PARAM_TYPE struct pkg_manifest **

int pkg_freebsd_lex(void);
int pkg_freebsd_parse(YYPARSE_PARAM_TYPE);
void pkg_freebsd_error(const char *);

static struct pkg_manifest_item *curitem = NULL;
static struct pkg *curdep = NULL;
static struct pkg_manifest *pkg_manifest = NULL;
%}

%union {
	char *str_val;
	struct pkg_manifest_item *item;
};

%token COMMENT FORMAT_1_1 IGNORE NL
%token <str_val> CONFLICTS CWD DATA DEPORIGIN DIRRM DISPLAY EXEC MD5 MTREE
%token <str_val> NAME ORIGIN PKGDEP PKGFILE UNEXEC

%type <item> contents_line

%%
contents_file:
	COMMENT FORMAT_1_1 NL head_1_1 data_1_1 {
		(*YYPARSE_PARAM) = pkg_manifest;

		/* Reset static values for the next run */
		curitem = NULL;
		curdep = NULL;
		pkg_manifest = NULL;
	}

head_1_1:
	NAME NL COMMENT ORIGIN NL CWD NL {
		assert(pkg_manifest == NULL);
		if (pkg_manifest == NULL)
			pkg_manifest = pkg_manifest_new();
		pkg_manifest_set_name(pkg_manifest, $1);
		free($1);

		/* Set the package origin */
		pkg_manifest_set_attr(pkg_manifest, pkgm_origin, $4);
		free($4);

		/* Set the package prefix */
		pkg_manifest_set_attr(pkg_manifest, pkgm_prefix, $6);
		free($6);
	}

data_1_1:
	| data_1_1 contents_line NL {
		if (pkg_manifest == NULL)
			pkg_manifest = pkg_manifest_new();
		pkg_manifest_append_item(pkg_manifest, $2);
	}
	| data_1_1 PKGDEP NL {
		struct pkg *pkg;
		assert(pkg_manifest != NULL);

		pkg = pkg_new_freebsd_empty($2);
		pkg_manifest_add_dependency(pkg_manifest, pkg);
		free($2);
		curdep = pkg;
		curitem = NULL;
	}
	| data_1_1 CONFLICTS NL {
		assert(pkg_manifest != NULL);

		pkg_manifest_add_conflict(pkg_manifest, $2);
		free($2);
		curdep = NULL;
		curitem = NULL;
	}
	| data_1_1 COMMENT comment_value NL;

contents_line
	: PKGFILE {
		$$ = pkg_manifest_item_new(pmt_file, $1);
		free($1);
		curitem = $$;
		curdep = NULL;
	}
	| CWD {
		$$ = pkg_manifest_item_new(pmt_chdir, $1);
		free($1);
		curitem = $$;
		curdep = NULL;
	}
	| EXEC {
		$$ = pkg_manifest_item_new(pmt_execute, $1);
		free($1);
		curitem = $$;
		curdep = NULL;
	}
	| UNEXEC {
		$$ = pkg_manifest_item_new(pmt_execute, $1);
		pkg_manifest_item_set_attr($$, pmia_deinstall, "YES");
		free($1);
		curitem = $$;
		curdep = NULL;
	}
	| DIRRM {
		$$ = pkg_manifest_item_new(pmt_dir, $1);
		free($1);
		curitem = $$;
		curdep = NULL;
	}
	| MTREE {
		$$ = pkg_manifest_item_new(pmt_dirlist, $1);
		free($1);
		curitem = $$;
		curdep = NULL;
	}
	| DISPLAY {
		$$ = pkg_manifest_item_new(pmt_output, $1);
		free($1);
		curitem = $$;
		curdep = NULL;
	}
	| IGNORE NL PKGFILE {
		$$ = pkg_manifest_item_new(pmt_file, $3);
		free($3);
		pkg_manifest_item_set_attr(curitem, pmia_ignore, NULL);
		curitem = $$;
		curdep = NULL;
	}
	;

comment_value
	: DATA {
		pkg_manifest_item_set_attr(curitem, pmia_other, $1);
		free($1);
	}
	| DEPORIGIN {
		pkg_set_origin(curdep, $1);
		free($1);
	}
	| MD5 {
		pkg_manifest_item_set_attr(curitem, pmia_md5, $1);
		free($1);
	}
	;
%%
