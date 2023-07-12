#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include <string.h>

#include <sys/types.h>
#if defined(sgi) || defined(__FreeBSD__)
#	define ISBSD
#endif
#ifdef ISBSD
#	include <utmpx.h>
	typedef struct utmpx	utmp;
#	define utmpname	utmpxname
#	define getutent	getutxent
#	define setutent	setutxent
#	define getutid		getutxid
#	define getutline	getutxline
#	define endutent	endutxent
#else
#	include <utmp.h>
	typedef struct utmp	utmp;
#endif

#include <unistd.h>

#if defined(_HPUX_SOURCE) || defined(linux)
#	include <netdb.h>
#	include <sys/socket.h>
#	include <netinet/in.h>
#	include <arpa/inet.h>
extern int h_errno;
#endif

#include <pwd.h>

#if defined(__MACH__) || defined(__APPLE_CC__)
#	define ut_user	ut_name
#endif


#if  !defined(linux) && !defined(__MACH__) && !defined(__APPLE_CC__) && !defined(__CYGWIN__) && !defined(__FreeBSD__)
extern int errno, sys_nerr;
extern char *sys_errlist[];
#endif
#define serror()	((errno==0)?"No Error":\
					(errno<sys_nerr)?sys_errlist[errno]:"Invalid errno")

#ifdef _APOLLO_SOURCE
#	define HNLENGTH	32
#else
#	define HNLENGTH	256	/* 16	*/
#endif

#if !defined(ISBSD) && !defined(linux) && !defined(_AIX) && !defined(__CYGWIN__) && !defined(__FreeBSD__)
	extern struct utmp *getutent(void);
	extern struct utmp *getutid(struct utmp *);
	extern utmp *getutline(utmp *);
#endif

#ifndef ISBSD
char *type_name[]= {
	"EMPTY",
	"RUN_LVL",
	"BOOT_TIME",
	"OLD_TIME",
	"NEW_TIME",
	"INIT_PROCESS",
	"LOGIN_PROCESS",
	"USER_PROCESS",
	"DEAD_PROCESS",
	"ACCOUNTING",
};
#else
char *type_name[]= {
	"EMPTY",
	"BOOT_TIME",
	"OLD_TIME",
	"NEW_TIME",
	"USER_PROCESS",
	"INIT_PROCESS",
	"LOGIN_PROCESS",
	"DEAD_PROCESS",
	"SHUTDOWN_TIME",
};
#endif

int verbose= 0, short_name= 0;

char *basename(f)
char *f;
{  char *n= strrchr( f, '/');
	return( (n)? ++n : f);
}

/* Compare two strings backwards (from the end)	*/
int strrcmp( char *a, char *b)
{ char *A, *B;
  int len, ret=0;
	if( !a ){
		a= "";
	}
	if( !b ){
		b= "";
	}
	len= strlen(a);
	A= &a[len-1];
	B= &b[strlen(b)-1];
	for( ; A>= a; A--){
		if( B>= b ){
			ret+= (unsigned char) *A - (unsigned char) *B;
			B--;
		}
		else{
			ret+= (unsigned char) *A;
		}
	}
	return( ret );
}

/* Compare two strings backwards (from the end)	*/
int strrncmp( char *a, char *b, int n)
{ char *A, *B;
  int len, i, ret=0;
	if( !a ){
		a= "";
	}
	if( !b ){
		b= "";
	}
	len= strlen(a);
	A= &a[len-1];
	B= &b[strlen(b)-1];
	for( i= 0; A>= a && i< n; i++, A-- ){
		if( B>= b ){
			ret+= (unsigned char) *A - (unsigned char) *B;
			B--;
		}
		else{
			ret+= (unsigned char) *A;
		}
	}
	if( verbose>0 && ret== 0 ){
		if( B< b ){
			B++;
		}
		fprintf( stderr, "\t\t\"%s\" == \"%s\"\n", A, B);
	}
	return( ret );
}

utmp *save_ut( utmp *ut )
{  utmp *u= (utmp*) calloc( 1, sizeof(utmp));
	if( u ){
		memcpy( u, ut, sizeof(utmp));
	}
	return( u );
}

int main( int argc, char *argv[])
{  char *utmp_name=
#if defined __FreeBSD__
	"/var/run/utx.active",
#elif defined(ISBSD)
	"/etc/utmpx",
#elif linux
	_PATH_UTMP,
#else
	"/etc/utmp",
#endif
	terminal[128], username[128], username2[128], *t, rhostname[HNLENGTH+1];
   utmp *the_utmp= NULL, *other_utmp= NULL, *utmp, line;
   int n= 0;
   unsigned long r_address= 0L;
   char *r_hostname= NULL, *dot;
   struct hostent *host;
   pid_t ppid= getppid();
   struct passwd *passwd;
   int arg=1;
   extern char *getlogin();

	username2[127]= username[127]= terminal[127]= '\0';

	for( arg= 1; arg< argc; arg++ ){
		if( !strcmp( argv[arg], "-v") ){
			verbose= 1;
		}
		else if( !strcmp( argv[arg], "-q") ){
			verbose= -1;
		}
		else if( !strcmp( argv[arg], "-s") ){
			short_name= 1;
		}
		else if( !strcmp( argv[arg], "-f") && arg< argc-1 ){
#if defined(__MACH__) || defined(__APPLE_CC__) || defined(__FreeBSD__)
			fprintf( stderr, "-f option not supported on this platform!\n" );
#else
			errno= 0;
			utmpname( argv[arg+1] );
			  /* check: utmpname doesn't!	*/
			if( !getutent() ){
				fprintf( stderr, "%s: can't open utmpfile \"%s\" (%s)\n", argv[0], argv[arg+1], serror() );
				exit(-1);
			}
			else if( errno){
				fprintf( stderr, "%s: \"%s\": %s\n", argv[0], argv[arg+1], serror() );
				fflush( stderr );
			}
			utmp_name= argv[arg+1];
			setutent();
#endif
		}
		  /* get a terminal (tty) name	*/
		else if( !strncmp( argv[arg], "/dev/tty", 8) || !strncmp( argv[arg], "/dev/pty", 8) ){
			strncpy( terminal, basename( argv[arg]), 127);
			ppid= 0;
		}
	}
	if( ppid ){
	  extern char *ttyname();
		errno= 0;
		t= ttyname( fileno(stdin) );
		if( !t ){
			if( errno){
				perror( "rhostname: can't get tty");
// 				exit(10);
				terminal[0]= '\0';
			}
			else{
				fprintf( stderr, "%s: can't get tty\n", argv[0]);
// 				exit(10);
				terminal[0]= '\0';
			}
		}
		else{
			strncpy( terminal, basename(t), 127);
			if( errno){
				fprintf( stderr, "%s: \"%s\": %s\n", argv[0], terminal, serror() );
			}
		}
	}

	/* now, get the user's name	*/
	setpwent();
	if( !(passwd= getpwuid(geteuid())) ){
		if( errno)
			perror( "rhostname: can't find your name");
		t= getlogin();
	}
	else{
		t= passwd->pw_name;
	}
	endpwent();
	if( !t ){
		if( errno)
			perror( "rhostname: can't get your name");
		else
			fprintf( stderr, "%s: can't get your name\n", argv[0]);
		exit(20);
	}
	else{
		strncpy( username, t, 127);
	}
	if( (t= getlogin()) ){
		strncpy( username2, t, 127 );
	}

	if( verbose> 0 ){
		gethostname( rhostname, HNLENGTH );
		fprintf( stderr, "\tuser='%s'(%s) line='%s' ppid='%d' on '%s'\n",
			username, username2, terminal, (int) ppid, rhostname
		);
		fflush(stderr);
	}

	/* use some extra selectivity if a loginshell called us.
	 * if not, don't use ppid as a selection criterion - we would
	 * most likely not be albe to give an answer
	 */
/* 	if( ppid!= A LOGIN PROCESS )	*/
/* 		ppid= 0;	*/

	/* now, process the utmp file, using username and
	 * terminal to find the correct entry.
	 */
	strncpy( line.ut_line, terminal, 12);
	line.ut_line[11]= '\0';
	for( n= 0, errno= 0, utmp= getutent(); utmp; utmp= getutent(), n++){
		if( (strcmp( utmp->ut_user, username)==0 || strcmp( utmp->ut_user, username2)== 0) &&
			(!terminal[0] || strrncmp( basename(utmp->ut_line), terminal, strlen(terminal) )==0 )
#if !defined(__MACH__) && !defined(__APPLE_CC__)
			&& (utmp->ut_type== LOGIN_PROCESS || utmp->ut_type== USER_PROCESS)
#endif
		  ){
#if defined(__MACH__) || defined(__APPLE_CC__)
			/* This is about the best we can do on this platform...: */
			the_utmp= save_ut( utmp );
#else
			if( utmp->ut_pid== ppid ){
			  /* this is most certainly the one we're looking for	*/
				the_utmp= save_ut( utmp );
#ifdef DEBUG
				fprintf( stderr, "ut_pid(%d)==ppid(%d) saving the_utmp\n", utmp->ut_pid, ppid);
#endif
			}
			else{
			  /* this is almost certainly the one we're looking for
			   \ (catches su's and sub-shells)
			   */
				other_utmp= save_ut( utmp );
#ifdef DEBUG
				fprintf( stderr, "ut_pid(%d)!=ppid(%d) saving other_utmp\n", utmp->ut_pid, ppid);
#endif
			}
#endif
		}
#if !defined(__MACH__) && !defined(__APPLE_CC__)
		if( errno && utmp->ut_type!= BOOT_TIME && verbose>= 0 ){
			fprintf( stderr, "%s: error in %s at entry %d (0x%lx): '%s'\n",
				argv[0], utmp_name, n, utmp, serror()
			);
			fflush( stderr);
		}
		else{
			errno= 0;
		}
#endif
		if( verbose> 0 || errno ){
#if defined(_HPUX_SOURCE) || defined(linux)
		  struct in_addr addr;
			addr.s_addr= utmp->ut_addr;
#endif
			if( utmp->ut_host && utmp->ut_host[0] ){
				strncpy( rhostname, utmp->ut_host, HNLENGTH);
			}
			if( verbose> 0 ){
				fprintf( stderr, "[%d] ", n );
			}
#if !defined(__MACH__) && !defined(__APPLE_CC__) && !defined(__CYGWIN__)
			switch( utmp->ut_type ){
				case USER_PROCESS:
				case INIT_PROCESS:
				case LOGIN_PROCESS:
				case DEAD_PROCESS:
					// ut_id is applicable
					break;
				default:
					utmp->ut_type[0] = '\0';
					break;
			}
#endif
#if defined(__FreeBSD__)
			fprintf( stderr,
	"%s\tuser=\"%s\" id=\"%s\" tty=\"%s\" pid=%ld type=\"%s\"\n\trhost=\"%s\"\n",
				(the_utmp && the_utmp->ut_pid== utmp->ut_pid)? ">" : 
					(other_utmp && other_utmp->ut_pid== utmp->ut_pid)? "?" : "",
				utmp->ut_user, utmp->ut_id, utmp->ut_line, (long) utmp->ut_pid,
				type_name[utmp->ut_type],
				rhostname
			);
#elif !defined(__MACH__) && !defined(__APPLE_CC__) && !defined(__CYGWIN__)
			fprintf( stderr,
	"%s\tuser=\"%s\" id=\"%s\" tty=\"%s\" pid=%ld type=\"%s\"\n\texit=%d,%d rhost=\"%s\" addr=\"%s\" %s%s\n",
				(the_utmp && the_utmp->ut_pid== utmp->ut_pid)? ">" : 
					(other_utmp && other_utmp->ut_pid== utmp->ut_pid)? "?" : "",
				utmp->ut_user, utmp->ut_id, utmp->ut_line, (long) utmp->ut_pid,
				type_name[utmp->ut_type],
				utmp->ut_exit.e_termination, utmp->ut_exit.e_exit,
				rhostname,
#if defined(_HPUX_SOURCE) || defined(linux)
				inet_ntoa(addr),
#else	
				"unsupplied",
#endif
#ifndef ISBSD
				"T=",
				ctime( &utmp->ut_time)
#else
				"", ""
#endif
			);
#else
			fprintf( stderr,
	"\tuser=\"%s\" tty=\"%s\" rhost=\"%s\" addr=\"%s\" %s%s\n",
				utmp->ut_user, utmp->ut_line, 
				rhostname,
#if defined(_HPUX_SOURCE) || defined(linux)
				inet_ntoa(addr),
#else	
				"unsupplied",
#endif
#ifndef ISBSD
				"T=",
				ctime( &utmp->ut_time)
#else
				"", ""
#endif
			);
#endif
		}
		errno= 0;
#ifdef DEBUG
#	if defined(_HPUX_SOURCE) || defined(linux)
		fprintf( stderr, "\tuser='%s' line='%s' host='%s','%lx' pid='%lu' type='%d'\n",	
			utmp->ut_user, utmp->ut_line, utmp->ut_host, utmp->ut_addr,
			(unsigned long) utmp->ut_pid, (int) utmp->ut_type
		);
#	else
		fprintf( stderr, "\tuser='%s' line='%s' host='%s' pid='%lu' type='%d'\n",	
			utmp->ut_user, utmp->ut_line, utmp->ut_host,
			(unsigned long) utmp->ut_pid, (int) utmp->ut_type
		);
#	endif	/* _HPUX_SOURCE	*/
		fflush(stderr);
#endif	/* DEBUG	*/
	}
	if( !the_utmp && !other_utmp ){	
		if( errno)
			perror( "rhostname: can't find correct utmp entry");
		else
			fprintf( stderr, "%s: can't find correct utmp entry\n", argv[0]);
		exit(30);
	}

	rhostname[0]= rhostname[HNLENGTH]= '\0';

	if( !the_utmp && other_utmp ){
		the_utmp= other_utmp;
		other_utmp= NULL;
	}

	if( the_utmp->ut_host && strlen(the_utmp->ut_host) ){
		strncpy( rhostname, the_utmp->ut_host, HNLENGTH);
#ifdef DEBUG
		fprintf( stderr, "rhostname= %s\n", rhostname);
		fflush( stderr);
#endif	/* DEBUG	*/
	}
	else{
#ifndef _HPUX_SOURCE
		if( verbose>= 0 ){
			fprintf( stderr, "%s: no remote hostname in utmp file\n", argv[0]);
		}
		gethostname( rhostname, HNLENGTH );
#	ifdef DEBUG
		fprintf( stderr, "rhostname=hostname= %s\n", rhostname );
#	endif
		fflush( stderr);
/* 		exit(40);	*/
#else
		;
#endif
	}

#if defined(_HPUX_SOURCE) || defined(linux)
	/* also stores the address: see if we can use that	*/
	if( !(r_address= the_utmp->ut_addr) ){
/* 		if( !strlen( rhostname) ){
			fprintf( stderr, "%s: no remote hostname nor remote address in utmp file\n", argv[0]);
			exit(50);
		}
 *//* Maybe we should quit. Instead, we suppose we're logged on from local...	*/
		  char h_host[128];
			gethostname( h_host, 127);
			if( strlen(h_host) )
				r_hostname= h_host;
			else
				r_hostname= NULL;
	}
	else{
	  struct in_addr addr;
	  char *ip= NULL;
		addr.s_addr= r_address;
		ip= inet_ntoa(addr);
		if( verbose>= 0 ){
			fprintf( stderr, "\tremotehost address is %s (%s,%s) '%lx' - hostname is:\n",
				ip, the_utmp->ut_user, the_utmp->ut_line, r_address
			);
			fflush( stderr);
		}
		if( r_address== 0xff ){
		  char h_host[128];
			gethostname( h_host, 127);
			if( strlen(h_host) )
				r_hostname= h_host;
			else
				r_hostname= NULL;
		}
		else{
			if( !(host= gethostbyaddr( (char*) &r_address, sizeof(r_address), 2)) ){
				if( !ip){
					switch( h_errno ){
						case HOST_NOT_FOUND:
							t= "Authoritive Answer Host not found";
							break;
						case TRY_AGAIN:
							t= "Non-Authoritive Host not found, or SERVERFAIL";
							break;
						case NO_RECOVERY:
							t= "Non recoverable error";
							break;
						case NO_DATA:
							t= "Valid name, no data record of requested type";
							break;
						default:
							t= "Unknown error";
							break;
					}
					fprintf( stderr, "%s: %s\n", argv[0], t);
					fflush( stderr);
				}
				if( !strlen(rhostname) )
					exit( 60);
				else{
					if( (host= gethostbyname( rhostname)) )
						r_hostname= host->h_name;
					else
						r_hostname= ip;
				}
			}
			else{
				r_hostname= host->h_name;
			}
		}
	}
#endif

	if( short_name ){
		if( r_hostname && (dot= index( r_hostname, '.')) ){
			*dot= '\0';
		}
		if( (dot= index( rhostname, '.')) ){
			*dot= '\0';
		}
	}
	if( r_hostname){
		if( strcmp( rhostname, r_hostname) ){
			fprintf( stdout, "%s\n", r_hostname);
			fflush( stdout);
			if( strlen(rhostname) && verbose>= 0 ){
				fprintf( stderr, "\t%s has '%s'\n", utmp_name, rhostname);
			}
		}
		else if( strlen(rhostname) )
			fprintf( stdout, "%s\n", rhostname);
	}
	else if( strlen(rhostname) )
		fprintf( stdout, "%s\n", rhostname);

	endutent();

	if( the_utmp ){
		free( the_utmp );
	}
	if( other_utmp ){
		free( other_utmp );
	}

	exit(0);
}
