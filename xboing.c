#include <stdio.h>
#include <errno.h>

#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <local/Macros.h>

IDENTIFY( "Small utility to ring an X11 server's bell");

extern int errno;
#if !defined(linux) && !defined(__MACH__) && !defined(__APPLE_CC__)
extern char *sys_errlist[];
#endif


main( int argc, char **argv)
{  char *displayname= NULL;
   extern char *getenv();
   int i;
   Display *display;
   XKeyboardState kbstate;
   unsigned long vmask= 0;
   XKeyboardControl kbc;
   int duration;

	if( !(displayname= getenv("DISPLAY")) ){
		fprintf( stderr, "%s: no display specified on commandline or in environment\n", argv[0]);
		exit(-1);
	}
	if( argc== 1 ){
		fprintf( stderr, "%s [-p pitch] [-d duration] percent [[-p pitch] [-d duration] percent [...] ]\n", argv[0] );
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

	  /* obtain entry settings: */
	XGetKeyboardControl( display, &kbstate );
	duration= kbstate.bell_duration;
	for( i= 1; i< argc; i++ ){
	 int percent;
		if( strcmp( argv[i], "-p" ) == 0 ){
		  int pitch;
			i+= 1;
			if( sscanf(argv[i], "%d", &pitch )== 1 ){
				vmask|= KBBellPitch;
				kbc.bell_pitch= pitch;
			}
		}
		else if( strcmp( argv[i], "-d" ) == 0 ){
			i+= 1;
			if( sscanf(argv[i], "%d", &duration )== 1 ){
				vmask|= KBBellDuration;
				kbc.bell_duration= (duration< 0)? (duration=kbstate.bell_duration) : duration;
			}
		}
		else if( sscanf( argv[i], "%d", &percent )== 1 ){
			if( vmask ){
				XChangeKeyboardControl( display, vmask, &kbc );
			}
			XBell( display, percent );
			XFlush(display);
			  /* wait for the specified duration, so that we can execute xboing commands in series;
			   \ the XBell command is asynchronous, and even XSync() returns before (long) bells have
			   \ finished.
			   */
			usleep( duration*1000 );
		}
	}
	  /* restore entry settings: */
	kbc.bell_percent= kbstate.bell_percent;
	kbc.bell_pitch= kbstate.bell_pitch;
	kbc.bell_duration= kbstate.bell_duration;
	vmask = KBBellPercent|KBBellPitch|KBBellDuration;
	XChangeKeyboardControl( display, vmask, &kbc );
	XCloseDisplay( display);
	exit(0);
}
