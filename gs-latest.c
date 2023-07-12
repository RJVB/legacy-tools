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

IDENTIFY( "Simple gs wrapper to allow some local configurations. (C) 2002 RJVB" );

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

#define SETTINGS	"/usr/local/lib/ghostscript/local-wrapper-settings"
#define APPLICATION		"/usr/local/bin/gs.latest"

#define xfree(x)	if((x)){ free((x)) ; (x)=NULL; }

#define SETENV()	if( *name && nvalue ){ \
		if( value && strcmp(value, nvalue) ){ \
			nvalue= concat2( nvalue, ((nvalue)? seper : ""), value, 0 ); \
		} \
	} \
	setenv( name, nvalue, 1 ); \
	xfree(nvalue); name[0]= '\0';

main( int argc, char *argv[] )
{ char name[MAXPATHLEN], *value= NULL, seper[2]= {0,0};
  char *nvalue= NULL;
  char application[MAXPATHLEN];
  FILE *fp= fopen(SETTINGS, "r");
  char buf[MAXPATHLEN];

	application[0]= '\0';
	name[0]= '\0';
	if( fp ){
		while( fgets( buf, sizeof(buf), fp ) && !feof(fp) && !ferror(fp) ){
		  int l;
			if( buf[0]!= '#' ){
				if( buf[ (l= strlen(buf)-1) ]== '\n' ){
					buf[l]= '\0';
				}
				if( *buf ){
					if( strncmp( buf, "application=", 12)== 0 ){
					  char *c= &buf[12];
						while( c && *c && isspace(*c) ){
							c++;
						}
						if( c && *c ){
							strcpy( application, c);
						}
					}
					else if( strncmp( buf, "name=", 5) == 0 ){
					  char *c= &buf[5];
						while( c && *c && isspace(*c) ){
							c++;
						}
						if( c && *c ){
							strcpy( name, c);
							value= getenv(name);
						}
					}
					else if( strncmp( buf, "seper=", 6)== 0 ){
						seper[0]= buf[6];
					}
					else if( name ){
						nvalue= concat2( nvalue, ((nvalue)? seper : ""), buf, 0 );
					}
				}
				else{
					SETENV();
				}
			}
			else{
				SETENV();
			}
		}
		fclose(fp);
	}
	SETENV();
	if( *application ){
		execv( application, argv );
		fprintf( stderr, "%s: ", application );
		perror( "Can't execute" );
	}
	else{
		execv( APPLICATION, argv );
		fprintf( stderr, "%s: ", APPLICATION );
		perror( "Can't execute" );
	}
}
