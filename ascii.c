#include <stdio.h>

main()
{ int i;
	for( i= ' '; i< (1 << (8*sizeof(char))); i++ ){
		printf( "%c", i );
		if( i && (i % 32)== 0 ){
			printf( "\r\n");
		}
	}
	fputs( "\r\n", stdout );
	for( i= 0; i< (1 << (8*sizeof(char))); i++ ){
		printf( "|%03d %02x %c", i, i, i );
		if( i && (i % 8)== 0 ){
			printf( "\r\n");
		}
		else{
			printf( " " );
		}
	}
	printf( " \r\n");
	exit(0);
}
