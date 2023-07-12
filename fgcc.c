#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <sys/param.h>
#include <sys/stat.h>

#include <errno.h>

#include <libgen.h>

#ifndef SWITCHES
#	ifdef DEBUG
#		define _IDENTIFY(s,i)	"$Id: @(#) '" __FILE__ "'-[" __DATE__ "," __TIME__ "]-(\015\013\t\t" s "\015\013\t) DEBUG version" i " $"
#	else
#		define _IDENTIFY(s,i)	"$Id: @(#) '" __FILE__ "'-[" __DATE__ "," __TIME__ "]-(\015\013\t\t" s "\015\013\t)" i " $"
#	endif
#else
  /* SWITCHES contains the compiler name and the switches given to the compiler.	*/
#	define _IDENTIFY(s,i)	"$Id: @(#) '" __FILE__ "'-[" __DATE__ "," __TIME__ "]-(\015\013\t\t" s "\015\013\t)["SWITCHES"] $"
#endif

#ifdef PROFILING
#	define __IDENTIFY(s,i)\
	static char *ident= _IDENTIFY(s,i);
#else
#	define __IDENTIFY(s,i)\
	static char *ident_stub(){ static char *ident= _IDENTIFY(s,i);\
		static char called=0;\
		if( !called){\
			called=1;\
			return(ident_stub());\
		}\
		else{\
			called= 0;\
			return(ident);\
		}}
#endif

#ifdef __GNUC__
#	define IDENTIFY(s)	__attribute__((used)) __IDENTIFY(s," (gcc)")
#else
#	define IDENTIFY(s)	__IDENTIFY(s," (cc)")
#endif


IDENTIFY( "An alternative to netlib's fc script that emulates a Fortran compiler using f2c and a C compiler. (C) 2009 RJVB" );

#ifdef DEBUG
	int debugFlag = 1;
#else
	int debugFlag = 0;
#endif

#define streq(a,b)	strcmp((a),(b))==0
#define strneq(a,b,n)	strncmp((a),(b),(((n)>=0)?(n):strlen((b))))==0


/* concat2(): concats a number of strings; reallocates the first, returning a pointer to the
 \ new area. Caution: a NULL-pointer is accepted as a first argument: passing a single NULL-pointer
 \ as the (1-item) arglist is therefore valid, but possibly lethal!
 */
char *concat2( char *target, ...)
{ va_list ap;
  int n= 0, tlen= 0;
  char *c, *buf= NULL, *first= NULL;
	va_start(ap, target);
	while( (c= va_arg(ap, char*)) != NULL || n== 0 ){
		if( !n ){
			first= target;
		}
		if( c ){
			tlen+= strlen(c);
		}
		n+= 1;
	}
	va_end(ap);
	if( n== 0 || tlen== 0 ){
		return( NULL );
	}
	tlen+= 4;
	if( first ){
		tlen+= strlen(first)+ 1;
		buf= realloc( first, tlen* sizeof(char) );
	}
	else{
		buf= first= calloc( tlen, sizeof(char) );
	}
	if( buf ){
	  int i= 0;
		va_start(ap, target);
		  /* realloc() leaves the contents of the old array intact,
		   \ but it may no longer be found at the same location. Therefore
		   \ need not make a copy of it; the first argument can be discarded.
		   \ strcat'ing is done from the 2nd argument.
		   */
		while( (c= va_arg(ap, char*)) != NULL ){
			if( strcmp(c, " ") || i || target ){
				strcat( buf, c);
			}
			i += 1;
		}
		va_end(ap);
	}
	return( buf );
}

#ifdef sgi
char *setenv( const char *n, const char *v, int overwrite_ignored )
{	static char *fail_env= "setenv=failed (no memory)";
	char *nv;

	if( !n ){
		return(NULL);
	}
	if( !v ){
		v= "";
	}
	if( !(nv= calloc( strlen(n)+strlen(v)+2, 1L)) ){
		putenv( fail_env);
		return( NULL);
	}
	sprintf( nv, "%s=%s", n, v);
	putenv( nv);
	return( getenv(n) );
}
#endif

#define xfree(x)	if((x)){ free((x)); (x)=NULL; }

#define SETTINGFILE	"fgcc-local-settings"
#define SETTINGS	"/usr/local/etc/"SETTINGFILE
#define CC		"gcc"

char *nnspace(char *c)
{
	while( c && *c && isspace(*c) ){
		c++;
	}
	return( c );
}

char *ignoreArgs[] = {
	"-fno-second-underscore",
	NULL
};

int ignoredArg(char *arg )
{ int ret= 0;
  char **ia = ignoreArgs;
	while( ia && *ia ){
		if( strneq(arg, *ia, -1) ){
			ret = 1;
			ia = NULL;
		}
		else{
			ia++;
		}
	}
	return( ret );
}

main( int argc, char *argv[] )
{ char *ecflags= getenv("CFLAGS");
  char *cflags= NULL, *f2cflags= NULL, *cppflags= NULL, *cflagsf2c= NULL;
  char *libs= NULL, *fglibs= NULL, *outo= NULL;
  char *versioncommand= NULL;
  char *ignoredArgs= NULL;
  char *files= NULL;
  char cc[MAXPATHLEN];
  FILE *fp= fopen( "./"SETTINGFILE, "r");
  char buf[MAXPATHLEN];
  char *self = basename(argv[0]);
  int link = 1, ret= -1, dashv= 0, printlibgcc= 0, dashversion= 0, dashV= 0;
  int needf2c= 1, Nlibs= 1, keepC= 0, regenerate= 0;

	if( strneq( self, "fgc", 3) || strneq(self, "fcc", 3) || strneq(self, "fg+", 3) ){
		strncpy( cc, &self[1], MAXPATHLEN );
	}
	else{
		strcpy( cc, CC );
	}

	if( !fp ){
		fp = fopen( SETTINGS, "r" );
	}
	if( fp ){
		while( fgets( buf, sizeof(buf), fp ) && !feof(fp) && !ferror(fp) ){
		  int l;
			if( buf[0]!= '#' ){
				if( buf[ (l= strlen(buf)-1) ]== '\n' ){
					buf[l]= '\0';
				}
				if( *buf ){
					if( strneq( buf, "CC=", 3) ){
					  char *c= nnspace( &buf[3] );
						if( c && *c ){
							strcpy( cc, c);
						}
					}
					else if( strneq( buf, "CFLAGS=", 7) ){
					  char *c= nnspace( &buf[7] );
						cflags= concat2( cflags, ((cflags)? " " : ""), c, NULL );
					}
					else if( strneq( buf, "F2CFLAGS=", 9) ){
					  char *c= nnspace( &buf[9] );
						f2cflags= concat2( f2cflags, ((f2cflags)? " " : ""), c, NULL );
					}
					else if( strneq( buf, "CPPFLAGS=", 9) ){
					  char *c= nnspace( &buf[9] );
						cppflags= concat2( cppflags, ((cppflags)? " " : ""), c, NULL );
					}
					else if( strneq( buf, "CFLAGSF2C=", 10) ){
					  char *c= nnspace( &buf[10] );
						cflagsf2c= concat2( cflagsf2c, ((cflagsf2c)? " " : ""), c, NULL );
					}
					else if( strneq( buf, "LIBS=", 5) ){
					  char *c= nnspace( &buf[5] );
						fglibs= concat2( fglibs, ((fglibs)? " " : ""), c, NULL );
					}
					else if( strneq( buf, "VERSIONCMD=", 11) ){
					  char *c= nnspace( &buf[11] );
						xfree(versioncommand);
						if( *c ){
							versioncommand= strdup(c);
						}
					}
					else if( strneq( buf, "IGNORE=", 7) ){
					  char *c= nnspace( &buf[7] );
						ignoredArgs= concat2( ignoredArgs, ((ignoredArgs)? " " : ""), c, NULL );
					}
					else if( strneq( buf, "KEEPC=", 6) ){
					  char *c= nnspace( &buf[6] );
						keepC = atoi(c);
					}
					else if( strneq( buf, "DEBUG=", 6) ){
					  char *c= nnspace( &buf[6] );
						debugFlag = atoi(c);
					}
					else if( strneq( buf, "REGENERATE=", 11) ){
					  char *c= nnspace( &buf[11] );
						regenerate = atoi(c);
					}
					else if( strneq( buf, "MACOSX_DEPLOYMENT_TARGET=", 25) ){
					  char *c= nnspace( &buf[25] );
						setenv( "MACOSX_DEPLOYMENT_TARGET", c, 1);
					}
				}
			}
		}
		fclose(fp);
	}
	if( ecflags && (!cflags || strcmp( cflags, ecflags)) ){
		cflags= concat2( cflags, ((cflags)? " " : ""), ecflags, 0 );
	}
	if( !f2cflags ){
		f2cflags= concat2( f2cflags, "-ARw8 -Nn802 -Nq300 -Nx400", NULL );
	}
	if( !cppflags ){
		cppflags= concat2( cppflags, "-I/usr/local/include", NULL );
	}
	if( !cflagsf2c ){
		cflagsf2c= concat2( cflagsf2c, "-I/usr/local/include", NULL );
	}
	if( !fglibs ){
		fglibs = concat2( fglibs, "-lf2c", NULL );
	}
	if( !ignoredArgs ){
		ignoredArgs= strdup("");
	}

	{ int i, skip= 0;

		if( debugFlag ){
			fprintf( stderr, "# %s", argv[0] );
		}
		for( i= 1; i< argc; i++ ){
			if( debugFlag ){
				fprintf( stderr, " %s", argv[i] );
			}
			if( skip > 0 ){
				skip -= 1;
			}
			else if( ignoredArg(argv[i]) || strstr(ignoredArgs, argv[i]) ){
				if( debugFlag ){
					fprintf( stderr, "[ignored]" );
				}
			}
			else if( streq(argv[i], "--debug") ){
				if( !debugFlag ){
				  int j;
					fprintf( stderr, "# %s", argv[0] );
					for( j= 1; j<= i; j++ ){
						fprintf( stderr, " %s", argv[j] );
					}
				}
				debugFlag = 1;
			}
			else if( streq(argv[i], "--keepC") ){
				keepC = !keepC;
			}
			else if( streq(argv[i], "--regenerate") ){
				regenerate = !regenerate;
			}
			else if( streq(argv[i], "-a")
				|| streq(argv[i], "-C")
				|| streq(argv[i], "-r8")
				|| streq(argv[i], "-u")
				|| streq(argv[i], "-w")
				|| streq(argv[i], "-P")
				|| strneq(argv[i], "-Nn", 3)
				|| strneq(argv[i], "-Nq", 3)
				|| strneq(argv[i], "-Nx", 3)
			){
				f2cflags = concat2( f2cflags, " ", argv[i], NULL );
			}
			else if( strneq(argv[i], "-I", -1) ){
				f2cflags = concat2( f2cflags, " ", argv[i], NULL );
				cppflags = concat2( cppflags, " ", argv[i], NULL );
				cflags = concat2( cflags, " ", argv[i], NULL );
			}
			else if( strneq(argv[i], "-D", -1)
				|| strneq(argv[i], "-U", -1)
			){
				cppflags = concat2( cppflags, " ", argv[i], NULL );
				cflags = concat2( cflags, " ", argv[i], NULL );
			}
			else if( streq(argv[i], "-v") ){
				cppflags = concat2( cppflags, " ", argv[i], NULL );
				cflags = concat2( cflags, " ", argv[i], NULL );
				dashv = 1;
			}
			else if( streq(argv[i], "-V") ){
				cppflags = concat2( cppflags, " ", argv[i], NULL );
				cflags = concat2( cflags, " ", argv[i], NULL );
				dashV = 1;
			}
			else if( streq(argv[i], "--version") ){
				cppflags = concat2( cppflags, " ", argv[i], NULL );
				cflags = concat2( cflags, " ", argv[i], NULL );
				dashversion = 1;
			}
			else if( streq(argv[i], "-print-libgcc-file-name") ){
				cppflags = concat2( cppflags, " ", argv[i], NULL );
				cflags = concat2( cflags, " ", argv[i], NULL );
				printlibgcc = 1;
			}
			else if( strneq(argv[i], "-g",-1) ){
				f2cflags = concat2( f2cflags, " ", "-g", NULL );
				cppflags = concat2( cppflags, " ", argv[i], NULL );
				cflags = concat2( cflags, " ", argv[i], NULL );
			}
			else if( strneq(argv[i], "-L", 2) ){
 				libs = concat2( libs, " ", argv[i], NULL );
// 				cflags = concat2( cflags, " ", argv[i], NULL );
			}
			else if( strneq(argv[i], "-l", 2) ){
 				libs = concat2( libs, " ", argv[i], NULL );
// 				cflags = concat2( cflags, " ", argv[i], NULL );
				Nlibs += 1;
			}
			else if( strneq(argv[i], "-framework", -1) ){
 				libs = concat2( libs, " ", argv[i], " ", argv[i+1], NULL );
// 				cflags = concat2( cflags, " ", argv[i], " ", argv[i+1], NULL );
				Nlibs += 1;
				skip = 1;
			}
			else if( strneq(argv[i], "-arch", -1)
				|| strneq(argv[i], "-isysroot", -1)
				|| strneq(argv[i], "-undefined", -1)
				|| strneq(argv[i], "-bundle", -1)
			){
				cflags = concat2( cflags, " ", argv[i], " ", argv[i+1], NULL );
				skip = 1;
			}
			else if( streq(argv[i], "-o") ){
				xfree(outo);
				skip = 1;
				outo= concat2( outo, argv[i+1], NULL );
			}
			else if( argv[i][0] == '-' ){
				// default: append to cflags!
				if( argv[i][1] == 'c' ){
					link = 0;
				}
				cflags = concat2( cflags, " ", argv[i], NULL );
			}
			else{
				if( !files ){
					files = concat2( files, argv[i], NULL );
					if( files[strlen(files)-1] != 'f' && files[strlen(files)-1] != 'F' ){
						needf2c = 0;
					}
				}
				else{
				  char *fname= argv[i];
					if( needf2c || fname[strlen(fname)-1] == 'f' || fname[strlen(fname)-1] == 'F' ){
						fprintf( stderr, "excess files ignored! (%s)\n", argv[i] );
					}
					else{
						files = concat2( files, " ", argv[i], NULL );
					}
				}
			}
		}
		if( debugFlag ){
			fputs( "\n", stderr );
		}
	}

	if( files || Nlibs > 1 ){
	  int Cisnewer= 0;
		if( cflags ){
			setenv( "CFLAGS", cflags, 1);
		}

		if( !files ){
		  // there's a possibility that everything thing we need (for linking) is contained in specified libraries.
		  // There's not really any way to know without trying...
			needf2c = 0;
		}

		if( *cc ){
		  char *command= NULL, *cname= NULL, *fname= NULL;

			if( needf2c ){
			  char *c = strrchr(files, '/');
				if( c ){
				  char cwd[MAXPATHLEN];
					*c = '\0';
					getwd(cwd);
#if 0
					if( outo ){
					  char *o= NULL;
						o = concat2( o, cwd, "/", outo, NULL );
						xfree(outo);
						outo = o;
					}
					if( debugFlag ){
						fprintf( stderr, "# entering %s\n", files );
					}
					chdir(files);
					fname = &c[1];
					*c = '/';
#else
					f2cflags = concat2(f2cflags, " -d ", files, NULL );
					*c = '/';
					fname = files;
#endif
				}
				else{
					fname = files;
				}
			}

			if( needf2c ){
			  int flen = strlen(fname);
				if( fname[flen-2] == '.' && (fname[flen-1] == 'f' || fname[flen-1] == 'F') ){
					fname[flen-2] = '\0';
					cname = concat2( cname, fname, ".c", NULL );
					fname[flen-2] = '.';
					command = concat2( command, "f2c", " ", f2cflags, " ", fname, NULL );
				}
				else{
					cname = concat2( cname, fname, ".c", NULL );
					command = concat2( command, "f2c", " ", f2cflags, " < ", fname, " > ", cname, NULL );
				}
			}
			else{
				cname = files;
			}
			if( needf2c && fname && cname ){
			  struct stat fstat, cstat;
				if( !stat(fname, &fstat) && !stat(cname, &cstat) ){
					if( cstat.st_mtime > fstat.st_mtime || (cstat.st_mtime == fstat.st_mtime && !regenerate) ){
						needf2c = 0;
						Cisnewer = 1;
						if( debugFlag ){
							fprintf( stderr, "# existing %s is not older than %s (and will not be removed)\n", cname, fname );
						}
					}
				}
			}
			if( needf2c ){
				if( debugFlag ){
					fputs( "# ", stderr );
					fprintf( stderr, "%s\n", command );
				}
			}
			if( !needf2c || (ret= system( command ) >> 8) == 0 ){
				xfree(command);
				if( fname && cname && keepC && !Cisnewer ){
					if( (fp= fopen(cname, "a")) ){
					  int i;
						fprintf( fp, "\n/* C file created through fgcc (c) RJVB 2009\n" );
						fprintf( fp, " \\ invoked as:\n" );
						fprintf( fp, " \\ %s", argv[0] );
						for( i= 1; i< argc; i++ ){
							fprintf( fp, " '%s'", argv[i] );
						}
						fprintf( fp, "\n */\n" );
						fclose(fp);
					}
					command = concat2( command, "touch -r ", fname, " ", cname, NULL );
					if( system(command) ){
						fprintf( stderr, "# " );
						perror(command);
					}
					xfree(command);
				}
				command = concat2( command, cc, " ", cflagsf2c, " ", cflags, " ", cname, NULL );
				if( outo ){
					command = concat2( command, " -o ", outo, NULL );
				}
				if( libs ){
					command = concat2( command, " ", libs, NULL );
				}
				if( link ){
					command = concat2( command, " ", fglibs, NULL );
				}
				if( debugFlag ){
					fputs( "# ", stderr );
					fprintf( stderr, "%s\n", command );
				}
				ret = system(command) >> 8;
			}
			else if( debugFlag && ret ){
				fprintf( stderr, "# f2c returned %d\n", ret );
			}
			if( !ret ){
				if( !Cisnewer && !keepC && needf2c ){
				  // unlink C file if we created one just for this compilation:
					if( cname && unlink(cname) ){
						fprintf( stderr, "# rm " );
						perror(cname);
					}
				}
			}
			else{
				if( debugFlag ){
					fprintf( stderr, "# %s returned %d\n", cc, ret );
				}
				if( !Cisnewer && cname ){
					fprintf( stderr, "# %s not removed automatically by fgcc\n", cname );
				}
			}
			xfree(command);
			if( cname != files ){
				xfree(cname);
			}
		}
	}
	else if( dashv || printlibgcc || dashV || dashversion ){
	  char *command = NULL, *c;
		 // we'll presume a -v argument without file specification is to get gcc's configuration output.
		 // as this may not be obtained when multiple -arch options are given, remove all opttions but the -v one:
		if( (c= strchr(cc, ' ')) ){
			*c = '\0';
		}
		if( (c= strchr(cc, '\t')) ){
			*c = '\0';
		}
		if( dashversion && versioncommand ){
			command = versioncommand;
			versioncommand = NULL;
		}
		else{
			command = concat2( command, cc, NULL );
			if( dashv ){
				command = concat2( command, " -v", NULL );
			}
			if( dashV ){
				command = concat2( command, " -V", NULL );
			}
			if( dashversion ){
				command = concat2( command, " --version", NULL );
				fprintf( stdout, "GNU Fortran 95 (GCC 4.0.3)\n" );
			}
			if( printlibgcc ){
				command = concat2( command, " -print-libgcc-file-name", NULL );
			}
		}
		if( debugFlag ){
			fprintf( stderr, "%s\n", command );
		}
		ret = system(command) >> 8;
		xfree(command);
	}

	xfree(cflags);
	xfree(f2cflags);
	xfree(cppflags);
	xfree(cflagsf2c);
	xfree(libs);
	xfree(fglibs);
	xfree(outo);
	xfree(files);
	xfree(versioncommand);
	xfree(ignoredArgs);

	exit(ret);
}
