#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <sys/param.h>

#include <errno.h>
extern int sys_nerr;
extern char *sys_errlist[];

#define serror() ((errno>0&&errno<sys_nerr)?sys_errlist[errno]:"invalid errno")

char *target_format= "%s.%03u";

unsigned int bsplit( char *source, char *pref, unsigned int maxsize )
{  unsigned int n, N= 0, cont= 1;
   FILE *sfp, *tfp;
   char target[MAXPATHLEN];

	if( strcmp( source, "-" ) ){
		sfp= fopen( source, "r");
	}
	else{
		source= "stdin";
		sfp= stdin;
	}
	if( sfp ){
		if( setvbuf( sfp, NULL, _IOFBF, maxsize ) ){
			fprintf( stderr, "Notice: couldn't set buffer[%u] for sourcefile \"%s\"\n",
				maxsize, source
			);
		}
		do{
			sprintf( target, target_format, pref, N+ 1);
			if( (tfp= fopen( target, "w")) ){
				if( setvbuf( tfp, NULL, _IOFBF, maxsize ) ){
					fprintf( stderr, "Notice: couldn't set buffer[%u] for targetfile \"%s\"\n",
						maxsize, target
					);
				}
				n= 0;
				while( !feof(sfp) && !ferror(sfp) && !ferror(tfp) && cont && n< maxsize ){
				  int c= getc(sfp);
					if( c!= EOF ){
						putc( c, tfp);
					}
					n+= 1;
				}
				if( feof(sfp) ){
					n-= 1;
				}
				fprintf( stderr, "Wrote %u bytes into \"%s\"\n", n, target );
				fclose(tfp);
				N+= 1;
			}
			else{
				fprintf( stderr, "Can't open target \"%s\" (%s)\n",
					target, serror()
				);
				cont= 0;
			}
		}
		while( !feof(sfp) && !ferror(sfp) && cont );
		if( sfp!= stdin ){
			fclose(sfp);
		}
	}
	else{
		fprintf( stderr, "Can't open source \"%s\" (%s)\n",
			source, serror()
		);
	}
	return(N);
}

main( int ac, char *av[] )
{ int fn= 0, args= ac;
  unsigned int maxsize;
  char _pref[64], *prefix, *source;
	if( args> 1 ){
		ac= 1;
		maxsize= (int)(1.3 * 1024 * 1024);
		while( args> 0 ){
			source= NULL;
			sprintf( _pref, "splt%d", fn );
			prefix= _pref;
			while( args> 0 && !source ){
				if( !strcmp( av[ac], "-s" ) ){
					if( args>= 3 ){
						maxsize= (unsigned) atoi( av[ac+1]);
						args-= 2;
						ac+= 2;
					}
					else{
						fprintf( stderr, "-s needs number, and filename\n" );
						exit(fn);
					}
				}
				else if( !strcmp( av[ac], "-f" ) ){
					if( args>= 3 ){
						if( strlen(av[ac+1]) && strlen(av[ac+1])< MAXPATHLEN-10 ){
							prefix= av[ac+1];
						}
						else{
							fprintf( stderr, "\"%s\": invalid prefix ignored\n", av[ac+1] );
						}
						args-= 2;
						ac+= 2;
					}
					else{
						fprintf( stderr, "-f needs prefix, and filename\n" );
						exit(fn);
					}
				}
				else if( !strcmp( av[ac], "-F" ) ){
					if( args>= 3 ){
						if( strlen(av[ac+1]) && strlen(av[ac+1])< MAXPATHLEN-10 ){
							target_format= av[ac+1];
						}
						else{
							fprintf( stderr, "\"%s\": invalid target_format ignored\n", av[ac+1] );
						}
						args-= 2;
						ac+= 2;
					}
					else{
						fprintf( stderr, "-F needs target-format string, and filename\n" );
						exit(fn);
					}
				}
				else{
					if( args>= 1 ){
						source= av[ac];
						args-= 1;
						ac+= 1;
					}
				}
			}
			if( source ){
				if( bsplit( source, prefix, maxsize ) ){
					fn+= 1;
				}
			}
		}
	}
	else{
		fprintf( stderr, "Usage: %s [-s maxsize] [-f prefix *] <filename|-> ...\n", av[0] );
		fprintf( stderr, "\ttarget-files <prefix|splt<fn#>>.#, with fn# the fn'th file being split\n"
			"\tand # the splitnumber (padded with leading zeros to 3 digits).\n"
			"\t-F <target-format>: specifies new format (default \"%s\")\n"
			"\t* : reset for each file being split\n",
			target_format
		);
	}
	exit( fn );
}
