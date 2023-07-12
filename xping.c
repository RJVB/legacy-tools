#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Intrinsic.h>

#include <sys/types.h>
#include <sys/times.h>
#include <sys/time.h>
#include <time.h>

#include <local/Macros.h>

IDENTIFY( "Small utility to ping an X11 server (test its presence)");

// see if xcb_util.c from libxcb can be adapted to implement a minimal
// "try to connect to server with timeout" test.

extern int errno;
#if !defined(linux) && !defined(__MACH__) && !defined(__APPLE_CC__) && !defined(__CYGWIN__)
extern char *sys_errlist[];
#endif

#ifdef _AUX_SOURCE
#	define CLK_TCK	60.0
#	define POLLS 500
#else
#	define POLLS 5000
#endif

/* struct tms tms;	*/
/* struct tms *Tms= &tms;	*/
/* double Then, Now;	*/
/* #define Set_Time	{Then=(double) times(Tms);}	*/
/* #define Add_Time(x)	{Now= (double) times(Tms); (x)+= (Now- Then)/CLK_TCK; Then= Now; }	*/


typedef struct __Time_Struct2{
	struct timeval prev_tv, Tot_tv;
	  /* the total time since (re)initialisation of this timer:	*/
	double Tot_T;
	Boolean do_reset;
} __Time_Struct2;

static struct timeval ES_tv;
#define __Elapsed_Since2(then,reset)	\
{ double Delta_Tot_T;	\
  struct timezone tzp;	\
\
	gettimeofday( &ES_tv, &tzp );	\
	Delta_Tot_T= (ES_tv.tv_sec - then->prev_tv.tv_sec) + (ES_tv.tv_usec- then->prev_tv.tv_usec)/ 1e6;	\
	then->prev_tv= ES_tv;	\
	if( reset || then->do_reset ){	\
		then->Tot_T= 0.0;	\
		then->Tot_tv= ES_tv;	\
		then->do_reset= False;	\
	}	\
	else{	\
		then->Tot_T= (ES_tv.tv_sec - then->Tot_tv.tv_sec) + (ES_tv.tv_usec- then->Tot_tv.tv_usec)/ 1e6;	\
	}	\
}

#define Set_Time	__Elapsed_Since2( (&timer), True)
#define Add_Time(x)	{__Elapsed_Since2((&timer), False); (x)= timer.Tot_T; }

main( int argc, char **argv)
{  char *displayname= NULL;
   extern char *getenv();
   int i= 1, j;
   __Time_Struct2 timer;
   double stime, time, time_correction= 0.0, stime_correction= 0.0;

	if( argc< 2)
		if( !(displayname= getenv("DISPLAY")) ){
			fprintf( stderr, "%s: no display specified on commandline or in environment\n", argv[0]);
			exit(-1);
		}

	while( i< argc || (i==1 && argc==1) ){
	  Display *display = NULL, *Display;
	  int quiet= 0;
	  int countdown = 0;
		if( i< argc){
			for( ; i< argc; i++ ){
				if( !strcmp( "-q", argv[i]) ){
					quiet= 1;
				}
				else if( !strcmp( "-downToFree", argv[i] ) ){
					countdown = 1;
				}
				else{
					displayname= argv[i];
				}
			}
			if( i< argc ){
				displayname= argv[i];
			}
			else if( !displayname ){
				displayname= getenv( "DISPLAY" );
			}
		}
		if( countdown && displayname ){
		  int found = 0, d, s=0;
		  char *colon;
		  char *ndisplay, *base;
			if( (colon = strchr(displayname, ':')) ){
				ndisplay = strdup(displayname);
				*colon = '\0';
				base = strdup(displayname);
				*colon = ':';
				if( sscanf( colon, ":%d.%d", &d, &s ) >= 1 ){
					fprintf( stderr, "Checking down from display %d (i.e. %d) to find lowest free server '%s:N.%d'\n",
						d, d-1, base, s
					);
					d -= 1;
					while( d >= 0 && !found ){
						sprintf( ndisplay, "%s:%d.%d", base, d, s );
						if( (display = XOpenDisplay(ndisplay)) ){
							XCloseDisplay(display);
							display = NULL;
							found = 1;
							fprintf( stdout, "%s:%d.%d\n", base, d+1, s );
						}
						else{
							d -= 1;
						}
					}
					if( !found && d <= 0 ){
						//fprintf( stdout, "%s:0.%d", base, s );
					}
					free(base);
					free(ndisplay);
				}
			}
			else{
				fprintf( stdout, "%s", displayname );
			}
		}
		else if( !(display= XOpenDisplay( displayname)) ){
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
			exit(i);
		}
		else if( !quiet ){
			fprintf( stdout, "%s: display '%s' ok\n", argv[0], displayname);
			fflush( stdout);
			if( Display= XOpenDisplay("unix:0") ){

				XSynchronize(display, False);
				Set_Time;
				for( time_correction= 0.0, j= 0; j< POLLS; j++){
					XNoOp(Display);
				}
				Add_Time(time_correction);
				XSynchronize(Display, True);
				Set_Time;
				for( stime_correction= 0.0, j= 0; j< POLLS; j++){
					XNoOp(Display);
				}
				Add_Time(stime_correction);
				XCloseDisplay(Display);
				fprintf( stderr, "%s: %d local (unix:0) POLLS: %lf seconds (%lf synchronous)\n",
					argv[0], POLLS, time_correction/(double)POLLS, stime_correction/(double)POLLS
				);

				fprintf( stdout, "\t:Response over %d POLLS: ", POLLS);
				fflush( stdout);
				XSynchronize( display, False);
				Set_Time;
				for( time= 0.0, j= 0; j< POLLS; j++){
					XNoOp(display);
				}
				Add_Time(time);
				XSynchronize( display, True);
				Set_Time;
				for( stime= 0.0, j= 0; j< POLLS; j++){
					XNoOp(display);
				}
				Add_Time(stime);
/* 				time-= time_correction;	*/
/* 				stime-= stime_correction;	*/
				fprintf( stdout, "%lf seconds (%lf synchronous) [%lf,%lf corrected]\n",
					time/ POLLS, stime/ POLLS,
					(time- time_correction)/ POLLS, (stime- stime_correction)/ POLLS
				);
				fflush( stdout);
			}
		}
		if( display ){
			XCloseDisplay(display);
		}
		i++;
	}
	exit(0);
}
