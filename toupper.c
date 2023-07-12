#include <stdio.h>

#include <ctype.h>

main( argc, argv)
int argc;
char **argv;
{	register char *c= argv[1];

	if( argc< 2){
	register int c;
		c= getc(stdin);
		while( c!= EOF){
			fputc( toupper(c), stdout);
			c= getc(stdin);
		}
	}
	else{
		for( --argc, ++argv; argc> 0; argc--, c= *(++argv) ){
			while( *c){
				fputc( toupper(*c), stdout);
				c++;
			}
			if( argc> 1)
				fputc( ' ', stdout);
		}
		fputc( '\n', stdout);
	}
	exit(0);
}

#ifdef MCH_AMIGA
_wb_parse(){}
#endif MCH_AMIGA
