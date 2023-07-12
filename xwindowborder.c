#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <local/Macros.h>

IDENTIFY( "Small utility to set X11 window border colour");

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
		fprintf( stderr, "%s resource_id colour\n", argv[0] );
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

	if( argc > 2 ){
	  Window xid;
	  XColor exact, screen;
		if( sscanf( argv[1], "0x%x", &xid )== 1 || sscanf( argv[1], "%lu", &xid )== 1 ){
			if( XLookupColor( display, DefaultColormap( display, DefaultScreen(display)), argv[2], &exact, &screen ) ){
				XSetWindowBorder( display, xid, screen.pixel );
				fprintf( stderr, "\"%s\" = #%04hx%04hx%04hx exact, #%04hx%04hx%04hx on screen\n",
					argv[2],
					exact.red, exact.green, exact.blue,
					screen.red, screen.green, screen.blue
				);
			}
			else{
				fprintf( stderr, "XLookupColor failed for \"%s\"\n", argv[2] );
			}
		}
	}
	XCloseDisplay( display);
	exit(0);
}
