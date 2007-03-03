%{
#include "pkg.h"

#include <errno.h>
#include <stdio.h>

#define YYPARSE_PARAM param
#define YYPARSE_PARAM_TYPE struct pkg_manifest **

int pkg_freebsd_lex(void);
int pkg_freebsd_parse(YYPARSE_PARAM_TYPE);
void pkg_freebsd_error(const char *);

struct pkg_manifest_item *curitem = NULL;
struct pkg_manifest *pkg_manifest = NULL;
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
	COMMENT FORMAT_1_1 NL data_1_1 { (*YYPARSE_PARAM) = pkg_manifest; }

data_1_1:
	| data_1_1 contents_line NL {
		if (pkg_manifest == NULL)
			pkg_manifest = pkg_manifest_new();
		pkg_manifest_append_item(pkg_manifest, $2);
	}
	| data_1_1 COMMENT comment_value NL

contents_line
	: PKGFILE {
		$$ = pkg_manifest_item_new(pmt_file, $1);
		free($1);
		curitem = $$;
	}
	| CWD {
		$$ = pkg_manifest_item_new(pmt_chdir, $1);
		free($1);
		curitem = $$;
	}
	| NAME {
		$$ = pkg_manifest_item_new(pmt_pkgname, $1);
		free($1);
		curitem = $$;
	}
	| PKGDEP {
		$$ = pkg_manifest_item_new(pmt_dependency, $1);
		free($1);
		curitem = $$;
	}
	| CONFLICTS {
		$$ = pkg_manifest_item_new(pmt_conflict, $1);
		free($1);
		curitem = $$;
	}
	| EXEC {
		$$ = pkg_manifest_item_new(pmt_execute, $1);
		free($1);
		curitem = $$;
	}
	| UNEXEC {
		$$ = pkg_manifest_item_new(pmt_execute, $1);
		free($1);
		curitem = $$;
	}
	| DIRRM {
		$$ = pkg_manifest_item_new(pmt_dir, $1);
		free($1);
		curitem = $$;
	}
	| MTREE {
		$$ = pkg_manifest_item_new(pmt_dirlist, $1);
		free($1);
		curitem = $$;
	}
	| DISPLAY {
		$$ = pkg_manifest_item_new(pmt_output, $1);
		free($1);
		curitem = $$;
	}
	| IGNORE NL PKGFILE {
		$$ = pkg_manifest_item_new(pmt_file, $3);
		free($3);
		pkg_manifest_item_set_attr(curitem, pmia_ignore, NULL);
		curitem = $$;
	}
	;

comment_value
	: DATA {
		pkg_manifest_item_set_attr(curitem, pmia_other, $1);
		free($1);
	}
	| DEPORIGIN {
		pkg_manifest_item_set_attr(curitem, pmia_other, $1);
		free($1);
	}
	| ORIGIN {
		pkg_manifest_item_set_attr(curitem, pmia_other, $1);
		free($1);
	}
	| MD5 {
		pkg_manifest_item_set_attr(curitem, pmia_md5, $1);
		free($1);
	}
	;
%%
