#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>
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


IDENTIFY( "Simple wrapper that closes stdin, stdout and stderr before handing over control to argv[0]+''.bin''. (C) 2014 RJVB" );

int main( int argc, char *argv[] )
{ char *progName = NULL;
	errno = 0;
	asprintf( &progName, "%s.bin", argv[0] );
	if( progName ){
		int i;
		fclose(stdin);
		fclose(stdout);
		fprintf( stderr, "Launching %s", progName );
		for( i = 1 ; i < argc ; ++i ){
			fprintf( stderr, " \"%s\"", argv[i] );
		}
		fputc( '\n', stderr );
		fclose(stderr);
		argv[0]= progName;
		execv( progName, argv );
		  /* not reached unless error */
		{ FILE *fp = fopen("/dev/tty", "w");
			fprintf( fp, "%s: %s\n", progName, strerror(errno) );
			fclose(fp);
		}
		exit(errno);
	}
	else{
		fprintf( stderr, "We shouldn't be here: %s::%d!\n", __FILE__, __LINE__ );
		if( !errno ){
			errno = ENOMEM;
		}
	}
	exit(errno);
}
