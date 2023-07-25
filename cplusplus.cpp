#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>

#if defined(i386) || defined(__i386__)
#	define __ARCHITECTURE__	"i386"
#elif defined(__x86_64__) || defined(x86_64) || defined(_LP64)
#	define __ARCHITECTURE__	"x86_64"
#elif defined(__ppc__)
#	define __ARCHITECTURE__	"ppc"
#else
#	define __ARCHITECTURE__	""
#endif

#ifndef SWITCHES
#	ifdef DEBUG
#		define _IDENTIFY(s,i)	static const char *xg_id_string= "$Id: @(#) (\015\013\t\t" s "\015\013\t) DEBUG version" i __ARCHITECTURE__" " " $"
#	else
#		define _IDENTIFY(s,i)	static const char *xg_id_string= "$Id: @(#) (\015\013\t\t" s "\015\013\t)" i __ARCHITECTURE__" " " $"
#	endif
#else
  /* SWITCHES contains the compiler name and the switches given to the compiler.	*/
#	define _IDENTIFY(s,i)	static const char *xg_id_string= "$Id: @(#) (\015\013\t\t" s "\015\013\t)[" __ARCHITECTURE__" " SWITCHES"]" " $"
#endif

#define __IDENTIFY(s,i)\
static const char *ident_stub(){ _IDENTIFY(s,i);\
	static char called=0;\
	if( !called){\
		called=1;\
		return(ident_stub());\
	}\
	else{\
		called= 0;\
		return(xg_id_string);\
	}}

#ifdef __GNUC__
#	ifdef __clang__
#		define IDENTIFY(s)	__attribute__((used)) __IDENTIFY(s," (clang" STRING(__clang__) ")")
#	else
#		define IDENTIFY(s)	__attribute__((used)) __IDENTIFY(s," (gcc" STRING(__GNUC__) ")")
#	endif
#else
#	define IDENTIFY(s)	__IDENTIFY(s," (cc)")
#endif
IDENTIFY("__cplusplus value checker");

int main(int argc, char *argv[])
{
#ifdef __cplusplus
	fprintf( stderr, "Code built using \"%s\", __cplusplus=", ident_stub() );
	fflush(stderr);
	fprintf( stdout, "%ld\n", __cplusplus );
	exit(0);
#else
	fprintf( stderr, "%s was built as something other than C++\n", argv[0] );
	exit(-1);
#endif
}
