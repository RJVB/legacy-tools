#include <stdio.h>
#include <errno.h>

#include <stdarg.h>

#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <local/Macros.h>

IDENTIFY( "Small utility to copy stdin to the X11 clipboard");

extern int errno;
#if !defined(linux) && !defined(__MACH__) && !defined(__APPLE_CC__)
extern char *sys_errlist[];
#endif


/* concat2(): concats a number of strings; reallocates the first, returning a pointer to the
 \ new area. Caution: a NULL-pointer is accepted as a first argument: passing a single NULL-pointer
 \ as the (1-item) arglist is therefore valid, but possibly lethal!
 */
char *concat2(char *string, ...)
{ va_list ap;
  int n= 0, tlen= 0;
  char *c= string, *buf= NULL, *first= NULL;
	va_start(ap, string);
	do{
		if( !n ){
			first= c;
		}
		if( c ){
			tlen+= strlen(c);
		}
		n+= 1;
	}
	while( (c= va_arg(ap, char*)) != NULL || n== 0 );
	va_end(ap);
	if( n== 0 || tlen== 0 ){
		return( NULL );
	}
	tlen+= 4;
	if( first ){
		buf= realloc( first, tlen* sizeof(char) );
	}
	else{
		buf= first= calloc( tlen, sizeof(char) );
	}
	if( buf ){
		va_start(ap, string);
		  /* realloc() leaves the contents of the old array intact,
		   \ but it may no longer be found at the same location. Therefore
		   \ need not make a copy of it; the first argument can be discarded.
		   \ strcat'ing is done from the 2nd argument.
		   */
		while( (c= va_arg(ap, char*)) != NULL ){
			strcat( buf, c);
		}
		va_end(ap);
		buf[tlen-1]= '\0';
	}
	return( buf );
}

main( int argc, char **argv)
{  char *displayname= NULL;
   extern char *getenv();
   int i, bufferID=-1;
   Display *display;
   char *progname= argv[0], *contents= NULL, buf[1024];
   FILE *in= stdin, *out= stdout;

	if( !(displayname= getenv("DISPLAY")) ){
		fprintf( stderr, "%s: no display specified on commandline or in environment\n", argv[0]);
		exit(-1);
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
	{ char *bid= getenv("XCUTBUFFER");
		if( bid && atoi(bid)>=0 ){
			bufferID= atoi(bid);
		}
	}

	if( strcmp( basename(progname), "pbxcopy" ) == 0 ){
	  char *command= NULL;
		progname= "xpaste";
		command= concat2( command, "pbcopy ", NULL );
		i= 1;
		while( argc> 1 ){
			command= concat2( command, argv[i++], NULL );
			argc--;
		}
		if( command ){
// 			fprintf( stderr, "%s | %s\n", progname, command );
			if( !(out= popen( command, "w" )) ){
				fprintf( stderr, "Error: cannot open pipe to '%s'\n", command );
			}
			free(command);
		}
		else{
			fprintf( stderr, "Error: can't construct command to pipe to (%s)\n", serror() );
			out= NULL;
		}
	}
	else if( strcmp( basename(progname), "pbxpaste" ) == 0 ){
	  char *command= NULL;
		progname= "xcopy";
		command= concat2( command, "pbpaste ", NULL );
		i= 1;
		while( argc> 1 ){
			command= concat2( command, argv[i++], NULL );
			argc--;
		}
		if( command ){
// 			fprintf( stderr, "%s | %s\n", command, progname );
			if( !(in= popen( command, "r" )) ){
				fprintf( stderr, "Error: cannot open pipe from '%s'\n", command );
			}
			free(command);
		}
		else{
			fprintf( stderr, "Error: can't construct command to pipe from (%s)\n", serror() );
			in= NULL;
		}
	}
	if( strcmp( basename(progname), "xcopy" ) == 0 && in ){
xcopy:;
// 		while( fgets(buf, sizeof(buf)/sizeof(char), in) && !feof(in) && !ferror(in) ){
// 			fprintf( stderr, "%s\n", buf ); fflush(stderr);
// 			contents= concat2( contents, buf, NULL );
// 		}
		while( (buf[0]= fgetc(in))!= EOF && !feof(in) && !ferror(in) ){
// 			fprintf( stderr, "%c", buf[0] ); fflush(stderr);
			buf[1]= '\0';
			contents= concat2( contents, buf, NULL );
		}
		if( contents ){
			if( getenv("XCUTVERBOSE") ){
				fprintf( stderr, "%s", contents );
			}
			if( bufferID< 0 ){
				XStoreBytes( display, "", 0 );
				XStoreBytes( display, contents, strlen(contents) );
			}
			else{
				XStoreBuffer( display, "", 0, bufferID );
				XStoreBuffer( display, contents, strlen(contents), bufferID );
			}
			XSync( display, False );
			free(contents);
		}
	}
	else if( out ){
xpaste:;
	  int N= 0;
		if( bufferID < 0 ){
			contents= XFetchBytes( display, &N );
		}
		else{
			contents= XFetchBuffer( display, &N, bufferID );
		}
		if( contents ){
			if( out!= stdout && getenv("XCUTVERBOSE") ){
				fprintf( stderr, "%s", contents );
			}
			fprintf( out, "%s", contents );
			XFree(contents);
		}
	}

	if( in!= stdin ){
		pclose(in);
	}
	if( out!= stdout ){
		pclose(out);
	}

	XCloseDisplay( display);
	exit(0);
}
