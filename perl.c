#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sys/param.h>

#ifndef SWITCHES
#	ifdef DEBUG
#		define _IDENTIFY(s,i)	"$Id: @(#) '" __FILE__ "'-[" __DATE__ "," __TIME__ "]-(\015\013\t\t" s "\015\013\t) DEBUG version" i " $"
#	else
#		define _IDENTIFY(s,i)	"$Id: @(#) '" __FILE__ "'-[" __DATE__ "," __TIME__ "]-(\015\013\t\t" s "\015\013\t)" i " $"
#	endif
#else
  /* SWITCHES contains the compiler name and the switches given to the compiler.	*/
#	define _IDENTIFY(s,i)	"$Id: @(#) '" __FILE__ "'-[" __DATE__ "," __TIME__ "]-(\015\013\t\t" s "\015\013\t)["SWITCHES"] $"
#endif

#ifdef PROFILING
#	define __IDENTIFY(s,i)\
	static char *ident= _IDENTIFY(s,i);
#else
#	define __IDENTIFY(s,i)\
	static char *ident_stub(){ static char *ident= _IDENTIFY(s,i);\
		static char called=0;\
		if( !called){\
			called=1;\
			return(ident_stub());\
		}\
		else{\
			called= 0;\
			return(ident);\
		}}
#endif

#ifdef __GNUC__
#	define IDENTIFY(s)	__IDENTIFY(s," (gcc)")
#else
#	define IDENTIFY(s)	__IDENTIFY(s," (cc)")
#endif

IDENTIFY( "Simple perl wrapper to allow some local configurations. (C) 2002 RJVB" );

/* concat2(): concats a number of strings; reallocates the first, returning a pointer to the
 \ new area. Caution: a NULL-pointer is accepted as a first argument: passing a single NULL-pointer
 \ as the (1-item) arglist is therefore valid, but possibly lethal!
 */
char *concat2( char *target, ...)
{ va_list ap;
  int n= 0, tlen= 0;
  char *c, *buf= NULL, *first= NULL;
	va_start(ap, target);
	while( (c= va_arg(ap, char*)) != NULL || n== 0 ){
		if( !n ){
			first= target;
		}
		if( c ){
			tlen+= strlen(c);
		}
		n+= 1;
	}
	va_end(ap);
	if( n== 0 || tlen== 0 ){
		return( NULL );
	}
	tlen+= 4;
	if( first ){
		tlen+= strlen(first)+ 1;
		buf= realloc( first, tlen* sizeof(char) );
	}
	else{
		buf= first= calloc( tlen, sizeof(char) );
	}
	if( buf ){
		va_start(ap, target);
		  /* realloc() leaves the contents of the old array intact,
		   \ but it may no longer be found at the same location. Therefore
		   \ need not make a copy of it; the first argument can be discarded.
		   \ strcat'ing is done from the 2nd argument.
		   */
		while( (c= va_arg(ap, char*)) != NULL ){
			strcat( buf, c);
		}
		va_end(ap);
	}
	return( buf );
}

#define SETTINGS	"/usr/local/etc/perl5-local-libdirs"
#define PERL		"/usr/bin/perl"

main( int argc, char *argv[] )
{ char *p5lib= getenv("PERL5LIB"), *plib= getenv("PERLLIB");
  char *np5lib= NULL, *nplib= NULL;
  char perl[MAXPATHLEN];
  FILE *fp= fopen(SETTINGS, "r");
  char buf[MAXPATHLEN];

	perl[0]= '\0';
	if( fp ){
		while( fgets( buf, sizeof(buf), fp ) && !feof(fp) && !ferror(fp) ){
		  int l;
			if( buf[0]!= '#' ){
				if( buf[ (l= strlen(buf)-1) ]== '\n' ){
					buf[l]= '\0';
				}
				if( *buf ){
					if( strncmp( buf, "perl=", 5)== 0 ){
					  char *c= &buf[5];
						while( c && *c && isspace(*c) ){
							c++;
						}
						if( c && *c ){
							strcpy( perl, c);
						}
					}
					else{
						np5lib= concat2( np5lib, ((np5lib)? ":" : ""), buf, 0 );
						nplib= concat2( nplib, ((nplib)? ":" : ""), buf, 0 );
					}
				}
			}
		}
		fclose(fp);
	}
	if( p5lib && strcmp( np5lib, p5lib) ){
		np5lib= concat2( np5lib, ((np5lib)? ":" : ""), p5lib, 0 );
	}
	if( plib && strcmp( nplib, plib) ){
		nplib= concat2( nplib, ((nplib)? ":" : ""), plib, 0 );
	}
	setenv( "PERL5LIB", np5lib, 1);
	setenv( "PERLLIB", nplib, 1);
	if( *perl ){
		  /* use execve(): under linux, execv(perl, argv) complains about too many arguments?! */
		execv( perl, argv );
		fprintf( stderr, "%s: ", perl );
		perror( "Can't execute" );
	}
	else{
		execv( PERL, argv );
		fprintf( stderr, "%s: ", PERL );
		perror( "Can't execute" );
	}
}
