#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <stddef.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#if !defined(linux) && !defined(__MACH__) && !defined(__ERRNO_H__) && !defined(__MINGW32__)
extern char *sys_errlist[];
#endif
extern char *getenv();
//extern int errno, system();

int main( int argc, char *argv[], char *envp[])
{ int i, verbose= (getenv("VERBOSE")!=NULL);

    if( verbose ){
        fprintf( stderr, "execve `%s`", argv[1] );
        for( i= 1; i< argc-1; i++){
            argv[i][ strlen(argv[i]) ]= ' ';
                fprintf( stderr, " %s", argv[i] );
        }
        fputs( "\n", stderr );
    }

    setuid(geteuid());
#ifndef __MINGW32__
    if( setsid()== -1 && verbose){
        perror( "execve: error in setsid()");
    }
#endif
    errno = 0;
    i = execve( argv[1], &argv[1], envp );
    if( errno){
        fprintf( stderr, "Exit %d (errno=%d : %s)\n", i, errno, strerror(errno) );
    }
    else{
        fprintf( stderr, "Exit %d\n", i);
    }
    exit(i);
}
