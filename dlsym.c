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

#ifdef linux
#ifndef HIJACK_LIB
#	define __RTLD_OPENEXEC 0x20000000
#endif
#else
#ifdef HIJACK_LIB
#	error "-DHIJACK_LIB is supported on Linux only!"
#endif
#endif

#include <string.h>

#if !defined(linux) && !defined(__MACH__)
extern char *sys_errlist[];
#endif
extern char *getenv();
extern int errno, system();

static const char *self = "libdlsym_hijack";

void usage()
{
#ifndef HIJACK_LIB
	fprintf(stderr, "%s: Load a library and optionally check if it provides one or more symbols\n", self);
	fprintf(stderr, "\nUsage:\n");
	fprintf(stderr, "%s [-i inject1] [[-i inject2] ...] library [symbol(s)]\n", self);
	fprintf(stderr, "\t-i prereq : inject/insert/preload library `prereq`, making its symbols available to the test `library`\n");
#ifdef __MACH__
	fprintf(stderr, "\tInjection probably requires setting DYLD_FORCE_FLAT_NAMESPACE=1 on Mac/Darwin.\n");
#endif
	fprintf(stderr, "\tSet DLSYM_VERBOSE=1 to trace libraries being loaded, or DLSYM_VERBOSE=2 for more information.\n");
#else
	fprintf(stderr, "%s: hijack an executable and optionally check if it if can access one or more symbols (and via/from which shared library)\n", self);
	fprintf(stderr, "\nUsage:\n");
	fprintf(stderr, "env LD_PRELOAD=%s executable [-i inject1] [[-i inject2] ...] [symbol(s)]\n", self);
	fprintf(stderr, "\t-i prereq : inject/insert/preload library `prereq`, making its symbols available to the test `library`\n");
	fprintf(stderr, "\tSet DLSYM_VERBOSE=1 to trace libraries being loaded, or DLSYM_VERBOSE=2 for more information.\n");
#endif
}

#ifndef HIJACK_LIB
int main( int argc, char *argv[] )
#else
int __libc_start_main(int (*main) (int, char **, char **), int argc, char *argv[])
#endif
{ int i, r = 1, min_argc, first_symbol;
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
#ifndef HIJACK_LIB
	self = basename(argv[0]);
	min_argc = 1;
#else
	min_argc = 0;
#endif
	if( argc > min_argc ){
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
#ifndef HIJACK_LIB
		if (i < argc) {
			first = i;
		} else {
			usage();
			exit(-1);
		}
		first_symbol = first + 1;
#else
		// the first argument (= the file to dlopen) is the executable we're launching, so argv[0]
		first = 0;
		if (i < argc || argc == 1) {
			first_symbol = i;
		} else {
			usage();
			exit(-1);
		}
#endif

#ifndef HIJACK_LIB
		if (!strlen(argv[first])) {
			lib = dlopen(NULL, RTLD_LOCAL|RTLD_NOW);
			argv[first] = argv[0];
		} else {
			lib = dlopen(argv[first], RTLD_LOCAL|RTLD_NOW);
		}
#else
		// the "lib" we want is the current executable we're hijacking, which we can
		// get by handing NULL to dlopen
		lib = dlopen(NULL, RTLD_LOCAL|RTLD_NOW);
		fprintf(stderr, "## \"dlopened\" %s (lib=%p)\n", argv[first], lib);
		r = 0;
#endif
		err = (char*) dlerror();
#ifdef __RTLD_OPENEXEC
		if (!lib && err && strstr(err, "cannot dynamically load executable")) {
			// let's try to be naughty and see what happens!
			lib = dlopen((strlen(argv[first]))? argv[first] : NULL, RTLD_LOCAL|RTLD_NOW|__RTLD_OPENEXEC);
			err = (char*) dlerror();
			// dlclose is likely to sigsegv in this case
			r = 0;
		}
#endif
		if( lib && !err ){
			for( i = first_symbol; i< argc; i++ ){
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
			if ( r ) {
				dlclose(lib);
				err = (char*) dlerror();
				if( err ){
					fprintf( stderr, "%s: error closing %s (%s)\n",
						argv[0], argv[first], err
					);
				} else {
					r = 0;
				}
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
