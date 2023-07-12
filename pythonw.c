#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sys/param.h>

#include <errno.h>

#include <libgen.h>

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
#	define IDENTIFY(s)	__attribute__((used)) __IDENTIFY(s," (gcc)")
#else
#	define IDENTIFY(s)	__IDENTIFY(s," (cc)")
#endif


IDENTIFY( "Simple Mac OS X python wrapper to launch the Python framework as #!/usr/local/bin/python[w] in .py scrips. (C) 2005 RJVB" );

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

/* The settings file can be used to specify another path to the actual executable */
#define SETTINGS	"/usr/local/etc/python-local-settings"
/* The following path to the system's python application should always work, as long as the framework remains put: */
#define PYTHON	"/System/Library/Frameworks/Python.framework/Versions/Current/Resources/Python.app/Contents/MacOS/Python"

main( int argc, char *argv[] )
{ char *epywopts= NULL /* getenv("PYTHONWOPTS") */;
  char *pywopts= NULL;
  char python[MAXPATHLEN];
  char settings[MAXPATHLEN]= SETTINGS;
  FILE *fp= NULL;
  char buf[MAXPATHLEN];

	python[0]= '\0';
	if( (argv[0]= basename(argv[0])) ){
		sprintf( settings, "/usr/local/etc/%s-local-settings", argv[0] );
		fp= fopen(settings, "r");
	}
	if( !fp ){
		fp= fopen(settings, "r");
	}
	if( fp ){
#ifdef DEBUG
		fprintf( stderr, "%s: reading local settings from \"%s\"...\n", argv[0], settings );
#endif
		while( fgets( buf, sizeof(buf), fp ) && !feof(fp) && !ferror(fp) ){
		  int l;
			if( buf[0]!= '#' ){
				if( buf[ (l= strlen(buf)-1) ]== '\n' ){
					buf[l]= '\0';
				}
				if( *buf ){
#undef BINARY
#define BINARY	"python"
					if( strncmp( buf, BINARY "=", strlen(BINARY)+1 )== 0 ){
					  char *c= &buf[strlen(BINARY)+1];
						while( c && *c && isspace(*c) ){
							c++;
						}
						if( c && *c ){
							strcpy( python, c);
						}
					}
					else{
					  char *c = strchr( buf, '=' );
// 20091126: since Python doesn't handle PYTHONWOPTS as an env.var, we just interpret extra lines
// without special keywords as variables to store in the environment. A typical line would be
// MACOSX_DEPLOYMENT_TARGET=10.4
// if that variable isn't set by default (which would seem to be a good idea).
// 						pywopts= concat2( pywopts, ((pywopts)? ":" : ""), buf, 0 );
						if( c ){
							*c = '\0';
							setenv( buf, &c[1], 1 );
							*c = '=';
						}

					}
				}
			}
#ifdef DEBUG
			fprintf( stderr, "%s: python=\"%s\", (ignored) options=\"%s\"\n", argv[0], python, (pywopts)? pywopts : "<none>" );
#endif
		}
		fclose(fp);
	}
	if( epywopts && (!pywopts || strcmp( pywopts, epywopts)) ){
		pywopts= concat2( pywopts, ((pywopts)? ":" : ""), epywopts, 0 );
	}
	if( pywopts ){
	  /* all this is dummy code: python has no env.var checked for options */
		setenv( "PYTHONWOPTS", pywopts, 1);
	}
// 	{ int i, j;
// 	  int hit= 0;
// 		for( i= 1; i< argc; i++ ){
			/* exclude or modify arguments here. Exclusion means left-shifting subsequent arguments.
			 \ Extension is only possible by reallocating the argv array!
			 \ The last element must be a NULL.
			 \ It is unclear if we can do a free(argv[i]) before changing it. It should not have big
			 \ memory usage effects if we do not, so let the system handle the freeing, just to be safe.
			 */
// 		}
// 	}

	if( !*python ){
	  double version;
	  char *vstring;
		if( argv[0][6] == 'w' ){
			vstring = &argv[0][7];
		}
		else{
			vstring = &argv[0][6];
		}
		version = strtod( vstring, NULL );
		if( version > 0 ){
			snprintf( python, MAXPATHLEN,
				"/Library/Frameworks/Python%s.framework/Versions/Current/Resources/Python.app/Contents/MacOS/Python",
				vstring
			);
		}
		else{
			strncpy( python, PYTHON, sizeof(python)/sizeof(char)-1 );
		}
	}
#ifdef DEBUG
	{ int i;
		fprintf( stderr, "%s: execv(\"%s\"", argv[0], python );
		for( i= 1; i< argc; i++ ){
			fprintf( stderr, ",\"%s\"", argv[i] );
		}
		fputs( "\n", stderr );
	}
#endif
	if( *python ){
		  /* In the case of pythonw on Mac OS X, the actual binary wants to be called by its true name.
		   \ Otherwise, it won't connect to the windowing environment, which is the sole purpose of
		   \ calling python through pythonw. This is also the reason why a simple symlink to Python
		   \ won't have the desired effect, and we have to use this solution. Thus, modify argv[0] .
		   */
		if( argc <= 1 ){
			fprintf( stderr, "RJVB %s launching ", basename(argv[0]) );
			fflush( stderr );
		}
		argv[0]= python;
		execv( python, argv );
		  /* not reached unless error */
		fprintf( stderr, "%s: ", python );
		perror( "Can't execute" );
		exit(errno);
	}
	else{
		fprintf( stderr, "We shouldn't be here: %s::%d!\n", __FILE__, __LINE__ );
		exit(-1);
	}
}
