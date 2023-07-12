#include <stdio.h>

main( argc, argv)
int argc;
char **argv;
{	char buf[256], *c;
	extern char *getenv();

	if( c= getenv( "TERM") ){
		if( strcmp( c, "xterm") ){
			fputs( argv[0], stderr);
			fputs( ": You don't have an xterm!\n", stderr);
			exit(1);
		}
	}
	else{
		fputs( argv[0], stderr);
		fputs( ": Can't determine terminal\n", stderr);
		exit(1);
	}
	if( argc< 2){
	  extern char *fgets();
		while( fgets( buf, 255, stdin) ){
			buf[255]= '\0';
			fputc( 0x1b, stdout);
			fputs( "]0;", stdout);
			fputs( buf, stdout);
			fputc( 0x07, stdout);
		}
	}
	else{
	  int i= 1;
		for( i= 1; i< argc-1; i++){
			argv[i][strlen(argv[i])]= ' ';
		}
		fputc( 0x1b, stdout);
		fputs( "]0;", stdout);
		fputs( argv[1], stdout);
		fputc( 0x07, stdout);
	}
	exit(0);
}
