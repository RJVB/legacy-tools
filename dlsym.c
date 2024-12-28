#include "local/Macros.h"
IDENTIFY("dlsym: see man dlsym(3)");

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <libgen.h>

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

static const char *self = NULL;

void usage()
{
	fprintf(stderr, "%s: Load a library and optionally check if it provides one or more symbols\n", self);
	fprintf(stderr, "\nUsage:\n");
	fprintf(stderr, "%s [-i inject1] [[-i inject 2] ...] library [symbol(s)]\n", self);
	fprintf(stderr, "\t-i inject : inject/insert/preload library `inject`, making its symbols available to the test `library`\n");
	fprintf(stderr, "\tInjection probably requires setting DYLD_FORCE_FLAT_NAMESPACE=1 on Mac/Darwin.\n");
	fprintf(stderr, "\tSet DLSYM_VERBOSE=1 to trace libraries being loaded, or DLSYM_VERBOSE=2 for more information.\n");
}

int main( int argc, char *argv[] )
{ int i, r = 1;
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
	self = basename(argv[0]);
	if( argc> 1 ){
	  char *err;
	  int first = 1, injected = 0;
		for (i = 1 ; i < argc && strncmp(argv[i], "-i", 2) == 0; ++i) {
			i += 1;
			if (i < argc) {
			  void *inject = dlopen(argv[i], RTLD_GLOBAL|RTLD_LAZY);
				err = (char*) dlerror();
				if (!inject || err) {
#ifdef __APPLE__
					fprintf(stderr, "Ignoring error injecting via %s\n", err);
#else
					fprintf(stderr, "Ignoring error injecting %s\n", err);
#endif
				} else {
					injected += 1;
				}
			} else {
				fprintf(stderr, "Error: invalid argument \"%s\"\n", argv[i-1]);
				usage();
				exit(-1);
			}
		}
		if (i < argc) {
			first = i;
		} else {
			usage();
			exit(-1);
		}
		lib = dlopen((strlen(argv[first]))? argv[first] : NULL, RTLD_LOCAL|RTLD_NOW);
		err = (char*) dlerror();
		if( lib && !err ){
			for( i = first + 1; i< argc; i++ ){
			  void *sym = dlsym( (injected)? RTLD_DEFAULT : lib, argv[i] );
				err = (char*) dlerror();
				if( sym && !err ){
					fprintf( stderr, "%s::%s: %p", argv[first], argv[i], sym );
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
						argv[0], argv[first], argv[i], err
					);
				}
			}
			dlclose(lib);
			err = (char*) dlerror();
			if( err ){
				fprintf( stderr, "%s: error closing %s (%s)\n",
					argv[0], argv[first], err
				);
			}
			else{
				r = 0;
			}
		}
		else{
			fprintf( stderr, "%s: can't open %s (%s)\n",
				argv[0], argv[first], err
			);
		}
	} else {
		usage();
	}
	exit(r);
}
