#include <stdio.h>
#include <limits.h>

#include <signal.h>

#include <sys/types.h>

extern char* calloc();
pid_t pid;
int fo= -1, fd= -1;

clean_up( action, mem)
int action;
char *mem;
{  static char *Mem= NULL;

	if( !action)
		Mem= mem;
	else{
		if( Mem)
			free( Mem);
		close( fd);
		close( fo);
		sleep(1);
		exit(action);
	}
}

int handle_sigint(sig)
int sig;
{
	if( sig== SIGINT){
		fputs( "xpipe: exitting\n", stderr);
		fflush( stderr);
		clean_up(sig);
	}
}

int handle_sighup(sig)
int sig;
{  static char called= 0;
	if( sig== SIGHUP && !called ){
		fputs( "xpipe: received HUP - detaching from tty\n", stderr);
		freopen( "/dev/null", "r", stdin);
		freopen( "/dev/null", "a", stdout);
		freopen( "/dev/null", "a", stderr);
		called++;
	}
	signal( SIGHUP, handle_sighup);
	return(0);
}

int read_pipe( fo, fd, bufsize)
int fo, fd, bufsize;
{ char *c, C;
  pid_t Pid;

	if( bufsize== 0){
		c= &C;
		bufsize= 1;
	}
	else{
		if( !(c= calloc( bufsize, 1)) ){
			perror( "xpipe: can't get buffer memory");
			return(-2);
		}
		clean_up( 0, c);
	}
	signal( SIGHUP, handle_sighup);
	signal( SIGINT, handle_sigint);
	sleep(1);
	read( fd, &Pid, sizeof(pid_t));
	if( Pid== pid)
		while( Pid== pid){
			write( fo, &Pid, sizeof(pid_t));
			sleep(1);
			read( fd, &Pid, sizeof(pid_t));
		}
	putw( Pid, stdout);

	while( read( fd, c, bufsize) ){
		fputs( c, stdout);
		fflush( stdout);
	}
	clean_up(1);
	return( 0);
}


main( argc, argv)
int argc;
char **argv;
{ char *geo= NULL;
  int i, bufsize= PIPE_MAX;

	pid= getpid();
	for( i= 1; i< argc; i++){
		if( !strcmp( argv[i], "-fildes") ){
			if( ++i< argc){
				if( sscanf( argv[i], "%d,%d", &fd, &fo)!= 2)
					perror( "xpipe (fildes mode):");
				else{
					setsid();
				}
			}
			else{
				fprintf( stderr, "xpipe: nead 2 pipe file descriptors after -fildes in,out\n");
				exit(-1);
			}
		}
		else if( !strcmp( argv[i], "-bufsize") ){
			if( ++i< argc){
				if( sscanf( argv[i], "%d", &bufsize)!= 1 || bufsize< 0 )
					fprintf( stderr, "xpipe: invalid buffersize after -bufsize (using %d)\n",
						(bufsize= PIPE_MAX)
					);
			}
			else{
				fprintf( stderr, "xpipe: nead a buffersize after -bufsize (using %d)\n",
					(bufsize= PIPE_MAX)
				);
			}
		}
	}

	if( fd>= 0){
		write( fo, &pid, sizeof(pid_t) );
		exit( read_pipe(fo, fd, bufsize) );
	}
	else{
		fprintf( stderr, "xpipe: usage xpipe [-bufsize #] -fildes <fd>\n");
		exit( -1);
	}
}
