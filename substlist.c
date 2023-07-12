#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <sys/unistd.h>

main( int argc, char *argv[] )
{ int i= 1;
	if( argc<= 2 ){
		fprintf( stderr, "Usage: %s <target-letter>[:] <source-path> ...\n", argv[0] );
		fprintf( stderr, "       %s <target-letter>[:] /d ...\n", argv[0] );
		fprintf( stderr,
			"Simple programme to SUBStitute a series of drives, without --hopefully--\n"
			"causing the new \"drives\" to be opened in the Explorer.\n"
			"From dos, must be called via bash:\n"
			"c:> bash.exe --login -c \"substlist p \\\"c:\progra~1\\\"\"\n"
		);
		exit(-1);
	}
	while( i+1< argc ){
	  char *target= argv[i];
	  char *source= argv[i+1];
	  char command[MAXPATHLEN];
		i+= 2;
		if( target[ strlen(target)-1 ] == ':' ){
			snprintf( command, sizeof(command)/sizeof(char), "subst %s \"%s\"", target, source );
		}
		else{
			snprintf( command, sizeof(command)/sizeof(char), "subst %s: \"%s\"", target, source );
		}
		fprintf( stderr, "%s\n", command );
		system( command );
	}
	exit(0);
}
