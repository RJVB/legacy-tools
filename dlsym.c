#include "local/Macros.h"
IDENTIFY("dlsym: see man dlsym(3)");

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>

#ifdef linux
#define __USE_GNU
#endif
#include <dlfcn.h>

#include <string.h>

#if !defined(linux) && !defined(__MACH__)
extern char *sys_errlist[];
#endif
extern char *getenv();
extern int errno, system();

int main( int argc, char *argv[] )
{ int i, r= 1;
  void *lib;
#if defined(__MACH__) || defined(__APPLE_CC__) || defined(__USE_GNU)
  Dl_info info;
#endif
	char *v;
	if( (v = getenv("DLSYM_VERBOSE")) ){
		putenv("DYLD_PRINT_LIBRARIES_POST_LAUNCH=1");
		putenv("DYLD_BIND_AT_LAUNCH=1");
		putenv("LD_BIND_NOW=1");
		if (atoi(v) > 1 ) {
			putenv("DYLD_PRINT_INITIALIZERS=1");
			putenv("DYLD_PRINT_APIS=1");
			putenv("DYLD_PRINT_RPATHS=1");
			putenv("LD_DEBUG=files");
		}
		unsetenv("DLSYM_VERBOSE");
		fprintf(stderr, "relaunching %s\n", argv[0]);
		execvp(argv[0], argv);
		perror("oops");
		exit(-1);
	}
	if( argc> 1 ){
	  char *err;
		lib= dlopen((strlen(argv[1]))? argv[1] : NULL, RTLD_LOCAL|RTLD_NOW);
		err= (char*) dlerror();
		if( lib && !err ){
			for( i= 2; i< argc; i++ ){
			  void *sym= dlsym( lib, argv[i] );
				err= (char*) dlerror();
				if( sym && !err ){
					fprintf( stderr, "%s::%s: %p", argv[1], argv[i], sym );
#if defined(__MACH__) || defined(__APPLE_CC__) || defined(__USE_GNU)
					if( sym ){
						if( dladdr( sym, &info) ){
							fprintf( stderr, " (%s::%s)", info.dli_fname, info.dli_sname );
						}
					}
#endif
					fputc( '\n', stderr );
				}
				else{
					fprintf( stderr, "%s: error retrieving %s::%s (%s)\n",
						argv[0], argv[1], argv[i], err
					);
				}
			}
			dlclose(lib);
			err= (char*) dlerror();
			if( err ){
				fprintf( stderr, "%s: error closing %s (%s)\n",
					argv[0], argv[1], err
				);
			}
			else{
				r= 0;
			}
		}
		else{
			fprintf( stderr, "%s: can't open %s (%s)\n",
				argv[0], argv[1], err
			);
		}
	}
	exit(r);
}
