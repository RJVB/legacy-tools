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

IDENTIFY( "Simple IRIX cc wrapper to allow some local configurations and ignore gcc flags. (C) 2003 RJVB" );

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

#ifdef sgi
char *setenv( const char *n, const char *v, int overwrite_ignored )
{	static char *fail_env= "setenv=failed (no memory)";
	char *nv;

	if( !n ){
		return(NULL);
	}
	if( !v ){
		v= "";
	}
	if( !(nv= calloc( strlen(n)+strlen(v)+2, 1L)) ){
		putenv( fail_env);
		return( NULL);
	}
	sprintf( nv, "%s=%s", n, v);
	putenv( nv);
	return( getenv(n) );
}
#endif

#define SETTINGS	"/usr/local/etc/cc-local-CFLAGS"
#define CC		"/usr/bin/cc"

main( int argc, char *argv[] )
{ char *ecflags= getenv("CFLAGS");
  char *cflags= NULL;
  char cc[MAXPATHLEN];
  FILE *fp= fopen(SETTINGS, "r");
  char buf[MAXPATHLEN];

	cc[0]= '\0';
	if( fp ){
		while( fgets( buf, sizeof(buf), fp ) && !feof(fp) && !ferror(fp) ){
		  int l;
			if( buf[0]!= '#' ){
				if( buf[ (l= strlen(buf)-1) ]== '\n' ){
					buf[l]= '\0';
				}
				if( *buf ){
					if( strncmp( buf, "cc=", 5)== 0 ){
					  char *c= &buf[5];
						while( c && *c && isspace(*c) ){
							c++;
						}
						if( c && *c ){
							strcpy( cc, c);
						}
					}
					else{
						cflags= concat2( cflags, ((cflags)? ":" : ""), buf, 0 );
					}
				}
			}
		}
		fclose(fp);
	}
	if( ecflags && (!cflags || strcmp( cflags, ecflags)) ){
		cflags= concat2( cflags, ((cflags)? ":" : ""), ecflags, 0 );
	}
	setenv( "CFLAGS", cflags, 1);
	{ int i, j;
	  int hit= 0;
		for( i= 1; i< argc; i++ ){
			if( strncmp( argv[i], "-mcpu", 5)== 0 || strncmp( argv[i], "-march", 6)== 0 ){
				fprintf( stderr, "%s: Warning 0: ignoring option '%s'\n", argv[0], argv[i] );
				for( j= i; j< argc-1; j++ ){
					argv[j]= argv[j+1];
				}
				argv[j]= NULL;
				argc-= 1;
				i-= 1;
				hit+= 1;
			}
			else if( strncmp( argv[i], "-mabi=", 6)== 0 ){
				if( strncmp( &argv[i][6], "32", 2)== 0 ){
					argv[i]= "-32";
				}
				else if( strncmp( &argv[i][6], "n32", 3)== 0 ){
					argv[i]= "-n32";
				}
				else if( strncmp( &argv[i][6], "64", 3)== 0 ){
					argv[i]= "-n64";
				}
				else{
					fprintf( stderr, "%s: Warning 0: ignoring option '%s'\n", argv[0], argv[i] );
					for( j= i; j< argc-1; j++ ){
						argv[j]= argv[j+1];
					}
					argv[j]= NULL;
					argc-= 1;
					i-= 1;
				}
				hit+= 1;
			}
		}
	}
	if( *cc ){
		  /* use execve(): under linux, execv(cc, argv) complains about too many arguments?! */
		execv( cc, argv );
		fprintf( stderr, "%s: ", cc );
		perror( "Can't execute" );
	}
	else{
		execv( CC, argv );
		fprintf( stderr, "%s: ", CC );
		perror( "Can't execute" );
	}
}
