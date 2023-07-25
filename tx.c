/* 
vi:set ts=4:
vi:set sw=4:
*/
/* tx <tss> : replace all TABS in standard input by that many spaces
 * that remain until the next tabstop of size tss, copying the result
 * to the standard output.
 * Use:   more <file(s)> | tx <tss> | lpr
 * will print <file(s)> to the lineprinter with a header (if multiple
 * files) for each file, and the tabs expanded to spaces. This ensures
 * that the printout will always have the same layout as the text has
 * in the editor ( if <tss> equals the tabstop size used by your editor!).

 * (C) (R) R. J. Bertin 29/11/1989  **  ICR
           R.J.V. Bertin (the same :)), 1990-2002

 * 11/4/1990: added support for ':ts=n' command: unlike in vi this works
 * between directly after it and the next occurence, or EOF. So a 
 * ':ts=10' in the M(iddle)OF does not override previous TABSTOP settings
 * (i.e. upto the :ts= pattern).
 * RJB.

 :ts=4	this file with TABSTOP 4.
 */

#define _XOPEN_SOURCE

#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "local/Macros.h"
#include <pwd.h>
#include <grp.h>

#include <time.h>
#include <sys/times.h>

IDENTIFY("simple tab-expansion/line-wrapping filter");

// this hasn't been required since a looong time:
// char *calloc();

#ifndef serror
//#	define serror() ((errno>0&&errno<sys_nerr)?sys_errlist[errno]:"invalid errno")
#	define serror()	strerror(errno)
#elif linux
#	undef serror
#	define serror()	strerror(errno)
#endif

/* Put here the printing command on your system. Either lp or lpr should do, but you may have
 \ yet another one. I know of no way to determine this runtime, other than bluntly checking,
 \ which won't tell what an existing executable 'lp' or 'lpr' actually does....
 */
#define PRINTCOMMAND	"lpr"
char *prcom= PRINTCOMMAND;

help(char **argv, char *argerr)
{
	fprintf( stderr, "TAB->SPACES expansion, CR insert, linewrap, formfeed filter\n");
	fprintf( stderr, "%s: unknown option \"%s\"\n", argv[0], argerr );
	fprintf( stderr,
		"Usage: %s [-<tabstop>]] [-cr] [-qp] [-pr] [-v] [-fi] [-nf<PAT> [-[nf|NF]<PAT>]] [-fl] [-w<COLS>|-W<COLS>] [-f<FORMFEEDS>] [infile] [outfile]\n",
		argv[0]
	);
	fprintf( stderr, "\t-<tabstop>:\tspecifies tabstop. Overriden by a :ts=<tabstop> line in file\n");
	fprintf( stderr, "\t-cr:\tinsert a carriage return (\\r) before every linebreak\n");
	fprintf( stderr, "\t-fi:\tprint a headerline showing file information\n");
	fprintf( stderr, "\t-fl:\tfold lines, ignoring the linebreaks in the file\n");
	fprintf( stderr, "\t-nf<PAT>:\tdon't fold (onto) lines starting with pattern <PAT> (multiple patterns possible)\n");
	fprintf( stderr, "\t        :\tlines following one ending with a ^A (0x01) are not folded neither\n");
	fprintf( stderr, "\t-NF<PAT>:\tidem, but ignore leading whitespace.\n");
	fprintf( stderr, "\t-qp:\tdecode MIME QUOTED-PRINTABLE codes\n");
	fprintf( stderr, "\t-qp2:\tidem, but requires compatible Content-Type and Content-Transfer-Encoding header lines\n");
	fprintf( stderr, "\t-pr[command]:\tsend to printer, optionally using <command> as the printing command, else \"%s\"\n", prcom );
	fprintf( stderr, "\t-w<COLS>:\twrap lines to length <COLS> [also by invoking the program as 'txw<COLS>']\n");
	fprintf( stderr, "\t-W<COLS>:\tidem, indent wrapped line by <tabstop>/4\n");
	fprintf( stderr, "\t-ws:\tonly wrap in whitespace (otherwise also after some punctuation characters)\n");
	exit(10);
}

/* 
char nextchar( FILE *fp, char *ss )
{
	if( ss[1]!= '\0' ){
		return( ss[1] );
	}
	else{
	  char c= getc(fp);
		ungetc( c, fp );
		return(c);
	}
}
*/

typedef struct Pattern{
	char *pat, afterpat;
	int ignore_leading_white;
	int len, hits;
	struct Pattern *next;
} Pattern;

Pattern *add_pattern( Pattern *list, char *pat, int ignore_leading_white )
{  Pattern *new= (Pattern*) calloc( sizeof(Pattern), 1);
	if( new ){
		new->pat= pat;
		new->len= strlen(pat);
		new->hits= 0;
		new->next= list;
		new->ignore_leading_white= ignore_leading_white;
		return( new );
	}
	else{
		fprintf( stderr, "tx: can't get memory for pattern \"%s\" (%s)\n",
			pat, serror()
		);
	}
	return( list );
}

Pattern *find_pattern( Pattern *list, char *pat )
{ char *p;
	if( !pat ){
		return(NULL);
	}
	while( list ){
		p= pat;
		if( list->ignore_leading_white ){
			while( *p && isspace((unsigned char) *p) ){
				p++;
			}
		}
		if( !strncmp( p, list->pat, list->len ) ){
			list->hits+= 1;
			if( p[list->len] && isspace((unsigned char)p[list->len]) ){
				list->afterpat= p[list->len];
			}
			else{
				list->afterpat= '\0';
			}
			return( list );
		}
		list= list->next;
	}
	return(NULL);
}

#ifndef REGISTER
#	define REGISTER /* */
#endif

extern char *rindex();

#include <signal.h>

char buff[513], buff2[513], buff0[513], last_char[2]= {'\0', 'a'};

void sig_handler(int  sig)
{
	switch( sig ){
		case SIGINT:
		case SIGHUP:
		case SIGUSR1:
			exit(0);
			break;
		case SIGKILL:
		case SIGUSR2:
		{ extern char *getenv();
		  char *home= getenv("HOME");
			if( home ){
				chdir( home );
			}
			abort();
			break;
		}
		default:
			exit( sig );
			break;
	}
}

int noflush= 0;

int txfflush( FILE *fp )
{
	if( !noflush ){
		return( fflush( fp ) );
	}
	else{
		return(0);
	}
}

#ifdef __MACH__
char *cuserid(const char *dum)
{ struct passwd *pwd;
	if( (pwd= getpwuid( getuid())) ){
		return( pwd->pw_name );
	}
	else{
		return( "" );
	}
}
#endif

main( int argc, char **argv)
{	REGISTER int i, j= 0, c, r0, r, cnt= 0;
	int spaces= 0, arg1= 1, insert_cr= 0, verbose= 0, wrap= 0, wrapping= 0, ident= 0,
		leadwhite= 0, formfeed= 0, quoted_printable= 0, do_quoted_printable= 0, fold_lines= 0, skip= 0;
	REGISTER char *ss, *iname= NULL, *oname= NULL, *prcommand= NULL, prev_char, wrappunct= 1;
	REGISTER long ttot= 0L, tot= 0L, crtot= 0L, wwrapped= 0, wrapped= 0, qps= 0, fqps= 0, folds= 0,
		fwrapped= 0, emptylines= 0, doprint= False;
	int wrap_indent, newline= 0, folded= 0, rm_inpf= False, finfo= False, fopipe= False;
	FILE *fi, *fo= NULL;
	Pattern *nofold_pat= NULL;
	Pattern *nofold_line= NULL, *prev_nofold_line= NULL;

#define nextchar(fi,ss) ((ss>=buff && ss<=&buff[513])? (ss[1])? ss[1] : buff2[0] :\
	(ss>=buff2 && ss<=&buff[513])? (ss[1])? ss[1] : '\0' : '\0' )
#define nextcharp(fi,ss) (((ss)>=buff && (ss)<=&buff[512])? ((ss)[1])? &(ss)[1] : &buff2[0] :\
	((ss)>=buff2 && (ss)<=&buff2[512])? ((ss)[1])? &(ss)[1] : NULL : NULL )
#define nextstring(fi,ss) ((ss[1])? &ss[1] : buff2)
#define PUTC(c,fp)	fputc((c),(fp)); last_char[0]= (c);
#define STRCASECMP(a,b)	strncasecmp((a),(b),strlen((b)))

	{ char *base= rindex( argv[0], '/');
		if( !base ){
			base= argv[0];
		}
		else{
			base++;
		}
		if( strncmp( base, "txw", 3)== 0 ){
			wrap_indent= False;
			if( isdigit( base[3] ) ){
				if( (wrap= atoi( &(base[3])))< 20 ){
					wrap= 20;
				}
			}
			else{
				wrap= 80;
			}
		}
	}
	while( arg1< argc){
		if( argv[arg1][0]== '-'){
			if( isdigit( argv[arg1][1]) ){
				spaces= atoi( &(argv[arg1][1]) );
			}
			else if( strcmp( &(argv[arg1][1]), "cr")== 0){
				insert_cr= 1;
			}
			else if( strcmp( &(argv[arg1][1]), "v")== 0){
				verbose= 1;
			}
			else if( strcmp( &(argv[arg1][1]), "v2")== 0){
				verbose= 2;
			}
			else if( strcmp( &(argv[arg1][1]), "qp2")== 0){
				quoted_printable= 2;
			}
			else if( strcmp( &(argv[arg1][1]), "qp")== 0){
				quoted_printable= 1;
			}
			else if( strncmp( &(argv[arg1][1]), "pr", 2)== 0){
				doprint= 1;
				if( argv[arg1][3] ){
					prcom= &argv[arg1][3];
				}
			}
			else if( strncmp( &(argv[arg1][1]), "nf", 2)== 0){
				nofold_pat= add_pattern( nofold_pat, &(argv[arg1][3]), False );
			}
			else if( strncmp( &(argv[arg1][1]), "NF", 2)== 0){
				nofold_pat= add_pattern( nofold_pat, &(argv[arg1][3]), True );
			}
			else if( strcmp( &(argv[arg1][1]), "fl")== 0){
				fold_lines= 1;
			}
			else if( strcmp( &(argv[arg1][1]), "fi")== 0){
				finfo= 1;
			}
			else if( strcmp( &(argv[arg1][1]), "noflush")== 0){
				noflush= 1;
			}
			else if( argv[arg1][1]== 'w' || argv[arg1][1]== 'W' ){
				if( argv[arg1][2] == 's' ){
					wrappunct = 0;
				}
				else{
					if( isdigit( argv[arg1][2]) ){
						if( (wrap= atoi( &(argv[arg1][2])) )< 20 )
							wrap= 20;
					}
					else{
						wrap= 80;
					}
					wrap_indent= (argv[arg1][1]== 'W')? True : False;
				}
			}
			else if( argv[arg1][1]== 'f' ){
				if( isdigit( argv[arg1][2]) ){
					if( (formfeed= atoi( &(argv[arg1][2])) )< 1 )
						formfeed= 1;
				}
				else{
					formfeed= 1;
				}
			}
			else{
				help(argv, argv[arg1] );
			}
		}
		else{
			if( iname){
				if( !oname){
					oname= argv[arg1];
				}
				else{
					help(argv, argv[arg1]);
				}
			}
			else{
				iname= strdup( argv[arg1] );
			}
		}
		arg1++;
	}

	signal( SIGHUP, sig_handler );
	signal( SIGINT, sig_handler );
	signal( SIGUSR1, sig_handler );
	signal( SIGUSR2, sig_handler );
	signal( SIGKILL, sig_handler );

	if( spaces<= 0)
		spaces= 8;
	errno= 0;
	if( oname && iname && strcmp( iname, oname)== 0 ){
	  char *c;
		c= tempnam( NULL, "txtmp" );
		if( (rename( iname, c )) ){
			fprintf( stderr, "Identical in and out filenames, but can't rename \"%s\" to \"%s\" (%s) \n",
				iname, c, serror()
			);
			exit(1);
		}
		else{
			free(iname);
			iname= strdup(c);
			rm_inpf= True;
		}
	}
	if( iname){
		fi= fopen( iname, "r");
	}
	else{
		fi= stdin;
		iname= strdup( "stdin" );
	}
	if( fi== NULL){
		fprintf( stderr, "Input error");
		if( errno)
			fprintf( stderr, ": %s", serror());
		fprintf( stderr, " (%s)\n", iname);
		exit( 1);
	}
	errno= 0;
	fopipe= False;
	if( oname){
		fo= fopen( oname, "w");
		if( doprint ){
			if( (prcommand= calloc( 1, strlen(prcom)+ 2+ strlen(oname))) ){
				sprintf( prcommand, "%s %s", prcom, oname );
			}
		}
	}
	else{
		if( doprint ){
			prcommand= strdup( prcom );
			oname= prcommand;
			fo= popen( prcommand, "w" );
			fopipe= True;
		}
		else{
			oname= "stdout";
			fo= stdout;
		}
#ifdef sgi
		setlinebuf( fo );
#endif
	}
	if( fo== NULL){
		fprintf( stderr, "Output error");
		if( errno)
			fprintf( stderr, ": %s", serror());
		fprintf( stderr, " (%s)\n", oname);
		exit( 1);
	}
	if( verbose){
		fprintf( stderr, "file in: %s ; file out: %s\n", iname, oname);
	}

	if( finfo ){
	  struct stat st;
	  struct tm *tm;
		if( (fi== stdin && !fstat(fileno(fi), &st)) || !stat( iname, &st) ){
			if( (tm= localtime( &(st.st_mtime) )) ){
			  time_t timer= time(NULL);
#ifndef L_cuserid
#	define L_cuserid 128
#endif
			  char *tstr= NULL, me[L_cuserid+16], him[L_cuserid+16], them[L_cuserid+16];
			  struct passwd *pw;
			  struct group *gr;
				setpwent();
				pw= getpwuid( st.st_uid );
				strncpy( him, (pw)? pw->pw_name : "", L_cuserid );
				endpwent();
				setgrent();
				gr= getgrgid( st.st_gid );
				strncpy( them, (gr)? gr->gr_name : "", L_cuserid );
				endgrent();
				tstr= strdup( asctime(tm) );
				if( tstr ){
					tstr[ strlen(tstr)-1 ]= '\0';
					fprintf( fo, "// File \"%s\"", iname );
					fprintf( fo, " owner %s", him );
					fprintf( fo, " [group %s]", them );
					fprintf( fo, " %lu bytes", (unsigned long) st.st_size );
					fprintf( fo, " %s", tstr );
					free(tstr);
				}
				tstr= strdup( asctime( localtime(&timer) ));
				if( tstr ){
					tstr[ strlen(tstr)-1 ]= '\0';
					fprintf( fo, "; listed d.d. %s for %s\n", tstr, cuserid(me) );
					free(tstr);
				}
			}
		}
		else{
			perror( iname );
		}
	}

	r0= (int) fgets( buff, 512, fi);
	fgets( buff2, 512, fi);
	r= 1;
	prev_char= 0;
	// for each line in the file (or 512 byte segment...)
	while( r0 && r ){

		buff[512]= '\0';
		buff2[512]= '\0';
#ifdef unix
		{ char *c= &buff[strlen(buff)-2];
			if( c>= buff && *c && *c== '\r' && c[1]== '\n' ){
				*c= '\n';
				c[1]= '\0';
			}
			c= &buff2[strlen(buff2)-2];
			if( c>= buff2 && *c && *c== '\r' && c[1]== '\n' ){
				*c= '\n';
				c[1]= '\0';
			}
		}
#endif
		i= 0; c= buff[0]; ss= buff;

		if( quoted_printable == 2 ){
			static short correctType= 1, armed= 0;
			if( strncasecmp( ss, "Content-Type: text/", 19 ) == 0
			    || strncasecmp( ss, "Content-Type:text/", 18 ) == 0
			){
				correctType= 2;
			}
			else if( strncasecmp( ss, "Content-Type:", 13 ) == 0 ){
				correctType= 0;
				if( verbose == 2 && do_quoted_printable ){
					fflush(stdout);
					fprintf( stderr, "deACTIVATING qp decoding (non-text Content-Type)\n" );
				}
				do_quoted_printable = 0;
				armed = 0;
			}
			if( correctType ){
				if( *ss == '\n' ){
					if( armed ){
						do_quoted_printable = 1;
					}
				}
				else if( STRCASECMP( ss, "Content-Transfer-Encoding: quoted-printable" ) == 0
					|| STRCASECMP( ss, "Content-Transfer-Encoding:quoted-printable" ) == 0
				){
					if( verbose == 2 && !do_quoted_printable ){
						fflush(stdout);
						fprintf( stderr, "ACTIVATING qp decoding%s\n",
							(correctType==2)? " after text content-type spec" : ""
						);
					}
					armed = 1;
				}
				else if( STRCASECMP( ss, "Content-Transfer-Encoding:" ) == 0 ){
					if( verbose == 2 && do_quoted_printable ){
						fflush(stdout);
						fprintf( stderr, "deACTIVATING qp decoding (other Transfer-Encoding)%s\n",
							(correctType==2)? " after text content-type spec" : ""
						);
					}
					do_quoted_printable = 0;
					armed = 0;
				}
			}
		}
		else if( quoted_printable ){
			do_quoted_printable = 1;
		}
		
		if(
#ifdef unix_redundant
			(*ss== 0x0d && ss[1]== 0x0a && ss[2]== '\0') ||
#endif
			(*ss== '\n' && ss[1]== '\0')
		){
			if( cnt!= 0 ){
				fputc( '\n', fo );
#ifdef DEBUG
				if( verbose ){
					fputc( '\n', stderr );
				}
#endif
			}
			emptylines+= 1;
			while( *ss!= '\n' ){
				ss++;
			}
			goto default_proc;
		}
		if( *ss ){
			prev_nofold_line= nofold_line;
			nofold_line= find_pattern( nofold_pat, buff );
			if( folded ){
				while( *ss && isspace((unsigned char)(*ss)) ){
					ss++;
				}
				if( *ss && !isspace((unsigned char)(*ss)) ){
					folded= 0;
				}
			}
		}
		if( *ss ){
			for( ; skip> 0 && *ss; skip-- ){
				ss++;
			}

			do{
				if( strncmp( ss, ":ts=", 4)== 0 || strncmp( ss, "vi:set ts=", 10)== 0){
				  int prev= spaces;
				  extern char *index();
					prev= atoi( &(index(ss, '=')[1]) );
					if( prev> 0){
						spaces= prev;
						if( verbose){
							txfflush( fo);
							fprintf( stderr, "%s: new TABSTOP = %d\n", argv[0], prev);
						}
					}
					else{
						txfflush( fo);
						fprintf( stderr,
							"%s: nonsense :ts=%d ignored\n", argv[0], prev
						);
						txfflush( stderr);
					}
				}
				if( wrap){
#ifdef unix_redundant
					if( *ss!= '\n' && !(*ss== 0x0d && ss[1]== 0x0a) )
#else
					if( *ss!= '\n')
#endif
					{
					  int doit= False, inword;
#define ISSPACE(ss) ( isspace((unsigned char) *(ss)) && !isspace((unsigned char)(ss)[1]) )
#define WRAPABLE(ss) ( (ISSPACE(ss) || (wrappunct && ispunct((unsigned char) *(ss)))) && !ispunct((unsigned char)(ss)[1]) )
						inword= !WRAPABLE(ss) && !WRAPABLE(last_char);
						if( cnt>= wrap || (*ss== '\t' && cnt+ spaces>= wrap) ){
						  /* wrapping needed now	*/
							doit= 2;
							fwrapped+= 1;
						}
						else if( WRAPABLE(ss) ){
						  char *s= nextcharp(fi,ss);
						  int Cnt= cnt+1;
						  /* scan rest of buffer to see if a wrapping is upcoming.
						   \ This is because we like to wrap in whitespace.
						   */
							while( s && *s && !WRAPABLE(s) && !doit ){
								if( Cnt>= wrap ){
									doit= 1;
								}
								Cnt++;
								s= nextcharp( fi, s);
							}
							if( doit ){
							  /* in this case, we want the current character to be output before
							   \ wrapping takes place, if it is not whitespace.
							   */
								if( ispunct((unsigned char) *(ss)) ){
									fputc( *ss, fo);
#ifdef DEBUG
									if( verbose ){
										fputc( *ss, stderr );
									}
#endif
									ss++;
									cnt++;
								}
								else{
									ss++;
								}
								wwrapped+= 1;
							}
						}
						if( doit ){
							if( !wrapping){
							  int nident= ident+ spaces/ 2;
								if( wrap_indent ){
									if( wrap- nident< wrap/ 4){
										ident-= wrap/ 4;
										if( ident<= 0)
											ident= wrap/ 4;
									}
									else{
										ident= nident;
									}
								}
							}
							wrapping= 1;
							if( fold_lines && inword ){
								fputc( '-', fo );
#ifdef DEBUG
								if( verbose ){
									fputc( '-', stderr );
								}
#endif
							}
							  /* Here we must of course output a newline, regardles
							   \ of fold_lines!
							   */
							if( insert_cr){
								PUTC( 0x0d, fo);
#ifdef DEBUG
								if( verbose ){
									PUTC( 0x0d, stderr );
								}
#endif
								crtot++;
							}
							PUTC( '\n', fo);
#ifdef DEBUG
							if( verbose ){
								PUTC( '\n', stderr );
							}
#endif
							if( verbose== 2 ){
								if( doit== 2 ){
									fprintf( stderr, "Forced wrap %d: ", fwrapped );
								}
								else{
									fprintf( stderr, "Word wrap %d: ", wwrapped );
								}
								fprintf( stderr, "\"%s\" -> \"%s\" -> \"%s\", buf=\"%s\"\n",
									buff0, buff, buff2, ss
								);
								txfflush( stderr );
							}
							wrapped++;
							cnt= 0;
							{ int idnt= ident;
								if( nofold_line ){
								  char *c= nofold_line->pat;
									while( c && *c ){
										PUTC( (*c), fo);
#ifdef DEBUG
										if( verbose ){
											fputc( (*c), stderr );
										}
#endif
										c++;
										idnt-= 1;
									}
									if( nofold_line->afterpat ){
										PUTC( (nofold_line->afterpat), fo);
#ifdef DEBUG
										if( verbose ){
											PUTC( (nofold_line->afterpat), stderr );
										}
#endif
										idnt-= 1;
									}
/* 									nofold_line= NULL;	*/
								}
								for( ; cnt< idnt; cnt++){
									PUTC( ' ', fo);
#ifdef DEBUG
									if( verbose ){
										PUTC( ' ', stderr );
									}
#endif
								}
							}
							if( fold_lines== -1 ){
								ident= 0;
							}
							else{
							  char *s= ss;
								  /* remove any whitespaces after wrapping a line.	*/
								while( s && *s && isspace((unsigned char) *(s) ) ){
									s= nextcharp(fi,s);
								}
								if( isspace((unsigned char)*(ss)) && s ){
									ss= s;
								}
							}
							leadwhite= 1;
						}
					}
				}
test_char:;
				switch( *ss){
					case 0:
						break;
					case '\t':
						if( leadwhite || ss== buff ){
							if( !wrapping)
								ident+= spaces;
							else
								break;
						}
						fputc( ' ', fo);
#ifdef DEBUG
						if( verbose ){
							fputc( ' ', stderr );
						}
#endif
						cnt++;
						if( !(wrapping && leadwhite) ){
							for( ; cnt % spaces; cnt++){
								if( wrap && cnt>= wrap ){
									*ss= '\n';
									goto default_proc;
								}
								PUTC( ' ', fo);
#ifdef DEBUG
								if( verbose ){
									PUTC( ' ', stderr );
								}
#endif
							}
							ttot+= (long) spaces;
						}
						tot+= 1L;
						break;
					case 0x01:
						  /* when folding, a line-terminating ^A prevents folding,
						   \ and is not output.
						   */
						if( fold_lines== 0 || nextchar( fi, ss)!= '\n' ){
							goto default_proc;
						}
						break;
#ifdef unix
					case 0x0d:
						if( nextchar(fi, ss)!= 0x0a ){
							goto default_proc;
						}
						break;
#endif
					case 0x0a:{
					  int do_cr= 0;
					  char nc= nextchar( fi, ss);
#ifdef unix_redundant
					  char *nnc= nextcharp( fi, &ss[1]);
#endif
						prev_nofold_line= nofold_line;
						nofold_line= find_pattern( nofold_pat, nextstring( fi, ss) );
						if( fold_lines ){
							if( ss== buff || prev_char== 0x0a ){
							  /* Empty line	*/
								fold_lines= -2;
							}
							else if( ss[-1]== 0x01 ){
							  /* or line terminated with a ^A	*/
								fold_lines= -2;
							}
							else if( nofold_line || prev_nofold_line ){
							  /* don't fold "onto" the last nofold_line neither	*/
								fold_lines= -1;
							}
							else{
								fold_lines= 1;
							}
						}
						if( nc== '\n'
#ifdef unix_redundant
							|| (nc== 0x0d && nnc && *nnc== 0x0a)
#endif
						){
							do_cr= 1;
							newline= 2;
						}
						else if( newline== 2 ){
							do_cr= 1;
							newline= 0;
						}
						else{
							newline= 1;
						}
						if( fold_lines<= 0 || do_cr ){
							if( insert_cr){
								if( ss[-1]!= 0x0d){
									PUTC( 0x0d, fo);
#ifdef DEBUG
									if( verbose ){
										PUTC( 0x0d, stderr );
									}
#endif
									crtot++;
								}
							}
						}
						else{
							folds+= 1;
							folded= 1;
							*ss= ' ';
						}
						goto default_proc;
					}
					case '=':{
					  char *s1= nextcharp(fi, ss),
						  *s2= nextcharp(fi, &ss[1]),
						  xbuf[3];
					  int newline;
						newline= (s1 && *s1== '\n'
#ifdef unix_redundant
								|| (s1 && s2 && (*s1== 0x0d && *s2== 0x0a))
#endif
							);
						if( do_quoted_printable && ((s1 && newline) || (s1 && *s1 && s2 && *s2)) ){
						  int c;
							if( newline || (isxdigit(*s1) && isxdigit(*s2)) ){
								qps+= 1;
								if( newline ){
									  /* a line-terminating '=' means wrapping.	*/
									skip= 1;
									goto next_char;
									  /* break should get us to the same place??!	*/
									break;
								}
								else{
									  /* decode the hex code	*/
									xbuf[0]= *s1;
									xbuf[1]= *s2;
									xbuf[2]= '\0';
									sscanf( xbuf, "%x", &c );
								}
								c= c & 0x00FF;
								*ss= c;
								skip= 2;
								switch( c ){
									case '\t':
									case 0x0a:
										  /* use the normal char-handling code for the decoded character,
										   \ in the same sweep by jumping back (oops!... ;-)).
										  */
										goto test_char;
										break;
									default:
										  /* use the normal printing code for the decoded character	*/
										goto default_proc;
										break;
								}
								  /* decoded valid char, don't fall through to the (default) printing code
								   \ (but we should not end up here!
								   */
								break;
							}
							else{
							  /* no '\n' or valid hexnumber following =; just print the pattern by falling through.	*/
								fqps+= 1;
								if( verbose== 2 ){
									fprintf( stderr, "\"%s\": invalid MIME QUOTED-PRINTABLE code \"%s\"\n",
										buff, ss
									);
									txfflush( stderr );
								}
							}
						}
						else if( do_quoted_printable ){
							fqps+= 1;
						}
					}
					default:
default_proc:;
						PUTC( *ss, fo);
#ifdef DEBUG
						if( verbose ){
							PUTC( *ss, stderr );
						}
#endif
						if( *ss== '\n'
#ifdef unix_redundant
							|| (*ss== 0x0d && nextchar(fi,ss)== 0x0a)
#endif
						){
							wrapping= 0;
							ident= 0;
							cnt= 0;
							leadwhite= 1;
						}
						else{
							if( leadwhite){
								if( ! isspace((unsigned char)*ss) )
									leadwhite= 0;
								else
									ident++;
							}
							cnt++;
						}
						break;
				}
next_char:;
				i++;
				if( folded ){
					while( *ss && isspace((unsigned char)(*ss)) ){
						ss++;
					}
					if( *ss && !isspace((unsigned char)(*ss)) ){
						folded= 0;
					}
				}
				for( ; skip> 0 && *ss; skip-- ){
					ss++;
				}
#ifdef DEBUG
				txfflush(fo);
#endif
				prev_char= *ss;
			}
			while( *ss && *(++ss) && c!= EOF );
		}
		if( verbose== 2 ){
			strcpy( buff0, buff );
		}
		strcpy( buff, buff2 );
		r0= strlen(buff);
		{ int rr= (int) fgets( buff2, 512, fi);
			if( r> 0 && !rr ){
				r= -r;
				buff2[0]= '\0';
			}
			else{
				r= rr;
			}
		}
		j+= 1;
	}
	for( i= 0; i< formfeed; i++ ){
		fputc( 0x0c, fo);
#ifdef DEBUG
		if( verbose ){
			fputc( 0x0c, stderr );
		}
#endif
	}
	if( fi!= stdin){
		fclose( fi);
	}
	else
		txfflush( fi);
	if( rm_inpf ){
		unlink( iname );
	}
	if( iname ){
		free(iname);
	}
	if( fo!= stdout){
		if( fopipe ){
			pclose(fo);

		}
		else{
			fclose( fo);
		}
		if( doprint && !fopipe ){
			system( prcommand );
		}
	}
	else{
		txfflush( fo);
	}
	if( verbose){
		fprintf( stderr, "%s", argv[0] );
		if( !(tot || crtot || wrapped || !qps || !folds ) ){
			fprintf( stderr, "\t: no changes\n");
			exit(0);
		}
		if( tot){
			fprintf( stderr, "\t: %ld TABs -> %ld SPACEs\n",
				tot, ttot
			);
		}
		if( qps ){
			fprintf( stderr, "\t: %ld QUOTED-PRINTABLE codes decoded", qps );
			if( fqps ){
				fprintf( stderr, " (%d failed opcodes)", fqps );
			}
			fputc( '\n', stderr);
		}
		if( folds ){
			fprintf( stderr, "\t: %ld lines folded\n", folds );
		}
		if( crtot){
			fprintf( stderr, "\t: %ld CRs inserted\n", crtot);
		}
		if( wrapped){
			fprintf( stderr, "\t: %ld lines wrapped to width %d (%d word wraps)\n",
				wrapped, wrap, wwrapped
			);
		}
		if( emptylines ){
			fprintf( stderr, "\t: %ld empty lines \"shortcut\"\n",
				emptylines
			);
		}
		if( i){
			fprintf( stderr, "\t: %d formfeeds appended\n", i);
		}
		if( doprint ){
			fprintf( stderr, "\t: printed using %scommand \"%s\"\n",
				(fopipe)? "pipe to " : "", prcommand
			);
		}
	}
	if( nofold_pat ){
	  Pattern *l= nofold_pat, *m;
		while( l ){
			if( l->hits && verbose ){
				fprintf( stderr, "\t\t: %dx nofold pattern \"%s\"\n", l->hits, l->pat );
			}
			m= l;
			l= l->next;
			free(m);
		}
	}
	exit( 0);
}

#ifdef MCH_AMIGA
_wb_parse()
{
	exit(212);
}
#endif MCH_AMIGA
