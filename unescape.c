#include <stdio.h>

main()
{ char c, buf[5];
	c= getchar();
	while( c && !feof(stdin) && !ferror(stdin) ){
		if( c== '%' ){
			if( (c= getchar()) && !feof(stdin) && !ferror(stdin) && c!= '%' ){
			  int i= 0, n;
				if( c== 'u' ){
					c= getchar();
					n= 5;
				}
				else{
					n= 3;
				}
				do{
					buf[i]= c;
					i++;
				}
				while( (c= getchar()) && !feof(stdin) && !ferror(stdin) && i< n && c!= '%' );
				buf[i]= '\0';
				sscanf( buf, "%x", &i );
				if( n== 3 ){
					fputc( i, stdout );
				}
				else{
					fprintf( stdout, "%u", i );
				}
			}
			else{
				fputc( c, stdout );
				c= getchar();
			}
		}
		else{
			fputc( c, stdout );
			c= getchar();
		}
	}
}
