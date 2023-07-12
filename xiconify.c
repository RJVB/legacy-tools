#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <local/Macros.h>

IDENTIFY( "Small utility to (de)iconify X11 windows");

extern int errno;
#if !defined(linux) && !defined(__MACH__) && !defined(__APPLE_CC__)
extern char *sys_errlist[];
#endif


int main( int argc, char **argv)
{  char *displayname= NULL;
   extern char *getenv();
   int i;
   Display *display;
   int deicon= 0;

	if( !(displayname= getenv("DISPLAY")) ){
		fprintf( stderr, "%s: no display specified on commandline or in environment\n", argv[0]);
		exit(-1);
	}
	if( argc== 1 ){
		fprintf( stderr, "%s [-d|-i] resource_id [...]\n", argv[0] );
		fprintf( stderr, "\t-d: deiconify\n\t-i: iconify\n\totherwise, toggle.\n" );
	}
	if( !(display= XOpenDisplay( displayname)) ){
		if( errno){
			fprintf( stderr, "%s: can't open display '%s' (%s)\n",
				argv[0], displayname, sys_errlist[errno]
			);
		}
		else{
			fprintf( stderr, "%s: can't open display '%s'\n",
				argv[0], displayname
			);
		}
		fflush( stderr);
		exit(-1);
	}

	for( i= 1; i< argc; i++ ){
	  Window xid;
		if( strcmp( argv[i], "-d" ) == 0 ){
			deicon= 1;
		}
		else if( strcmp( argv[i], "-i" ) == 0 ){
			deicon= 0;
		}
		else if( sscanf( argv[i], "0x%x", &xid )== 1 || sscanf( argv[i], "%lu", &xid )== 1 ){
			if( deicon ){
				XMapRaised( display, xid );
			}
			else{
				XIconifyWindow( display, xid, DefaultScreen(display) );
			}
		}
	}
	XCloseDisplay( display);
	exit(0);
}
