/* ts <tss> : replace all contiguous SPACEs up to a <tss> tabstop> in
 * standard input by a TAB, copying the result to standard out.
 * Use:	ts <tss> <infile> | tx 5 | lpr
 * will replace all SPACEs in <infile> aligned to a tabstop of <tss> for
 * SPACEs aligned to a tabstop of 5, and send the result to the lineprinter

 * (C) (R) R. J. Bertin 20/12/1989  ** ICR
 :ts=4
 */

#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include "local/Macros.h"

char *calloc();

#ifndef serror
#	define serror() ((errno>0&&errno<sys_nerr)?sys_errlist[errno]:"invalid errno")
#endif

int verbose= 0;
long line= 1L;
FILE *StdErr;

int no_repl( c, ok)
register char c;
register int ok;
{	static int quote_ok= 1, comment_ok= 1, prev_ok= 1, prev_qok= 1;
	static char prev= EOF;
	static int qsline= 0;

	if( c== '\n')
		line++;
	switch( c){
		case '"':
			if( !(quote_ok== 0 && prev== '\\') ){
				prev_qok= quote_ok;
				if( !(quote_ok= !quote_ok) ){
					qsline= line;
				}
			}
			break;
		case '*':
			if( prev== '/'){
				comment_ok= 0;
			}
			break;
		case '/':
			if( prev== '*'){
				comment_ok= 1;
			}
			break;
		default:
			prev= c;
			return( ok);
			break;
	}
	ok= quote_ok && comment_ok;
	if( verbose){
		if( !ok && prev_ok){
			if( !quote_ok)
				fprintf( StdErr, "Line %ld: string (%c%c)", line, prev, c);
			if( !comment_ok)
				fprintf( StdErr, "Line %ld: comment ", line);
			fputs( "-> off ; ", StdErr);
		}
		else if( ok && !prev_ok){
			fprintf( StdErr, "back on in line %ld (%c%c)", line, prev, c);
			if( !prev_qok && quote_ok && qsline!= line ){
				fprintf( StdErr, " Warning: multiline string!");
			}
			fputc( '\n', StdErr );
		}
	}
	prev= c;
	prev_ok= ok;
	prev_qok= quote_ok;
	return( ok);
}

main( argc, argv)
int argc;
char **argv;
{	register int i, c, cnt= 0;
	int spaces, ok= 1, arg= 1;
	register char *ss;
	register long tot= 0L;
	FILE *fi, *fo;

	StdErr= stderr;

	if( argc< 2){
		fprintf( StdErr, "Shrink SPACES to TABS:\n");
		fprintf( StdErr, "Usage: %s [-]<tabstop size> [-v] [infile] [outfile]\n", argv[0]);
		exit( 1);
	}
	spaces= abs(atoi( argv[arg++]));
	if( spaces<= 0)
		spaces= 8;
	errno= 0;
	if( arg< argc)
		if( strcmp( argv[arg], "-v")== 0){
			arg++;
			verbose= 1;
/* 			StdErr= stdout;	*/
		}
	if( arg< argc)
		fi= fopen( argv[arg++], "r");
	else
		fi= stdin;
	if( fi== NULL){
		fprintf( StdErr, "Input error on %s", argv[arg-1]);
		if( errno)
			fprintf( StdErr, ": %s", serror());
		fprintf( StdErr, "\n");
		exit( 1);
	}
	errno= 0;
	if( arg< argc)
		fo= fopen( argv[arg++], "w");
	else
		fo= stdout;
	if( fo== NULL){
		fprintf( StdErr, "Output error");
		if( errno)
			fprintf( StdErr, ": %s", serror());
		fprintf( StdErr, "\n");
		exit( 1);
	}
	if( spaces< 0){
		fprintf( StdErr, "%s: 4 space replaced by 1 tab\n", argv[0]);
		spaces= 4;
	}
	c= getc( fi);
	while( c!= EOF){
		ok= no_repl( c, ok);
		if( c== ' ' && ok){					/* we may condense	*/
		int spc= 0;							/* don't condense only one SPACE, so	*/
			c= getc( fi);					/* get next char	*/
			cnt++;							/* next cursor pos	*/
			if( c== ' ')					/* second contiguous SPACE	*/
				spc++;						/* increment SPACE counter	*/
			ok= no_repl( c, ok);			/* check this too	*/
			while( ok && c== ' ' && cnt % spaces && c!= EOF && c!= '\n'){
				cnt++;						/* read contiguous spaces until EOL 	*/
				spc++;						/* or next TABSTOP	*/
				c= getc( fi);
				ok= no_repl( c, ok);		/* check this too	*/
			}
			if( cnt % spaces== 0 && spc> 1){
				putc( '\t', fo);			/* TABSTOP && > 1 SPACE	*/
				if( c!= ' ')
					putc( c, fo);			/* don't loose this c!	*/
				tot+= 1L;
			}
			else{				/* we read too little for next TABSTOP	*/
				for( i= 0; i<= spc; i++)	/* so write'em	*/
					putc( ' ', fo);
				putc( c, fo);
			}
/* 			cnt++;	*/
		}
		else{
			putc( c, fo);
#ifdef KKKKK
			if( c== '\n')
				cnt= 0;						/* new line	*/
			else
				cnt++;						/* next cursor pos	*/
#endif
		}
		if( c== '\n')
			cnt= 0;							/* new line	*/
		else
			cnt++;							/* next cursor pos	*/
		c= getc( fi);
	}
	if( fi!= stdin)
		fclose( fi);
	if( fo!= stdout)
		fclose( fo);
	if( tot)
		fprintf( StdErr, "%c%s: %ld x ( %d SPACES -> 1 TAB)\n", 0x07, argv[0], tot, spaces);
	exit( 0);
}
