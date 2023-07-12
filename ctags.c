/* :ts=4	*/
/* $Header */

static char *sccsid = "@(#)ctags.c	4.7 (Berkeley) 8/18/83";

/* added by RJB	*/
#include <local/cpu.h>
#include <local/Macros.h>
IDENTIFY( "tagfile generator, **RJB");

#define OTHER_MACHINES
#ifdef MCH_AMIGA
#	undef OTHER_MACHINES
#endif
#ifdef _MAC_MPWC_
#	undef OTHER_MACHINES
#endif

#include <stdio.h>
#include <ctype.h>
#include <errno.h>

/*
 * ctags: create a tags file
 */

#define	reg	register
#define	bool	char

#define	TRUE	(1)
#define	FALSE	(0)

#define	iswhite(arg)	(_wht[arg])		/* T if char is white		*/
#define	begtoken(arg)	((macdef==begin)?!iswhite(arg):_btk[arg])	/* T if char can start token	*/
#define	intoken(arg)	(_itk[arg])		/* T if char can be in token	*/
#define	endtoken(arg)	(_etk[arg])		/* T if char ends tokens	*/
#define	isgood(arg)	(_gd[arg])			/* T if char can be after ')'	*/

/*
 * The following ifndefs were necessary to get this to compile on an Amiga.
 * The original of this version came over usenet (mod.sources Nov85).
 * I also applied the bug fix mentioned in comp.bugs.4bsd Mar88.
 *
 * Compile with Amiga Lattice C using : lc -cw -L ctags
 *
 * G. R. Walter (Fred) December 30, 1988
 */
#ifndef AMIGA
# define	max(I1,I2)	(I1 > I2 ? I1 : I2)
#endif
#ifdef AMIGA
#	define entry EnTrY
#	define index(S, C) strchr(S, C)
#	define cfree(C) free(C)
#endif

#ifdef MCH_AMIGA
	extern char *malloc(), *fgets();
	extern void *Output();
#	define cfree(C) free(C)
#endif

extern char *getenv();

/* Macintosh MPW port by RJB 21-12-90
 * _MAC_MPWC_ has to be defined in cpu.h
 */
#ifdef _MAC_MPWC_
#	include <stdlib.h>
#	include <stdarg.h>
#	define cfree free
#	define index strchr
	char search_start[]= "\xa5";
#endif _MAC_MPWC_

#include <string.h>


typedef struct	nd_st {			 	/* sorting structure		*/
	char	*entry;					/* function or type name	*/
	char	*file;					/* file name			*/
	bool	f;						/* use pattern or line no	*/
	int	lno;						/* for -x option		*/
	char	*pat;					/* search pattern		*/
	bool	been_warned;			/* set if noticed dup		*/
	struct	nd_st	*left,*right;	/* left and right sons		*/
} NODE;

long	ftell();

bool	number,						/* T if on line starting with #	*/
	term	= FALSE,				/* T if print on terminal	*/
	makefile= TRUE,					/* T if to creat "tags" file	*/
	gotone,							/* found a func already on line	*/
									/* boolean "func" (see init)	*/
	_wht[0177],_etk[0177],_itk[0177],_btk[0177],_gd[0177];

	/* typedefs are recognized using a simple finite automata,
	 * tydef is its state variable.
	 */
typedef enum {none, begin, middle, end } TYST;

typedef enum { off, newline} DEBUGSWITCHES;

DEBUGSWITCHES debugswitch=  off;

TYST tydef = none, macdef= none;

char	searchar = '/';			/* use /.../ searches 		*/

int	lineno;						/* line number of current line */
int macline;					/* line number of last macro definition	*/
char	line[4*BUFSIZ],			/* current input line			*/
	*curfile,					/* current input file name		*/
	*outfile= "tags",			/* output file				*/
	*white	= " \f\t\n",		/* white chars				*/
	*endtk	= " \t\n\"'#()[]{}=-+%*/&|^~!<>;,.:?",
				/* token ending chars			*/
	*begtk	= "ABCDEFGHIJKLMNOPQRSTUVWXYZ_abcdefghijklmnopqrstuvwxyz",
				/* token starting chars			*/
	*intk	= "ABCDEFGHIJKLMNOPQRSTUVWXYZ_abcdefghijklmnopqrstuvwxyz0123456789",
				/* valid in-token chars			*/
	*notgd	= ",;";				/* non-valid after-function chars	*/

int	file_num= 1;				/* current file number			*/
int	aflag= 0;					/* -a: append to tags */
int	tflag= 0;					/* -t: create tags for typedefs */
int	mflag= 0;					/* -m: include all macro definitions	*/
int	uflag= 0;					/* -u: update tags */
int	wflag= 0;					/* -w: suppress warnings */
int	vflag= 0;					/* -v: create vgrind style index output */
int	xflag= 0;					/* -x: create cxref style output */
char lang[64];
#ifdef __STDC__
	int	STDC_defined= 1;		/* -STDC: emulate '#define __STDC__'	*/
#else
	int	STDC_defined= 0;		/* -STDC: emulate '#define __STDC__'	*/
#endif

char	lbuf[4*BUFSIZ];
#ifdef MCH_AMIGA
#	define PATSIZE 25
#else
#	define PATSIZE BUFSIZ
#endif

#ifdef DEBUG
void lperror( const char *msg )
{
	perror( msg );
}

#	define perror(msg)	lperror(msg)

#endif

  /* local version of rename that deals with cross-filesystem moves.
   \ If rename() fails, it will bluntly construct and invoke (through system() the
   \ necessary shell command. Note that this currently works on unix like systems
   \ only.
   */
int lrename( const char *src, const char *dest )
{ char cmd[1024];
  int r;
	if( (r= rename( src, dest)) ){
		sprintf( cmd, "Error renaming %s to %s, trying mv", src, dest );
		perror( cmd );
		errno= 0;
		sprintf( cmd, "ls -l %s ; mv %s %s ; ls -l %s", src, src, dest, dest );
		fprintf( stderr, ">> %s\n", cmd );
		r= system( cmd );
	}
	return(r);
}

FILE	*inf,					/* ioptr for current input file		*/
	*outf;						/* ioptr for tags file			*/

long	lineftell, currentlineftell;				/* ftell after getc( inf ) == '\n' 	*/

NODE	*head;					/* the head of the sorted binary tree	*/

char	*savestr(), *savenstr();
#ifndef __STDC__
#ifdef AMIGA
char	*rindex(), *strchr();
#else
char	*rindex(), *index();
#endif
#endif
char	*toss_comment();
long put_entries();

usage()
{
	fprintf( stderr, "Usage: ctags [-BFat[l]muwvx] [-[DU]STDC] [-Ifile] file ...\n");
	fputs( "\t-B\tgenerate backsearch patterns\n", stderr);
	fputs( "\t-F\tgenerate forwardsearch patterns\n", stderr);
	fputs( "\t-a\tappend to tags file\n", stderr);
	fputs( "\t-m\tinclude tags for macro definitions\n", stderr);
	fputs( "\t-t[level]\tinclude tags for typedef statements - default level is 1\n", stderr);
	fputs( "\t-u\tupdate in tags file\n", stderr);
	fputs( "\t-w\tsuppress warnings\n", stderr);
	fputs( "\t-v\tcreate vgrind style index output\n", stderr);
	fputs( "\t-x\tcreate cxref style output\n", stderr);
	fputs( "\t--lang <language>\toverride the default extension-derived language selection.\n"
		"\t\tlisp lex yacc c fortran\n", stderr
	);
	fprintf( stderr, "\t-DSTDC\temulate a #define __STDC__ statement%s\n",
		(STDC_defined)?" (default)":""
	);
	fprintf( stderr, "\t-USTDC\temulate a #undef __STDC__ statement%s\n",
		(!STDC_defined)?" (default)":""
	);
	fputs( "\t-Ifile\tinclude tags-file <file> in the generated tags file\n", stderr );
	fputs( "\tFiles named tags are also directly included\n", stderr );
	exit(1);
}

int copied= 0;

int copy_tags( char *fname )
{  FILE *fp;
   char dirname[1024], buf[1024], *c;
   int len;
   extern char *rindex();
	if( (fp= fopen( fname, "r" )) ){
		strcpy( dirname, fname );
		{ char *c= rindex( dirname, '/');
			if( c ){
				fprintf( stderr, "; refering to \"%s\"'s directory ",
					&c[1]
				);
				c[1]= '\0';
				fprintf( stderr, "\"%s\" ", dirname );
				fflush( stderr );
			}
			else{
				dirname[0]= '\0';
			}
		}
		do{
			c= fgets( buf, sizeof(buf), fp);
			if( c && !feof(fp) && !ferror(fp) ){
				len= strlen(buf);
				if( len> 1 ){
					if( buf[len-1]== '\n' ){
						copied+= 1;
					}
					if( dirname[0] ){
					  char *c= buf;
						while( *c && *c!= '\t' ){
							fputc( *c++, outf );
						}
						if( *c ){
							fputc( *c++, outf );
							fputs( dirname, outf);
							fputs( c, outf );
						}
					}
					else{
						fputs( buf, outf );
					}
				}
			}
		}
		while( c && !feof(fp) && !ferror(fp) );
		fclose(fp);
		return(1);
	}
	else{
		fprintf( stderr, "Can't copy from file " );
		perror( fname );
		return(0);
	}
}

sort_tags( cmd, outfile, templ_nam, tnam, max_templ_len)
char *cmd, *outfile, *templ_nam, *tnam;
int max_templ_len;
{
	sprintf( cmd, "egrep >%s -v -f %s %s", outfile, templ_nam, tnam);
	if( tnam){
		unlink( tnam);
		errno= 0;
		if( !lrename( outfile, tnam) && !errno){
		  int ret;
				fprintf( stderr, ">> %s\n", cmd );
			ret= (system(cmd) >> 8);
				sprintf( cmd, "cat %s ; wc %s %s %s", templ_nam, templ_nam, outfile, tnam );
				system( cmd);
			if( ret== 2){
				fprintf( stderr, "ctags: error sorting (EGREP_EXPR_LENGTH==%d)\n", max_templ_len);
				unlink(outfile);
				lrename(tnam, outfile);
			}
			unlink(tnam);
		}
		else
			perror( tnam);
	}
	else{
		sprintf( cmd, "ctags: could not open temporary file '%s'", tnam);
		perror( cmd);
	}
}

#if defined(sgi) || defined(linux) || defined(__MACH__) || defined(__APPLE_CC__)
const char *strrstr( const char *a,  const char *b)	/* return pointer to first occurence of b in a */
{ unsigned int len;
  int success= 0;
  const char *A= &a[strlen(a)-1];
	
	len= strlen(b);
	while( !( success= !(strncmp(A, b, len)) ) && A> a ){
		A--;
	}
	if( !success )
		return(NULL);
	else
		return(A);
}
#endif

char *is_tags( char *name )
{ char *c;
	if( !strncmp( name, "-I", 2 ) ){
		return( &name[2] );
	}
	else if( (c= strrstr( name, "tags" )) ){
		if( c[4]== '\0' ){
			if( c== name || c[-1]== '/' ){
				return(name);
			}
			else{
				return(0);
			}
		}
		else{
			return(0);
		}
	}
	else{
		return(0);
	}
}

#	ifdef __STDC__
main( int ac, char *av[] )
#	else
main(ac,av)
  int	ac;
  char	*av[];
#	endif
{
	char cmd[128];
	char *tweednam= NULL;
	int i, copied= 0;

	if (ac <= 1) 
		usage();
	lang[0]= '\0';
	while (ac > 1 && av[1][0] == '-' &&
		strcmp( av[1], "-DSTDC") && strcmp( av[1], "-USTDC") && !is_tags( av[1])
	){
		for (i=1; av[1][i]; i++) {
			switch(av[1][i]) {
			  case 'B':
#ifdef _MAC_MPWC_
				searchar= '\\';
				strcpy( search_start, "\xb0");
#else
				searchar='?';
#endif _MAC_MPWC_
				break;
			  case 'F':
				searchar='/';
				break;
			  case 'a':
				aflag++;
				break;
			  case 'd':
			  case 'm':
				mflag++;
				break;
			  case 't':{
			  char c;
				if( isdigit( (c=av[1][i+1])) ){
					i++;
					tflag= (int)(c- '0');
				}
				else{
					tflag= 1;
				}
				break;
			  }
			  case 'u':
				uflag++;
				break;
			  case 'w':
				wflag++;
				break;
			  case 'v':
				vflag++;
				xflag++;
				break;
			  case 'x':
				xflag++;
				break;
			  case '-':
				  if( strcmp( av[1], "--lang" )== 0 ){
					  if( ac<= 2 ){
						  usage();
					  }
					  else{
						  strncpy( lang, av[2], 63 );
					  }
					  ac--, av++;
					  goto next_arg;
				  }
				  break;
			  default:
				usage();
			}
		}
next_arg:;
		ac-- ; av++;
	}


	init();			/* set up boolean "functions"		*/

	if( uflag){
	  FILE *tags;
		if( !(tags= fopen( "tags", "r")) )
			uflag= 0;
		else
			fclose(tags);
	}

	/*
	 * loop through files finding functions
	 */
	for( ; file_num < ac; file_num++){
		if( !strcmp( av[file_num], "-DSTDC") ){
			STDC_defined= 1;
			fputs( "__STDC__ defined\n", stderr);
		}
		else if( !strcmp( av[file_num], "-USTDC") ){
			STDC_defined= 0;
			fputs( "__STDC__ undefined\n", stderr);
		}
		else{
			if( !is_tags( av[file_num]) ){
				fprintf( stderr, "%s: ", av[file_num]);
				fflush( stderr);
				find_entries(av[file_num]);
				fputc( '\n', stderr);
			}
			else if( uflag ){
				if( !tweednam ){
					tweednam= tempnam( NULL, "ITAGS" );
					if( tweednam ){
						unlink( tweednam );
					}
					else{
						perror( tweednam );
					}
				}
				if( tweednam ){
					fprintf( stderr, "Listing files tagged in '%s'.. ", is_tags( av[file_num] ) );
					fflush( stderr );
					sprintf( cmd, "cut -f 2 '%s' | sort -u >> '%s'",
						is_tags( av[file_num] ), tweednam
					);
					system( cmd );
					fputs( "done\n", stderr );
				}
			}
		}
		fflush( stderr);
	}

	if (xflag) {
		fprintf( stderr, "Tagfile %s: %ld lines\n", outfile, put_entries(head) );
		exit(0);
	}
	if( uflag){
	  FILE *tags;
		if( !(tags= fopen( "tags", "r")) )
			uflag= 0;
		else
			fclose(tags);
	}
	if (uflag) {
		fputs( "Weeding out obsolete tags:", stderr);
		fflush( stderr);
		if( tweednam ){
		  char *tnam= tempnam( NULL, "OTAGS");
		  FILE *fp= fopen( tweednam, "r" );
			if( fp ){
				fclose(fp);
				fprintf( stderr, " <included tags files> " );
				fflush( stderr );
				if( tnam){
					unlink( tnam);
					errno= 0;
					if( !lrename( "tags", tnam) && !errno ){
						sprintf(cmd,
							"fgrep > tags -v -f '%s' %s", tweednam, tnam
						);
						system(cmd);
						unlink(tnam);
						unlink(tweednam);
					}
					else{
						perror( "tags" );
					}
				}
				else{
					sprintf( cmd, "ctags: could not open temporary file '%s'", tnam);
					perror( cmd);
				}
			}
			else{
				fputs( "Error opening ", stderr);
				perror( tweednam );
			}
		}
#ifdef NO_EGREP
		for (i=1; i<ac; i++) {
			if( !strcmp( av[i], "-DSTDC") || !strcmp( av[i], "-USTDC") || is_tags( av[i]) )
				continue;
			fputc( ' ', stderr);
			fputs( av[i], stderr);
			fflush( stderr);
#	ifdef MCH_AMIGA
			unlink( "OTAGS");
			lrename( outfile, "OTAGS");
			sprintf(cmd, "fgrep >%s -v \" %s \" OTAGS",
				outfile, av[i]
			);
			Execute( cmd, NULL, Output());
			unlink( "OTAGS");
#	else
{
	char *tnam= tempnam( NULL, "OTAGS");
			if( tnam){
				unlink( tnam);
				errno= 0;
				if( !lrename( outfile, tnam) && !errno){
					sprintf(cmd,
/* 					"mv %s %s;fgrep >%s -v '\t%s\t' %s;rm %s",	*/
						"fgrep >%s -v '\t%s\t' %s",
						outfile, av[i], tnam
					);
					system(cmd);
					unlink(tnam);
				}
				else{
					perror( tnam);
				}
			}
			else{
				sprintf( cmd, "ctags: could not open temporary file '%s'", tnam);
				perror( cmd);
			}
}
#	endif	/* MCH_AMIGA	*/
		}
#else
{ char *templ_nam= tempnam( NULL, "OTAGS");
  FILE *weed_fp;
  int max_templ_len= -1;
#	if defined(_APOLLO_SOURCE) || defined(_AUX_SOURCE)
  char *c= getenv( "EGREP_EXPR_LENGTH");
	max_templ_len= -128;
	if( c){
	  int x= atoi(c);
		if( x> 0)
			max_templ_len= x;
	}
#	endif
		if( (weed_fp= fopen( templ_nam, "w")) ){
		  char *tnam= tempnam( NULL, "OTAGS");
		  int templ_len= 0;
			for( i= 1; i< ac; i++){
				if( strcmp( av[i], "-DSTDC") && strcmp( av[i], "-USTDC") && !is_tags( av[i]) ){
					fprintf( stderr, "%s ", av[i]);
					fflush( stderr);
					templ_len+= fprintf( weed_fp, "\t%s\t\n", av[i]);
					fflush( weed_fp);
				}
#	if defined(_APOLLO_SOURCE) || defined(_AUX_SOURCE)
				if( max_templ_len> 0 && templ_len>= max_templ_len - (3 + strlen(av[i+1])) ){
					fputc( '\n', stderr);
					fclose(weed_fp);
					sort_tags( cmd, outfile, templ_nam, tnam, max_templ_len);
					templ_len= 0;
					weed_fp= freopen( templ_nam, "w", weed_fp);
				}
#	endif
			}
			if( templ_len> 0){
				fclose(weed_fp);
				sort_tags( cmd, outfile, templ_nam, tnam, max_templ_len);
				fputc( '\n', stderr);
			}
			fflush( stderr);
			unlink( templ_nam);
		}
}
#endif	/* NO_EGREP	*/
		aflag++;
		fputc( '\n', stderr);
		fflush( stderr);
	}
	outf = fopen(outfile, aflag ? "a" : "w");
	if( outf == NULL ){
	  char cwd[512], *TMPDIR;
		cwd[0]= '\0';
		getcwd( cwd, 512);
		fprintf( stderr, "%s/", cwd );
		perror(outfile);
		if( !(TMPDIR=getenv("TMPDIR")) || chdir(TMPDIR) ){
			TMPDIR="/tmp";
			if( chdir(TMPDIR) ){
				perror( TMPDIR );
				exit(1);
			}
			fprintf( stderr, "Warning: couldn't open %s in %s: %s tags in %s/%s\n",
				outfile, cwd, (aflag)? "updating" : "leaving", TMPDIR, outfile
			);
			outf = fopen(outfile, aflag ? "a" : "w");
			if( !outf ){
				fprintf( stderr, "%s/", TMPDIR );
				perror( outfile );
				exit(1);
			}
		}
	}
	for( i= 1; i< ac; i++){
	  char *name;
		if( (name= is_tags( av[i])) ){
			fprintf( stderr, "Including \"%s\" ", name );
			fflush( stderr );
			copied+= copy_tags( name );
			fputs( "\n", stderr );
		}
	}
	if( uflag)
		fprintf( stderr, "Tagfile %s: %ld lines changed", outfile, copied+ put_entries(head) );
	else
		fprintf( stderr, "Tagfile %s: %ld lines\n", outfile, copied+ put_entries(head) );
	fclose(outf);
	if (uflag || copied ) {
#ifdef MCH_AMIGA
		fputs( ".. Sorting.. ", stderr);
		fflush( stderr);
		unlink( "OTAGS");
		lrename( outfile, "OTAGS");
		sprintf( cmd, "sort from OTAGS to %s", outfile);
		Execute( cmd, NULL, Output());
		unlink( "OTAGS");
		fputs( "ready\n", stderr);
#else
		fputs( ".. sorting in progress", stderr); fflush( stderr);
#ifdef _HPUX_SOURCE
		sprintf(cmd, "(sort +0 -1 -u -o %s %s ; echo \"tags finished\") &",
			outfile, outfile
		);
#else
		{ char *tnam= tempnam(NULL, "STAGS");
			unlink(tnam);
			lrename(outfile, tnam);
			sprintf(cmd, "(sort +0 -1 -u %s -o %s ; echo \"tags finished\") &",
				tnam, outfile
			);
			system(cmd);
			cmd[0]= '\0';
			unlink(tnam);
		}
#endif
		if( cmd[0] ){
			system(cmd);
		}
		fputc( '\n', stderr);
#endif
	}
	exit(0);
}

/*
 * This routine sets up the boolean psuedo-functions which work
 * by seting boolean flags dependent upon the corresponding character
 * Every char which is NOT in that string is not a white char.  Therefore,
 * all of the array "_wht" is set to FALSE, and then the elements
 * subscripted by the chars in "white" are set to TRUE.  Thus "_wht"
 * of a char is TRUE if it is the string "white", else FALSE.
 */
init()
{

	reg	char	*sp;
	reg	int	i;

	for (i = 0; i < 0177; i++) {
		_wht[i] = _etk[i] = _itk[i] = _btk[i] = FALSE;
		_gd[i] = TRUE;
	}
	for (sp = white; *sp; sp++)
		_wht[(unsigned int) *sp] = TRUE;
	for (sp = endtk; *sp; sp++)
		_etk[(unsigned int) *sp] = TRUE;
	for (sp = intk; *sp; sp++)
		_itk[(unsigned int) *sp] = TRUE;
	for (sp = begtk; *sp; sp++)
		_btk[(unsigned int) *sp] = TRUE;
	for (sp = notgd; *sp; sp++)
		_gd[(unsigned int) *sp] = FALSE;
}

/*
 * This routine opens the specified file and calls the function
 * which finds the function and type definitions.
 */
#	ifdef __STDC__
find_entries( char *file)
#	else
find_entries(file)
  char	*file;
#	endif
{
	char *cp;

	if ((inf = fopen(file,"r")) == NULL) {
		perror( "can't open");
		return;
	}
	curfile = savestr(file);
	lineno = 0;
	cp = rindex(file, '.');
	if( strcasecmp(lang, "lisp")== 0 || (!lang[0] && cp && (strcmp( &cp[1], "lisp")== 0 || strcmp( &cp[1], "lsp")== 0 ||
			strcmp( &cp[1], "LISP")== 0 || strcmp( &cp[1], "LSP")== 0))
	){
		L_funcs(inf);					/* lisp	*/
		fclose(inf);
		return;
	}
	/* .l implies lisp or lex source code */
	if( strcasecmp(lang,"lex")== 0 || (!lang[0] && cp && (cp[1] == 'l' || cp[1] == 'L') && cp[2] == '\0') ){
		if( !lang[0] && index(";([", first_char()) != NULL) {	/* lisp */
			L_funcs(inf);
			fclose(inf);
			return;
		}
		else {						/* lex */
			/*
			 * throw away all the code before the second "%%"
			 */
			toss_yysec();
			getline();
			pfnote("yylex", lineno, TRUE);
			toss_yysec();
			C_entries();
			fclose(inf);
			return;
		}
	}
	/* .y implies a yacc file */
	if( strcasecmp(lang,"yacc")==0 || (!lang[0] && cp && (cp[1] == 'y' || cp[1] == 'Y') && cp[2] == '\0') ){
		toss_yysec();
		Y_entries();
		C_entries();
		fclose(inf);
		return;
	}
	/* if not a .c or .h file, try fortran */
	if( strcasecmp(lang,"fortran")==0 || (!lang[0] && cp && (cp[1] != 'c' && cp[1]!= 'C' && cp[1] != 'h' && cp[1]!= 'H') && cp[2] == '\0') ){
		if (PF_funcs(inf) != 0) {
			fclose(inf);
			return;
		}
		else if( lang[0] ){
			return;
		}
		rewind(inf);	/* no fortran tags found, try C */
		lineno= 0;
	}
	C_entries();
	fclose(inf);
}

pfnote(name, ln, f)
char	*name;
int	ln;
bool	f;		/* f == TRUE when function */
{
	register char *fp;
	register NODE *np;
	char nbuf[BUFSIZ];

	if ((np = (NODE *) malloc(sizeof (NODE))) == NULL) {
		fprintf(stderr, "ctags: too many entries to sort\n");
		fprintf( stderr, "Tagfile %s: %ld lines\n", outfile, put_entries(head) );
		free_tree(head);
		head = np = (NODE *) malloc(sizeof (NODE));
	}
	if (xflag == 0 && !strcmp(name, "main")) {
		fp = rindex(curfile, '/');
#ifdef _MAC_MPWC_
		fp= rindex( curfile, ':');
#endif
#ifdef MCH_AMIGA
		if( !fp)
			fp= rindex( curfile, ':');
#endif MCH_AMIGA
		if (fp == 0)
			fp = curfile;
		else
			fp++;
		sprintf(nbuf, "M%s", fp);
		fp = rindex(nbuf, '.');
		if (fp && fp[2] == 0)
			*fp = 0;
		name = nbuf;
	}
	np->entry = savestr(name);
	np->file = curfile;
	np->f = f;
	np->lno = ln;
	np->left = np->right = 0;
	if (xflag == 0) {
		lbuf[50] = 0;
#ifdef _MAC_MPWC_
		strcat( lbuf, "\xb0");
#else
		strcat(lbuf, "$");
#endif _MAC_MPWC_
		lbuf[50] = 0;
	}
	np->pat = savenstr(lbuf, PATSIZE);
	if (head == NULL)
		head = np;
	else
		add_node(np, head);
}

bool all_white;

/*
 * This routine finds functions and typedefs in C syntax and adds them
 * to the list.
 */
C_entries()
{
	register int c;
	register char *token, *tp;
	bool incomm, inquote, inchar, midtoken;
	int level;
	char *sp;
	char tok[BUFSIZ];
	bool allow_c_entries;
	bool ifdef_STDC, ifndef_STDC;
	int STDC_level;

	number = gotone = midtoken = inquote = inchar = incomm = FALSE;
	all_white= 1;
	level = 0;

	allow_c_entries= 1;
	STDC_level= 0;
	ifdef_STDC= ifndef_STDC= 0;

	sp = tp = token = line;
	lineno++;
	lineftell = ftell(inf);
	for (;;) {
		*sp = c = getc(inf);
		if (feof(inf))
			break;
		if( all_white){
			if( !iswhite(c)  && c!= '#' )
				all_white= 0;
		}
		if (c == '\n'){
			currentlineftell = ftell(inf);
			all_white= 1;
			lineno++;
		}
		else if (c == '\\') {
			c = *++sp = getc(inf);
			if (c == '\n'){
				currentlineftell = ftell(inf);
				all_white= 1;
				lineno++;
				c = ' ';
			}
		}
		else if (incomm) {
			if (c == '*') {
				while ((*++sp=c=getc(inf)) == '*')
					continue;
				if (c == '\n'){
					currentlineftell = ftell(inf);
					all_white= 1;
					lineno++;
				}
				if (c == '/')
					incomm = FALSE;
			}
		}
		else if (inquote) {
			/*
			 * Too dumb to know about \" not being magic, but
			 * they usually occur in pairs anyway.
			 */
			if (c == '"')
				inquote = FALSE;
			continue;
		}
		else if (inchar) {
			if (c == '\'')
				inchar = FALSE;
			continue;
		}
		else switch (c) {
		  case '"':
			if( macdef!= begin)
				inquote = TRUE;
			continue;
		  case '\'':
			inchar = TRUE;
			continue;
		  case '/':
			if ((*++sp=c=getc(inf)) == '*')
				incomm = TRUE;
			else
				ungetc(*sp, inf);
			continue;
		  case '#':{
		    char *d;
			if (sp == line || all_white== 1){
				number = TRUE;
				getcurrentline();
				d= &lbuf[1];
				while( iswhite((unsigned int) *d) )
					d++;
				if( !strncmp( d, "endif", 4) && STDC_level){
					if( ! --STDC_level){
						if( !wflag){
							if( ifdef_STDC)
								fprintf( stderr, "\t%d End __STDC__ code\n", lineno);
							if( ifndef_STDC)
								fprintf( stderr, "\t%d End NOT __STDC__ code\n", lineno);
							fflush( stderr);
						}
						ifdef_STDC= ifndef_STDC= 0;
						c= '\n';
					}
				}
				else if( !strncmp( d, "else", 4) && STDC_level== 1){
					if( ifdef_STDC){
						ifdef_STDC= 0;
						ifndef_STDC= 1;
						c= '\n';
					}
					else if( ifndef_STDC){
						ifdef_STDC= 1;
						ifndef_STDC= 0;
						c= '\n';
					}
				}
				else if( !strncmp( d, "ifdef", 5) ){
					d+= 5;
					while( iswhite((unsigned int) *d) )
						d++;
					if( !strncmp( d, "__STDC__", 8) && !STDC_level){
						ifdef_STDC= 1;
						ifndef_STDC= 0;
						STDC_level++;
						c= '\n';
					}
					else if( STDC_level)
						STDC_level++;
				}
				else if( !strncmp( d, "ifndef", 6) ){
					d+= 6;
					while( iswhite((unsigned int) *d) )
						d++;
					if( !strncmp( d, "__STDC__", 8) && !STDC_level){
						ifdef_STDC= 0;
						ifndef_STDC= 1;
						STDC_level++;
						c= '\n';
					}
					else if( STDC_level)
						STDC_level++;
				}
				else if( !strncmp( d, "define", 6) ){
					d+= 6;
					while( iswhite((unsigned int) *d) )
						d++;
					if( !strncmp( d, "__STDC__", 8) ){
						if( !wflag)
							fputs( "\t__STDC__ defined\n", stderr);
						STDC_defined= 1;
						c= '\n';
					}
				}
				else if( !strncmp( d, "undef", 5) ){
					d+= 5;
					while( iswhite((unsigned int) *d) )
						d++;
					if( !strncmp( d, "__STDC__", 8) ){
						if( !wflag)
							fputs( "\t__STDC__ undefined\n", stderr);
						STDC_defined= 0;
						c= '\n';
					}
				}
				if( STDC_level== 1){
					if( STDC_defined){
						allow_c_entries= ( ifdef_STDC && !ifndef_STDC)? 1 : 0;
						if( !wflag){
							if( !allow_c_entries)
								fprintf( stderr, "\t%d Skipping NOT __STDC__ code\n", lineno);
							else
								fprintf( stderr, "\t%d Scanning __STDC__ code\n", lineno);
							fflush( stderr);
						}
					}
					else{
						allow_c_entries= ( !ifdef_STDC && ifndef_STDC)? 1 : 0;
						if( !wflag){
							if( !allow_c_entries)
								fprintf( stderr, "\t%d Skipping __STDC__ code\n", lineno);
							else
								fprintf( stderr, "\t%d Scanning NOT __STDC__ code\n", lineno);
							fflush( stderr);
						}
					}
				}
				else if( !STDC_level)
					allow_c_entries= 1;

				if( c== '\n'){
				/* we found some __STDC__ reference - skip the rest of the line	*/
					do{
						*(++sp) = c= getc(inf);
					} while( c!= '\n' && !feof(inf) );
					if( c== '\n'){
					/* this will make us fall to the part of this function that
					 * handles the end of the line
					 */
						ungetc(*sp, inf);
						c= *(--sp);
						break;
					}
				}
				/* False alarm (?) - proceed to next character	*/
			}
			continue;
		  }
		  case '{':
			if (tydef == begin) {
				tydef=middle;
				if( tflag== 2 && macdef== begin)
					macdef= none;
			}
			level++;
			continue;
		  case '}':
			if (sp == line)
				level = 0;	/* reset */
			else
				level--;
			if (!level && tydef==middle) {
				tydef=end;
			}
			continue;
		}
		if (!level && !inquote && !incomm && gotone == FALSE) {
			if (midtoken) {
				if (endtoken(c)) {
				int f;
				int pfline = lineno;
					if( macdef== end)
						pfline= macline;
					if (start_entry(&sp,token,tp,&f)) {
						strncpy(tok,token,tp-token+1);
						tok[tp-token+1] = 0;
						getline();
						pfnote(tok, pfline, f);
						gotone = f;	/* function */
					}
					midtoken = FALSE;
					token = sp;
				}
				else if (intoken(c))
					tp++;
			}
			else if( begtoken(c)) {
				if( allow_c_entries){
					token = tp = sp;
					midtoken = TRUE;
				}
			}
		}
		if (c == ';'  &&  tydef==end)	/* clean with typedefs */
			tydef=none;
		sp++;
		if( macdef!= none && (c== '\n' || lineno> macline) ){
			if( macdef== end){
				tp = token = sp = line;
				lineftell = ftell(inf);
				number = gotone = midtoken = inquote = inchar = FALSE;
				all_white= 1;
			}
			macdef= none;
		}
		if( c == '\n' || sp > &line[sizeof (line) - BUFSIZ]) {
			tp = token = sp = line;
			lineftell = ftell(inf);
			number = gotone = midtoken = inquote = inchar = FALSE;
			all_white= 1;
		}
	}
	fprintf( stderr, "C code ");
}

/*
 * This routine  checks to see if the current token is
 * at the start of a function, or corresponds to a typedef
 * It updates the input line * so that the '(' will be
 * in it when it returns.
 */
start_entry(lp,token,tp,f)
char	**lp,*token,*tp;
int	*f;
{
	reg	char	c,*sp, end_char;
	static	bool	found;
	bool	firsttok;		/* T if have seen first token in ()'s */
	int	bad;

	*f = 1;			/* a function */
	sp = *lp;
	c = *sp;
	bad = FALSE;
	if (!number) {		/* space is not allowed in macro defs	*/
		while (iswhite((unsigned int) c)) {
			*++sp = c = getc(inf);
			if (c == '\n') {
				currentlineftell= ftell(inf);
				all_white= 1;
				lineno++;
				if (sp > &line[sizeof (line) - BUFSIZ])
					goto ret;
			}
		}
	/* the following tries to make it so that a #define a b(c)	*/
	/* doesn't count as a define of b.				*/
	}
	else {
		if (!strncmp(token, "define", 6)){
			found = 0;
			if( mflag){
				macdef= begin;
				macline= lineno;
				*f= 0;
				goto badone;
			}
		}
		else
			found++;
		if (found >= 2) {
			gotone = TRUE;
badone:			bad = TRUE;
			goto ret;
		}
	}
	if( mflag && !strncmp( token, "define", 6) ){
		macdef= begin;
		macline= lineno;
		*f= 0;
		goto badone;
	}
	/* check for the typedef cases		*/
	if( tflag && !strncmp(token, "typedef", 7) ){
		if( tflag== 2){
		/* treat a typedef as a special case of
		 * a macro statement (allowing for 
		 * #typedef foo bar;
		 * statements (otherwise, the algorithm searches
		 * for a {} combination
		 */
			macline= lineno;
			macdef= begin;
			*f= 0;
		}
		tydef=begin;
		goto badone;
	}
	if(!strncmp(token, "struct", 6) || !strncmp(token, "union", 5) || !strncmp(token, "enum", 4)){
		if( tydef== none ){
		  /* a "hidden" struct/union/enum definition	*/
			tydef= begin;
		}
		goto badone;
	}
	if (tydef==begin) {
	  static char *t, *Token;
		t= token;
		Token= token;
		*f= 0;
		while( *t && !iswhite((unsigned int)  *t) && *t!= '{' )
			t++;
		while( *t && iswhite((unsigned int) *t) && *t!= '{')
			t++;
		if( *t== '{'){
			tydef= middle;
				macdef= none;
			goto ret;
		}
		else{
		  /* no '{': an instance declaration	*/
			tydef= end;
		}
/* 		if( macdef!= begin)	*/
			goto ret;
	}
	else if( tydef==end) {
		tydef= none;
		*f = 0;
		goto ret;
	}
	end_char= ')';
	if( c != '('){
		if( macdef== none)
		/* if we want #define's to be included	*/
			goto badone;
		if( macdef== begin ){
			macdef= end;
			end_char= '\n';
			*f= 0;
		}
	}
	else{
		if( mflag && macdef== begin && tydef== none ){
			macdef= end;
			*f= 1;
		}
	}
	firsttok = FALSE;
	*++sp=c=getc(inf);
	if( macdef!= end || end_char== ')' ){
		while( ((int)c)!= EOF && c!= end_char){
			/*
			 * This line used to confuse ctags:
			 *	int	(*oldhup)();
			 * This fixes it. A nonwhite char before the first
			 * token, other than a / (in case of a comment in there)
			 * makes this not a declaration.
			 */
			if (c == '\n') {
				currentlineftell= ftell(inf);
				all_white= 1;
				lineno++;
				if (sp > &line[sizeof (line) - BUFSIZ])
					goto ret;
			}
			if (begtoken((unsigned int)c) || c=='/')
				firsttok++;
			else if (!iswhite((unsigned int)c) && !firsttok)
				goto badone;
			*++sp=c=getc(inf);
		}
		macdef= none;
	}
	else{
		while( ((int)c)!= EOF && c!= end_char /* !iswhite(c) */ ){
			if (sp > &line[sizeof (line) - BUFSIZ])
				goto ret;
			/* no check necessary for newline	*/
			if (begtoken((unsigned int)c) || c=='/')
				firsttok++;
			else if (!iswhite((unsigned int)c) && !firsttok)
				goto badone;
			*++sp=c=getc(inf);
		}
/* 		if ( c == '\n') {	*/
			currentlineftell= ftell(inf);
			all_white= 1;
			lineno++;
			if (sp > &line[sizeof (line) - BUFSIZ])
				goto ret;
/* 		}	*/
	}
	while (iswhite((unsigned int)(*++sp=c=getc(inf))))
		if (c == '\n') {
			currentlineftell= ftell(inf);
			all_white= 1;
			lineno++;
			if (sp > &line[sizeof (line) - BUFSIZ])
				break;
		}
ret:
	*lp = --sp;
	if (c == '\n'){
		ungetc(c,inf);
		currentlineftell= ftell(inf);
		all_white= 0;
		lineno--;
	}
	else
		ungetc(c,inf);
	return !bad && (!*f || isgood((unsigned int)c));
					/* hack for typedefs */
}

/*
 * Y_entries:
 *	Find the yacc tags and put them in.
 */
Y_entries()
{
	register char	*sp, *orig_sp;
	register int	brace;
	register bool	in_rule, toklen;
	char		tok[BUFSIZ];

	brace = 0;
	getline();
	pfnote("yyparse", lineno, TRUE);
	while (fgets(line, sizeof line, inf) != NULL)
		for (sp = line; *sp; sp++)
			switch (*sp) {
			  case '\n':
				lineno++;
				/* FALLTHROUGH */
			  case ' ':
			  case '\t':
			  case '\f':
			  case '\r':
				break;
			  case '"':
				do {
					while (*++sp != '"')
						continue;
				} while (sp[-1] == '\\');
				break;
			  case '\'':
				do {
					while (*++sp != '\'')
						continue;
				} while (sp[-1] == '\\');
				break;
			  case '/':
				if (*++sp == '*')
					sp = toss_comment(sp);
				else
					--sp;
				break;
			  case '{':
				brace++;
				break;
			  case '}':
				brace--;
				break;
			  case '%':
				if (sp[1] == '%' && sp == line)
					return;
				break;
			  case '|':
			  case ';':
				in_rule = FALSE;
				break;
			  default:
				if (brace == 0  && !in_rule && (isalpha(*sp) ||
								*sp == '.' ||
								*sp == '_')) {
					orig_sp = sp;
					++sp;
					while (isalnum(*sp) || *sp == '_' ||
					       *sp == '.')
						sp++;
					toklen = sp - orig_sp;
					while (isspace(*sp))
						sp++;
					if (*sp == ':' || (*sp == '\0' &&
							   first_char() == ':'))
					{
						strncpy(tok, orig_sp, toklen);
						tok[(unsigned int)toklen] = '\0';
						strcpy(lbuf, line);
						lbuf[strlen(lbuf) - 1] = '\0';
						pfnote(tok, lineno, TRUE);
						in_rule = TRUE;
					}
					else
						sp--;
				}
				break;
			}
	fprintf( stderr, "Yacc code ");
}

char *
toss_comment(start)
char	*start;
{
	register char	*sp;

	/*
	 * first, see if the end-of-comment is on the same line
	 */
	do {
		while ((sp = index(start, '*')) != NULL)
			if (sp[1] == '/')
				return ++sp;
			else
				start = ++sp;
		start = line;
		lineno++;
	} while (fgets(line, sizeof line, inf) != NULL);
}

getcurrentline()
{
	long saveftell = ftell( inf );
	register char *cp;

	fseek( inf , currentlineftell , 0 );
	fgets(lbuf, sizeof lbuf, inf);
	cp = rindex(lbuf, '\n');
	if (cp)
		*cp = 0;
	fseek(inf, saveftell, 0);
}

getline()
{
	long saveftell = ftell( inf );
	register char *cp;

	fseek( inf , lineftell , 0 );
	fgets(lbuf, sizeof lbuf, inf);
	cp = rindex(lbuf, '\n');
	if (cp)
		*cp = 0;
	fseek(inf, saveftell, 0);
}

free_tree(node)
NODE	*node;
{

	while (node) {
		free_tree(node->right);
/* 		cfree(node);	*/
		free(node);
		node = node->left;
	}
}

add_node(node, cur_node)
	NODE *node,*cur_node;
{
	register int dif;

	dif = strcmp(node->entry, cur_node->entry);
	if (dif == 0) {
		if (node->file == cur_node->file) {
			if (!wflag) {
fprintf(stderr,"Duplicate entry in file %s, line %d: %s\n",
    node->file,lineno,node->entry);
fprintf(stderr,"Second entry ignored\n");
			}
			return;
		}
		if (!cur_node->been_warned)
			if (!wflag)
fprintf(stderr,"Duplicate entry in files %s and %s: %s (Warning only)\n",
    node->file, cur_node->file, node->entry);
		cur_node->been_warned = TRUE;
		return;
	}

	if (dif < 0) {
		if (cur_node->left != NULL)
			add_node(node,cur_node->left);
		else
			cur_node->left = node;
		return;
	}
	if (cur_node->right != NULL)
		add_node(node,cur_node->right);
	else
		cur_node->right = node;
}

long put_entries(node)
reg NODE	*node;
{
	reg char	*sp;
	static long lines= 0L;

	if (node == NULL)
		return( lines);
	put_entries(node->left);
#ifdef _MAC_MPWC_
	if (xflag == 0){
		if (node->f) {		/* a function */
			fprintf(outf, "File \x22%s\x22 ; Find %s \x22%s\x22 ; Find %c\xa5",
				node->file, search_start, node->file, searchar
			);
			for (sp = node->pat; *sp; sp++){
				/* check for special characters; quote them
				 * to prevent interference with the selection
				 * expression search algorithm
				 */
				if( index( "()[]\\#*/{}+", *sp)){
					fputc( 0xb6, outf);
					fputc( *sp, outf);
				}
				else
					fputc( *sp, outf);
			}
			fprintf(outf, "%c \x22%s\x22\t; open \x22%s\x22\t# %s\n",
				searchar, node->file, node->file, node->entry
			);
		}
		else
			fprintf( outf, "File %s\t;\tLine %d\t# %s\n",
				node->file, node->lno, node->entry
			);
	}
	else{
		if (vflag)
			fprintf(stdout, "%s %s %d\n",
				node->entry, node->file, (node->lno+63)/64
			);
		else
			fprintf(stdout, "%-16s%4d %-16s %s\n",
				node->entry, node->lno, node->file, node->pat
			);
	}
#endif _MAC_MPWC_
#ifdef MCH_AMIGA
/* ifdef MCH_AMIGA (Manx), postulate use of Manx 'z' (== vi) editor.
 * Therefore generate a 'z' useable tagsfile:
 * 	* no "end-of-search" character
 *	* no TABS as seperators
 */
	if (xflag == 0)
		if (node->f) {		/* a function */
			fprintf(outf, "%s %s %c^",
				node->entry, node->file, searchar);
			for (sp = node->pat; *sp; sp++)
				if( index( "[]\\*-.^", *sp)){
					fputc( '\\', outf);
					fputc( *sp, outf);
				}
				else
					fputc( *sp, outf);
			fputc( '\n', outf);
		}
		else {		/* a typedef; text pattern inadequate */
			fprintf(outf, "%s %s %dg\n",
				node->entry, node->file, node->lno);
		}
	else if (vflag)
		fprintf(stdout, "%s %s %d\n",
				node->entry, node->file, (node->lno+63)/64);
	else
		fprintf(stdout, "%-16s%4d %-16s %s\n",
			node->entry, node->lno, node->file, node->pat);
#endif MCH_AMIGA
#ifdef OTHER_MACHINES
	if (xflag == 0)
		if( node->pat && strlen(node->pat)> 2 ) {
			fprintf(outf, "%s\t%s\t%c^",
				node->entry, node->file, searchar);
			for (sp = node->pat; *sp; sp++){
/* 				if( index( "[]\\/?.^", *sp)){	*/
				if( index( "\\/?.^", *sp)){
					fputc( '\\', outf);
					fputc( *sp, outf);
				}
				else
					fputc( *sp, outf);
			}
			fprintf(outf, "%c\n", searchar);
		}
		else {		/* text pattern inadequate */
			fprintf(outf, "%s\t%s\t%d\n",
				node->entry, node->file, node->lno);
		}
	else if (vflag)
		fprintf(stdout, "%s %s %d\n",
				node->entry, node->file, (node->lno+63)/64);
	else{
		fprintf(stdout, "%s\t%4d\t%s\t%s\n",
			node->entry, node->lno, node->file, node->pat);
	}
#endif OTHER_MACHINES
	lines++;
	put_entries(node->right);
	return( lines);
}


char	*dbp = lbuf;
int	pfcnt;

PF_funcs(fi)
	FILE *fi;
{

	pfcnt = 0;
	while (fgets(lbuf, sizeof(lbuf), fi)) {
		lineno++;
		dbp = lbuf;
		if ( *dbp == '%' ) dbp++ ;	/* Ratfor escape to fortran */
		while (isspace(*dbp))
			dbp++;
		if (*dbp == 0)
			continue;
		switch (*dbp |' ') {

		  case 'i':
			if (tail("integer"))
				takeprec();
			break;
		  case 'r':
			if (tail("real"))
				takeprec();
			break;
		  case 'l':
			if (tail("logical"))
				takeprec();
			break;
		  case 'c':
			if (tail("complex") || tail("character"))
				takeprec();
			break;
		  case 'd':
			if (tail("double")) {
				while (isspace(*dbp))
					dbp++;
				if (*dbp == 0)
					continue;
				if (tail("precision"))
					break;
				continue;
			}
			break;
		}
		while (isspace(*dbp))
			dbp++;
		if (*dbp == 0)
			continue;
		switch (*dbp|' ') {

		  case 'f':
			if (tail("function"))
				getit();
			continue;
		  case 's':
			if (tail("subroutine"))
				getit();
			continue;
		  case 'p':
			if (tail("program")) {
				getit();
				continue;
			}
			if (tail("procedure"))
				getit();
			continue;
		}
	}
	return (pfcnt);
}

tail(cp)
	char *cp;
{
	register int len = 0;

	while (*cp && (*cp&~' ') == ((*(dbp+len))&~' '))
		cp++, len++;
	if (*cp == 0) {
		dbp += len;
		return (1);
	}
	return (0);
}

takeprec()
{

	while (isspace(*dbp))
		dbp++;
	if (*dbp != '*')
		return;
	dbp++;
	while (isspace(*dbp))
		dbp++;
	if (!isdigit(*dbp)) {
		--dbp;		/* force failure */
		return;
	}
	do
		dbp++;
	while (isdigit(*dbp));
}

getit()
{
	register char *cp;
	char c;
	char nambuf[BUFSIZ];

	for (cp = lbuf; *cp; cp++)
		;
	*--cp = 0;	/* zap newline */
	while (isspace(*dbp))
		dbp++;
	if (*dbp == 0 || !isalpha(*dbp))
		return;
	for (cp = dbp+1; *cp && (isalpha(*cp) || isdigit(*cp)); cp++)
		continue;
	c = cp[0];
	cp[0] = 0;
	strcpy(nambuf, dbp);
	cp[0] = c;
	pfnote(nambuf, lineno, TRUE);
	pfcnt++;
}

char *
savestr(cp)
	char *cp;
{
	register int len;
	register char *dp;

	len = strlen(cp);
	dp = (char *)malloc(len+1);
	strcpy(dp, cp);
	return (dp);
}

/* save the first n bytes in cp	*/
char *
savenstr(cp, len)
	char *cp;
	register int len;
{
	register int len2= strlen(cp);
	register char *dp;

	if( len2< len)
		len = len2;
	dp = (char *)malloc(len+1);
	strncpy(dp, cp, len);
	dp[len]= '\0';
	return (dp);
}

#if !defined(MCH_AMIGA) && !defined(__STDC__)
/*
 * Return the ptr in sp at which the character c last
 * appears; NULL if not found
 *
 * Identical to v7 rindex, included for portability.
 */

char *
rindex(register char * sp, register char c)
{
	register char *r;

	r = NULL;
	do {
		if (*sp == c)
			r = sp;
	} while (*sp++);
	return(r);
}
#endif MCH_AMIGA

/*
 * lisp tag functions
 * just look for (def or (DEF
 */

L_funcs (fi)
FILE *fi;
{
	register int	special;

	pfcnt = 0;
	while (fgets(lbuf, sizeof(lbuf), fi)) {
		lineno++;
		dbp = lbuf;
		if (dbp[0] == '(' &&
		    (dbp[1] == 'D' || dbp[1] == 'd') &&
		    (dbp[2] == 'E' || dbp[2] == 'e') &&
		    (dbp[3] == 'F' || dbp[3] == 'f')) {
			dbp += 4;
			if (striccmp(dbp, "method") == 0 ||
			    striccmp(dbp, "wrapper") == 0 ||
			    striccmp(dbp, "whopper") == 0)
				special = TRUE;
			else
				special = FALSE;
			while (!isspace(*dbp))
				dbp++;
			while (isspace(*dbp))
				dbp++;
			L_getit(special);
		}
	}
	fprintf( stderr, "Lisp code ");
}

L_getit(special)
int	special;
{
	register char	*cp;
	register char	c;
	char		nambuf[BUFSIZ];

	for (cp = lbuf; *cp; cp++)
		continue;
	*--cp = 0;		/* zap newline */
	if (*dbp == 0)
		return;
	if (special) {
		if ((cp = index(dbp, ')')) == NULL)
			return;
		while (cp >= dbp && *cp != ':')
			cp--;
		if (cp < dbp)
			return;
		dbp = cp;
		while (*cp && *cp != ')' && *cp != ' ')
			cp++;
	}
	else
		for (cp = dbp + 1; *cp && *cp != '(' && *cp != ' '; cp++)
			continue;
	c = cp[0];
	cp[0] = 0;
	strcpy(nambuf, dbp);
	cp[0] = c;
	pfnote(nambuf, lineno,TRUE);
	pfcnt++;
}

/*
 * striccmp:
 *	Compare two strings over the length of the second, ignoring
 *	case distinctions.  If they are the same, return 0.  If they
 *	are different, return the difference of the first two different
 *	characters.  It is assumed that the pattern (second string) is
 *	completely lower case.
 */
striccmp(str, pat)
register char	*str, *pat;
{
	register int	c1;

	while (*pat) {
		if (isupper(*str))
			c1 = tolower(*str);
		else
			c1 = *str;
		if (c1 != *pat)
			return c1 - *pat;
		pat++;
		str++;
	}
	return 0;
}

/*
 * first_char:
 *	Return the first non-blank character in the file.  After
 *	finding it, rewind the input file so we start at the beginning
 *	again.
 */
first_char()
{
	register int	c;
	register long	off;

	off = ftell(inf);
	while ((c = getc(inf)) != EOF)
		if (!isspace(c) && c != '\r') {
			fseek(inf, off, 0);
			return c;
		}
	fseek(inf, off, 0);
	return EOF;
}

/*
 * toss_yysec:
 *	Toss away code until the next "%%" line.
 */
toss_yysec()
{
	char		buf[BUFSIZ];

	for (;;) {
		lineftell = ftell(inf);
		if (fgets(buf, BUFSIZ, inf) == NULL)
			return;
		lineno++;
		if (strncmp(buf, "%%", 2) == 0)
			return;
	}
}
