#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>

#ifdef __GCCOPT__
#	include <local/Macros.h>
	IDENTIFY( "File timestamp utility" );
#endif

int time_stamp( fp, name)
FILE *fp;
char *name;
{  struct stat Stat;
   struct tm *tm;
   static int verbose= 0, round_up= 0;
	if( !strcmp( name, "-v") ){
		verbose= 1;
		return(0);
	}
	else if( !strcmp( name, "-r") ){
		round_up= 1;
		return(0);
	}
	else if( stat( name, &Stat) ){
		perror( name);
	}
	else{
		tm= localtime( &(Stat.st_mtime) );
		if( tm){
			if( round_up ){
				if( (tm->tm_min+= 1)>= 60 ){
					tm->tm_min-= 60;
					tm->tm_hour+= 1;
				}
				if( tm->tm_hour>= 24 ){
					tm->tm_hour-= 24;
					tm->tm_mday+= 1;
				}
				if( tm->tm_mday> 31 ){
					tm->tm_mday-= 31;
					tm->tm_mon+= 1;
				}
				if( tm->tm_mon> 11 ){
					tm->tm_mon-= 11;
					tm->tm_year+= 1;
				}
			}
			if( verbose){
				fprintf( fp, "%s: %s", name, asctime(tm) );
			}
			else{
#ifdef __APPLE__
 				tm->tm_year += 1900;
#endif
				fprintf( stderr, "(%lu)", Stat.st_mtime );
				fflush(stderr);
#if defined(__hpux) 
				fprintf( fp, "%02d%02d%02d%02d%02d.%02d\n",
					tm->tm_year, tm->tm_mon+1, tm->tm_mday,
					tm->tm_hour, tm->tm_min+1,
					tm->tm_sec
				);
#elif defined(__APPLE__)
				fprintf( fp, "%04d%02d%02d%02d%02d.%02d\n",
					tm->tm_year, tm->tm_mon+1, tm->tm_mday,
					tm->tm_hour, tm->tm_min,
					tm->tm_sec
				);
#else
				fprintf( fp, "%02d%02d%02d%02d%02d\n",
					tm->tm_mon+1, tm->tm_mday,
					tm->tm_hour, tm->tm_min,
					tm->tm_year
				);
#endif
			}
			fflush( fp);
			return( 0);
		}
		else
			perror( name);
	}
	return(1);
}

main( argc, argv)
int argc;
char **argv;
{  int i= 1, j= 0;
	while( i< argc ){
		j+= time_stamp( stdout, argv[i]);
		i++;
	}
	return( j);
}
