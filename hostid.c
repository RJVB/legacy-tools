#include <stdio.h>
#include <stdlib.h>
#include <errno.h>


main( int argc, char *argv[] )
{ unsigned long int old= gethostid();
  unsigned long int new;
	if( argc>= 2 ){
	  char nbuf[64];
		errno= 0;
		if( strncmp( argv[1], "0x", 2 )== 0 ){
			new= strtoul( argv[1], NULL, 16 );
			sprintf( nbuf, "0x%lx", new );
		}
		else{
			new= strtoul( argv[1], NULL, 10 );
			sprintf( nbuf, "%lu", new );
		}
		if( errno || strcmp(nbuf,argv[1]) ){
			fprintf( stderr, "%s [new-numeric-id]\n\tor\n%s 0xnew-hex-id\n", argv[0], argv[0] );
			fprintf( stderr, "argument '%s' evaluates to 0x%lx (%lu)\n", argv[1], new, new );
			exit(1);
		}
		else{
			  /* Mandrake linux 6.1: /etc/hostid is expected in /var/adm/hostid; /var/adm doesn't exist... */
			sethostid(new);
		}
		fprintf( stdout, "New hostid 0x%lx (%lu) was 0x%lx; errno=%d\n", gethostid(), new, old, errno );
	}
	else{
		fprintf( stdout, "0x%lx\n", old );
	}
	exit(0);
}
