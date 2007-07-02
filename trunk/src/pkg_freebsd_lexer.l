%{
/*
 * Copyright (C) 2006, Andrew Turner
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include "pkg_freebsd_parser.h"

#include <errno.h>
#include <string.h>

int pkg_freebsd_lex(void);
void pkg_freebsd_error(const char *);

static unsigned int pkg_line = 1;

#define YY_NO_UNPUT
%}

%x IN_COMMENT

%%
^@comment[ ]		{ BEGIN IN_COMMENT; return COMMENT; }
^@name\ ([^\n])*$	{
	pkg_freebsd_lval.str_val = strdup(yytext + 6);
	return NAME;
			}
^@cwd\ [^\n]*$		{
	pkg_freebsd_lval.str_val = strdup(yytext + 5);
	return CWD;
			}
^@pkgdep\ ([^\n]*)$	{
	pkg_freebsd_lval.str_val = strdup(yytext + 8);
	return PKGDEP;
			}
^@conflicts\ .*$	{
	pkg_freebsd_lval.str_val = strdup(yytext + 11);
	return CONFLICTS;
			}
^@exec\ .*$		{
	pkg_freebsd_lval.str_val = strdup(yytext + 6);
	return EXEC;
			}
^@unexec\ .*$		{
	pkg_freebsd_lval.str_val = strdup(yytext + 8);
	return UNEXEC;
			}
^@ignore		{ return IGNORE; }
^@dirrm\ .*$		{
	pkg_freebsd_lval.str_val = strdup(yytext + 7);
	return DIRRM;
			}
^@mtree\ .*$		{
	pkg_freebsd_lval.str_val = strdup(yytext + 7);
	return MTREE;
			}
^@display\ .*$		{
	pkg_freebsd_lval.str_val = strdup(yytext + 9);
	return DISPLAY;
			}
<IN_COMMENT>DELETED:.*$ {
	pkg_freebsd_lval.str_val = strdup(yytext);
	return DATA;
			}
<IN_COMMENT>DEPORIGIN:.*$ {
	pkg_freebsd_lval.str_val = strdup(yytext + 10);
	return DEPORIGIN;
			}
<IN_COMMENT>ORIGIN:.*$	{
	pkg_freebsd_lval.str_val = strdup(yytext + 7);
	return ORIGIN;
			}
<IN_COMMENT>MD5:[0-9a-f]{32}$ {
	pkg_freebsd_lval.str_val = strdup(yytext + 4);
	return MD5;
			}
<IN_COMMENT>PKG_FORMAT_REVISION:1.1.*$ {
	return FORMAT_1_1;
			}
<IN_COMMENT>.+$		{
	pkg_freebsd_lval.str_val = strdup(yytext);
	return DATA;
			}
<IN_COMMENT>\n		{ BEGIN 0; pkg_line++; return NL; }
^[^@].*$		{
	pkg_freebsd_lval.str_val = strdup(yytext);
	return PKGFILE;
			}
\n			{ pkg_line++; return NL; }
%%
int
pkg_freebsd_wrap()
{
	pkg_line = 1;
	return 1;
}

void
pkg_freebsd_error(const char *msg __unused)
{
	printf("ERROR line %d: %s\n", pkg_line, msg);
}
