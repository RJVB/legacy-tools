/*
vi:set ts=4|set sw=4:
:ts=4
*/
/* 
 \ lock_term.c : simple utility for locking a (single) terminal.
 \ lock_term repeatedly calls the routine autolock(), which reads
 \ raw characters until a '\r' or '\n' character. Then it encrypts
 \ the buffer (with crypt()), and compares the result to 1) the calling
 \ user's password or 2) the superuser's password. If somehow those passwords
 \ couldn't be obtained, and an internal password is compiled in (INTERNALPW),
 \ this is used, otherwise the programme exits.
 \ Invalid passwords are logged using the syslog() call.
 \ SIGINT is ignored.
 \ After entering a valid password, the programme either exits, or, if
 \ -exec is given as the first (and only supported) argument, it tries
 \ to exec the user's shell (determined from $SHELL).
 \ Thus the command
 \ > exec lock_term -exec
 \ locks the terminal, starting a new shell if a correct password is entered,
 \ and releasing the line when lock_term exits otherwise. It is equivalent to
 \ the command
 \ > exec su $LOGNAME -c "exec $SHELL"
 */

#ifndef _STDIO_H
#	include "stdio.h"
#define _STDIO_H
#endif
#ifndef _TIME_H
#include <sys/types.h>
#include <sys/times.h>
#include <time.h>
#define _TIME_H
#endif

#include <ctype.h>
#include <termio.h>
#include <sys/ioctl.h>
/* #include <stdarg.h>	*/
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
/* #include <curses.h>	*/

#ifndef __sys_types_h
#	include <sys/types.h>
#endif

#include <unistd.h>
#include <pwd.h>
#include <syslog.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>

extern int errno;
extern char *sys_errlist[];

int _console_cursesmode= 0;
static unsigned char __cns_curses= 0;
static unsigned char cns_cc4, cns_cc5;
static unsigned short cns_cflag, cns_lflag, cns_iflag, cns_oflag;

char *ProgramName= "";

#define	str(name)	# name
#define	STR(name)	str(name)

int CX_set_nocurses()
{
	struct termio in_io;
	unsigned char *c_cc4= &in_io.c_cc[4];
	static char called= 0;

	if( _console_cursesmode== -1)
		return(0);
	if( _console_cursesmode && __cns_curses){
		ioctl( fileno(stdin), TCGETA, &in_io);
		in_io.c_lflag= cns_lflag;
		in_io.c_cflag= cns_cflag;
		in_io.c_iflag= cns_iflag;
		in_io.c_oflag= cns_oflag;
		*c_cc4++= cns_cc4;
		*c_cc4= cns_cc5;
		if( ioctl( fileno(stdin), TCSETA, &in_io)){
			if( !called){
				perror( "set_nocurses(): error resetting");
				fputs(  "               this message will not be repeated\n", stderr);
				fflush( stderr);
				called= 1;
			}
			if( errno== ENOTTY)
				_console_cursesmode= -1;
		}
		_console_cursesmode= 0;
		__cns_curses= 0;
		return( 1);
	}
	return(0);
}

int CX_set_curses()
{
	struct termio in_io;
	unsigned char *c_cc4= &in_io.c_cc[4];
	static char called= 0;

	if( _console_cursesmode== -1)
		return(0);
	if( !_console_cursesmode && !__cns_curses ){
		ioctl( fileno(stdin), TCGETA, &in_io);

		/* remember current settings	*/
		cns_cflag= in_io.c_cflag;
		cns_lflag= in_io.c_lflag;
		cns_iflag= in_io.c_iflag;
		cns_oflag= in_io.c_oflag;
		cns_cc4= *c_cc4;
			*c_cc4++= 1;
		cns_cc5= *c_cc4;
			*c_cc4= 0;

		/* set new:	*/
		in_io.c_cflag&= ~(CLOCAL|PARENB|HUPCL);
		in_io.c_lflag&= ~(ICANON | ECHO);
		in_io.c_iflag&= ~ICRNL;	/* ~(BRKINT|IGNPAR|ISTRIP);	*/
/*		in_io.c_oflag&= ~ONLCR;	*/
		if( ioctl( fileno(stdin), TCSETA, &in_io)== -1){
			if( !called){
				perror( "set_curses(): error setting");
				fputs(  "               this message will not be repeated\n", stderr);
				fflush( stderr);
				called= 1;
			}
			if( errno== ENOTTY)
				_console_cursesmode= -1;
		}
		_console_cursesmode= 1;
		__cns_curses= 1;
		return( 1);
	}
	return( 0);
}

#define set_curses	CX_set_curses
#define set_nocurses	CX_set_nocurses

char Cnecin()
{	char c;
	int ccr;

	ccr= set_curses();
	
 	if( !read( fileno(stdin), &c, 1) )
		c= '\0';

	if( ccr)
		set_nocurses();

	return( c);
}

extern char *crypt(), *strdup();

/* password routine called by LockScreen()	*/
int auto_lock(struct passwd *pw, struct passwd *rpw)
{
  int done= 0, l, len= 8;
  char c, *rsrpp= 0, *srpp = 0, pwbuf[32];

      /* Save the user's password	*/
    if( pw!= 0 ){
		srpp = pw->pw_passwd;
    }
      /* The root password:	*/
    if( rpw!= 0 ){
		rsrpp = rpw->pw_passwd;
    }

    if( !srpp && !rsrpp ){
	    len= 31;
	}
    {
     char *rcrpp, *crpp;
		pwbuf[0]= '\0';
		l= 0;
		done= 0;
		  /* event-loop that reads keyboard input into the password buffer */
		do{
			c= Cnecin();
			  /* ^u clears	*/
			if( c== ('u'-'a'+1) ){
				l= 0;
			}
			else if( c== '\n' || c== '\r' ){
				done= 1;
			}
			else{
				pwbuf[l]= c;
				if( l< len ){
					l++;
				}
			}
			pwbuf[l]= '\0';
		} while( !done );
		  /* First check the user's own password. If correct, return	*/
		if( srpp || rsrpp ){
		  char *msg= NULL, *rmsg= NULL;
			if( !pwbuf[0] ){
				msg= "empty pw";
			}
			if( srpp ){
				crpp = crypt( pwbuf ,   srpp ) ;
				if (strcmp(crpp, srpp) == 0) {
					return(0);
				}
				if( !msg ){
					msg= "invalid user pw";
				}
			}
			  /* Hmm. Maybe it is root releasing locked-up user?	*/
			if( rsrpp ){
				rcrpp = crypt( pwbuf ,   rsrpp ) ;
				if( strcmp( crpp, rsrpp)== 0 ){
					return(0);
				}
				rmsg= ", invalid root pw";
			}
			if( pwbuf[0] ){
				syslog( LOG_WARNING, "%s[%d]: illegal password for user %s (or %s): %s%s\n",
					ProgramName, getpid(),pw->pw_name, rpw->pw_name,
					(msg)? msg : "",
					(rmsg)? rmsg : ""
				);
			}
		}
#ifdef INTERNALPW
		  /* Couldn't get a password to prompt for. We just use an internal password.	*/
		else{
			if( strncmp( pwbuf, INTERNALPW, len)== 0 ){
				return(0);
			}
		}
#else
		  /* Couldn't get a password to prompt for. Exit with an error message	*/
		else{
			fprintf( stderr, "No password to check for - unable to lock terminal\n" );
			return(0);
		}
#endif
		  /* What are we doing here!! Someone trying to break-in???	*/
		fprintf( stderr, "illegal password\n");
	}
	return(1);
}

main( int argc, char *argv[] )
{
    int euid= (int) geteuid();
    struct passwd *pw, *rpw, PW, rPW;
    char *nm= NULL;
    extern char *getlogin();


    ProgramName= argv[0];
    signal( SIGINT, SIG_IGN);

	PW.pw_passwd= NULL;
	rPW.pw_passwd= NULL;

    pw= getpwuid( euid );

    if( pw ){
	    PW.pw_name= strdup(pw->pw_name);
	    PW.pw_passwd= strdup(pw->pw_passwd);
	    nm= PW.pw_name;
	}
	else{
	  char *c= getlogin();
		nm= (c)? strdup(c) : NULL;
		PW.pw_name= nm;
#ifdef INTERNALPW
		fprintf( stderr, "%s: can't get your password (%s) - using internal\n", ProgramName, sys_errlist[errno] );
#else
		fprintf( stderr, "%s: can't get your password (%s)\n", ProgramName, sys_errlist[errno] );
#endif
	}
	rpw= getpwuid( 0);
	if( rpw ){
	    rPW.pw_name= strdup(pw->pw_name);
	    rPW.pw_passwd= strdup(pw->pw_passwd);
	}
	
    do{
	    fprintf( stderr, "Enter password for %s\n", (nm)? nm : (PW.pw_name= "(unknown)") );
		 /* Call the function that reads/checks the password	*/
    }
    while( auto_lock( &PW, &rPW ) );
	if( PW.pw_passwd ){
		free( PW.pw_name );
		free( PW.pw_passwd );
	}
	if( rPW.pw_passwd ){
		free( rPW.pw_name );
		free( rPW.pw_passwd );
	}
	if( argc> 1 && !strcmp( argv[1], "-exec" ) ){
	  extern char *getenv();
	  char *c= getenv("SHELL"), *shell;
		if( c ){
			shell= strdup(c);
			fprintf( stderr, "execl(%s,%s,0)\n", shell, shell );
			execl( shell, shell, 0);
		}
		else{
			fprintf( stderr, "Can't get $SHELL - exitting");
		}
	}
	exit(0);
}
