#include <stdio.h>
#include <signal.h>
#include <errno.h>

#if !defined(linux) && !defined(__MACH__) && !defined(__ERRNO_H__) && !defined(__MINGW32__)
extern char *sys_errlist[];
#endif
extern char *getenv();
//extern int errno, system();

main( argc, argv)
int argc;
char **argv;
{ int i, verbose= (getenv("VERBOSE")!=NULL), prexit=(getenv("printexitvalue")!=NULL);

	for( i= 1; i< argc-1; i++)
		argv[i][ strlen(argv[i]) ]= ' ';
	if( verbose)
		fprintf( stderr, "system `%s`\n", argv[1]);

#ifndef __MINGW32__
	if( setsid()== -1 && verbose) {
		perror( "system: error in setsid()");
	}
#endif
	errno= 0;
	i= system( argv[1] ) >> 8;				/* system returns the return code in the high byte..	*/
	if( errno && prexit){
		fprintf( stderr, "Exit %d (errno=%d : %s)\n", i, errno, strerror(errno) );
	}
	else if( prexit){
		fprintf( stderr, "Exit %d\n", i);
	}
	exit( i);
}
