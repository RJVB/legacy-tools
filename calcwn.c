/* command line calculator	
 * (C)(R) R. J. Bertin, 20/11/1989
 * operands will work on the result of the operation on the left;
 * calc 1 arg 1 sin == calc 1 arg 1 | calc i sin == sin(45)
 :ts=4
 */

#include <local/cx.h>			/* just to have everything needed	*/
#include <local/mxt.h>
#include "local/Macros.h"
#include "local/64typedefs.h"
#include <ctype.h>
#ifdef CONSOLE_CONFIG
	#include <local/fopenw.h>
#endif

#ifdef _MAC_THINKC_
	#undef exit
#endif

#ifdef _MAC_MPWC_
#	include <:local:traps.h>
#else
#	include <local/traps.h>
#endif

IDENTIFY("small calculator, main body");

#define ERROR puts( "EEEEEEEEEEEE")
#define OP(a) (strcmp(op,a)==0)

#if !defined(linux) && !defined(__MACH__) && !defined(__APPLE_CC__)
extern char *sys_errlist[];
#endif

double xd, yd, rd, stored_res[10];
unsigned long xl, yl, rl;
char op[80];
int op2= 0, All;
int next= 2, arg1= 1, arg2= 2, arg3= 3, args, inpr= 0;

void prilo(x, maincall)											/* print a long	*/
long x;
int maincall;
{	register int i;

	if( maincall ){
		printf("%ld\n", x);
		fflush( stdout);
	}
	if( All){											/* verbose mode	*/
		fprintf( stderr, " = %lu = 0x%lx = \\%03lo = ", (unsigned long)x, x, x);
		if( abs(x)< (1 << (sizeof(char)*8)) ){
		  char xc= x & 255;
		  unsigned char xu= xc;
			fprintf( stderr, "%d = %u = %c = ", xc, xu, xc );
		}
		for( i= sizeof(long)*8-1; i>= 0; i--){			/* binary print	*/
			fputc( (((x >> i) & 1)+'0'), stderr);
			if( i && i % 8== 0)
				fputc( ':', stderr);
		}
		if( sizeof(x) == 4 || ((unsigned long)x <= 0xffffffff) ){
		  char *fourcharcode = (char*) &x;
			fprintf( stderr, " = '%c", fourcharcode[0] );
			for( i= 1; i< 4; i++ ){
				fputc( fourcharcode[i], stderr );
			}
			fputc( '\'', stderr );
		}
		putc( '\n', stderr);
	}
}

void prido(double x)											/* print a double	*/
{   long xl;
	int i, j;
	double longmax= (double)((long)0x7fffffff),
	  longmin= -1.0- longmax;

	printf( "%s\n", d2str( x, "%.10lE", NULL) );
	fflush( stdout);
	if( All){											/* silent mode	*/
		fprintf( stderr, " = %s = %s", d2str( x, "%.16lf", NULL), d2str( x, "%G", NULL) );
		if( !(NaN(x) || INF(x)) && ((x< 0 && x>longmin) || (x> 0 && x< longmax)) )
			xl= (long) x;
		else
			xl= 0L;
		if( x== (double)xl){
#if 0
			fprintf( stderr, " = %ld = %lu = 0x%lx = \\%03lo = ",
				xl, (unsigned long)xl, xl, xl);
			if( abs(x)< (1 << (sizeof(char)*8)) ){
			  char xc= xl & 255;
			  unsigned char xu= xc;
				fprintf( stderr, "%d = %u = %c = ", xc, xu, xc );
			}
			for( i= sizeof(long)*8-1; i>= 0; i--){		/* binary print	*/
				fputc( (((xl >> i) & 1)+'0'), stderr);
				if( i && i % 8== 0)
					fputc( ':', stderr);
			}
			putc( '\n', stderr);
#else
			fprintf( stderr, " = %ld", xl );
			prilo(xl, 0);
#endif
		}
		else{
#ifdef FP_BINARY
		register unsigned char *c;
		IEEEfp *ie= (IEEEfp*) &x;
			c= (unsigned char*) &x;
			fputs( " = 0x", stderr);
			if( sizeof(long) == sizeof(double) ){
 				fprintf( stderr, "%08x:%08x", ie->l.high, ie->l.low );
// 				fprintf( stderr, "%x", ie->ui );
			}
			else{
				fprintf( stderr, "%08lx:%08lx", ie->l.high, ie->l.low );
			}
/* 			for( i= 0; i< sizeof( double); i+= sizeof(long) ){	*/
/* 				fprintf( stderr, "%lx", *((long*)&c[i]) );	*/
/* 				if( i < sizeof(double)- sizeof(long) )	*/
/* 					fputc( ':', stderr);	*/
			/* 			}	*/
			fputs( " = ", stderr);
			for( i= 0; i< sizeof( double); i++, c++){
				for( j= 7; j>= 0; j--)
					fputc( (((*c >> j) & 1) + '0'), stderr);
				if( i< sizeof(double)-1)
					fputc( ':', stderr);
			}
#endif
			fprintf( stderr, "\n");
		}
	}
    return;
}

#define ExArg(x)	{}

int OpCheck( xd, op, yd)						/* check if we have two operands	*/
double *xd, *yd;
char *op;
{
		if( !op2){								/* copy the first as the second	*/
			if( All)
				fprintf( stderr, "!! %s %s %s\n", d2str( *xd, "%g", NULL), op, d2str( *xd, "%g", NULL) );
			*yd= *xd;
			return( 0);
		}
		return( 0);
}

extern int errno;

void read_xd( arg)									/* get the first operand	*/
char *arg;
{  int ind= 0;
   long xl;
   unsigned int len = strlen(arg);
	if( len && strncmp( arg, "0x", 2)== 0 ){
		sscanf( &arg[2], "%lx", &xl);
		xd= (double) xl;
		return;
	}
	else if( *arg && *arg== '\\' ){
		sscanf( &arg[1], "%lo", &xl );
		xd= (double) xl;
		return;
	}
	else if( !strcmp( arg, "-Inf") ){
		set_Inf( xd, -1);
		return;
	}
	else if( !strcmp( arg, "Inf") ){
		set_Inf( xd, 1);
		return;
	}
	else if( !strcmp( arg, "NaN") ){
		set_NaN( xd);
		return;
	}
	else if( len == 5 && arg[0] == '#' ){
	  uint32 uxl = *( (uint32*) &arg[1] );
		xd = (double) uxl;
		return;
	}
	else if( len> 1 && !isdigit( arg[1])){		/* number	*/
		sscanf( arg, FLOFMT, &xd);
		return;
	}
	switch( *arg){									/* constant or flag	*/
		case 'i': 
		case 'I':									/* check for stream input	*/
			fprintf( stderr, "%2d> ", inpr);
			fscanf( stdin, FLOFMT, &xd);
			inpr++;
			break;
		case 'p':
		case 'P':
			xd= acos( -1.0);
			break;
		case 'e':
		case 'E':
			xd= exp( 1.0);
			break;
		case 'g':									/* get buffer contents as operand	*/
		case 'G':
			ind= (isdigit( arg[1]))? arg[1]-'0' : 0;
			xd= stored_res[ind];
			break;
		default:
			sscanf( arg, FLOFMT, &xd);				/* so it should be a number	*/
			break;
	}
	return;
}

void read_yd( arg)									/* get the second operand	*/
char *arg;
{  int ind= 0;
   long yl;
   unsigned int len = strlen(arg);
	if( len && strncmp( arg, "0x", 2)== 0 ){
		sscanf( &arg[2], "%lx", &yl);
		yd= (double) yl;
		return;
	}
	else if( *arg && *arg== '\\' ){
		sscanf( &arg[1], "%lo", &yl );
		yd= (double) yl;
		return;
	}
	else if( !strcmp( arg, "-Inf") ){
		set_Inf( yd, -1);
		return;
	}
	else if( !strcmp( arg, "Inf") ){
		set_Inf( yd, 1);
		return;
	}
	else if( !strcmp( arg, "NaN") ){
		set_NaN( yd);
		return;
	}
	else if( len == 5 && arg[0] == '#' ){
	  uint32 uxl = *( (uint32*) &arg[1] );
		xd = (double) uxl;
		return;
	}
	else if( len> 1 && !isdigit( arg[1])){
		sscanf( arg, FLOFMT, &yd);
		return;
	}
	switch( *arg){
		case 'i': 
		case 'I':									/* check for stream input	*/
			fprintf( stderr, "%2d> ", inpr);
			fscanf( stdin, FLOFMT, &yd);
			inpr++;
			break;
		case 'p':
		case 'P':
			yd= acos( -1.0);
			break;
		case 'e':
		case 'E':
			yd= exp( 1.0);
			break;
		case 'g':
		case 'G':
			ind= (isdigit( arg[1]))? arg[1]-'0' : 0;
			yd= stored_res[ind];
			break;
		default:
			sscanf( arg, FLOFMT, &yd);
			break;
	}
	return;
}

int show_eqn( char *arg)						/* check if the equation must be shown	*/
{ int ret;	
	if( All== 0)
		ret=( 0);						/* silent mode	*/
	if( All== 2)
		ret=( 1);						/* verbose mode	*/
	if( ! isdigit( (unsigned int) arg[1]) ){				/* don't always show e.g. eq. with 'cos'	*/
		ret=( 0);
	}
	switch( *arg){
		case 'i':
		case 'I':
		case 'g':
		case 'G':
			ret=( 1);
			break;
		default:
			ret=( 0);
			break;	
	}
	return( ret );
}

int constant( arg)						/* the constants we know	*/
char *arg;
{
	if( strlen( arg)> 1)				/* constants have length 1	*/
		return( 0);
	switch( *arg){
		case 'p':
		case 'P':
			return( 1);
			break;
		case 'e':
		case 'E':
			return( 1);
			break;
		default:
			return( 0);
			break;
	}
}

int Args= 2;

calc_parse( argc, argv)
int argc;
char **argv;
{
	double resd;
	int type, arg;
	long resl;
	extern int calc();

	args= argc;
	if( argc<= 0)
		return( 0);
	if( argc>= Args && strcmp( argv[1], "-s")== 0){
		All= 0;											/* silent mode	*/
		Args++;
		arg1++; arg2++; arg3++;
	}
	else
		if( argc>= Args && strcmp( argv[1], "-v")== 0){
			All= 2;										/* verbose mode	*/
			Args++;
			arg1++; arg2++; arg3++;
		}
		else if( argc>= Args && strcmp( argv[1], "-q")== 0){
			argc--;
			argv++;
			exit( !calc_parse( argc, argv) );
		}
		else
			All= 1;
    if( argc< Args){									/* help!	*/
        fprintf( stderr, 
			"usage: %s [option] <operand> <operator> [<operand>| <operator> [<operand>|<operator>[...]]]\n",
			argv[0]);
		fprintf( stderr, "argument 'i' indicates input from stdin for that argument.\n");
		fprintf( stderr, "argument 'c#' copies the last result to buffer #\n");
		fprintf( stderr, "argument 'g#' is substituted for the contents of buffer #\n");
		fprintf( stderr, "options: -s = silent | -v = verbose\n");
        fprintf( stderr, "Valid operators:\n");
        fprintf( stderr, "+ - x / : mod xx(pow) log exp sin cos tan asin acos atan arg len fac shl shr and or xor not\n");
        return( 1);
    }
	read_xd( argv[ arg1]);								/* get operand 1	*/
    if( argc== Args){									/* only one argument: print it	*/
        prido( xd);
        return( 0);
    }
	if( ( *argv[arg2]== 'i' || *argv[arg2]== 'I') && argv[arg2][1]== '\0' ){
		fprintf( stderr, "%2d> ", inpr);				/* get the operator	*/
		fscanf( stdin, "%s ", op);
		inpr++;
	}
	else
		strncpy( op, argv[ arg2], 79);
	if( op[ strlen(op)]== '\n')
		op[ strlen(op)]= '\0';
    op[ 79]= '\0';
    if( argc> 3){										/* and operand 2	*/
        op2= 1;
		read_yd( argv[ arg3]);
    }
	arg1= calc( xd, op, yd, &resd, &resl, &type);		/* calculate	*/
	if( arg1){						/* error, so result is Not-a-Number	*/
		set_NaN( resd);
		type= 1;
	}
	else{
		arg= arg2+ next;			/* check for storing request	*/
		if( argc> arg &&
			(( isdigit( argv[arg][1]) || argv[arg][1]== '\0') &&
				(*argv[arg]== 'c' || *argv[arg]== 'C')
			)
		){
		  int ind;					/* the buffer number (default 0)	*/
			ind= ( isdigit( argv[arg][1]))? argv[arg][1]- '0' : 0;
			stored_res[ind]= (type)? resd : (double) resl;
			if( All){				/* notify the result was stored	*/
				if( next== 2)
					fprintf( stderr, "%s %s %s = %s stored in %d\n",
						d2str( xd, "%g", NULL), op,
						d2str( yd, "%g", NULL), d2str( stored_res[ind], "%g", NULL), ind
					);
				else{
					fprintf( stderr, "%s %s = %s stored in %d\n",
						d2str( xd, "%g", NULL), op,
						d2str( stored_res[ind], "%g", NULL), ind
					);
				}
			}
			arg2+= 1;
			arg3+= 1;
		}
		else if( show_eqn( argv[1]) || show_eqn( argv[arg2]) ||
			( argc> arg3 && show_eqn( argv[arg3]) )){
				fprintf( stderr, "%s %s %s",
					d2str( xd, "%g", NULL), op,
					d2str( yd, "%g", NULL)
				);	/* show the equation if legal	*/
				if( type)
					fprintf( stderr, " = %s\n", d2str( resd, "%g", NULL) );
				else
					fprintf( stderr, " = %ld\n", resl);
		}
	}
/* 	while( argc> arg2+ next && arg1== 0)	*/
	while( argc> arg3+ next && arg1== 0)
	{			/* process the rest of the commandline	*/
		arg2+= next;
		arg3+= next;
		if( argc> arg2 && 
			( isdigit( *argv[ arg2]) || tolower( *argv[arg2])== 'g' ||
				constant( argv[arg2])
			)
		){
		  /* new equation: break old one	*/
				read_xd( argv[arg2]);
				resd= xd;
				if( !Inf(xd) && !NaN(xd) ){
					resl= (long)xd;
				}
				else{
					resl= 0;
				}
				arg2+= 1, arg3+= 1;
		}
		if( type)								/* else: previous result is new lh operand	*/
			xd= resd;
		else
			xd= (double)resl;
		if( ( *argv[arg2]== 'i' || *argv[arg2]== 'I') && argv[arg2][1]== '\0' ){
			fprintf( stderr, "%2d> ", inpr);	/* stream input	*/
			fscanf( stdin, "%s ", op);
			inpr++;
		}
		else
			strncpy( op, argv[ arg2], 79);
		if( op[ strlen(op)]== '\n')
			op[ strlen(op)]= '\0';
		op[ 79]= '\0';
		if( argc> arg3){						/* 2nd operand always simplest	*/
			op2= 1;
			read_yd( argv[ arg3]);
		}
		else
			op2= 0;
		arg1= calc( xd, op, yd, &resd, &resl, &type);	/* calculate once more	*/
		arg= arg2+ next;
		if( arg< argc && (
				( isdigit( argv[arg][1]) || argv[arg][1]== '\0')
				&& (*argv[arg]== 'c' || *argv[arg]== 'C')
			)
		){
		  int ind= 0;										/* store the result if requested	*/
			ind= ( isdigit( argv[arg][1]))? argv[arg][1]- '0' : 0;
			stored_res[ind]= (type)? resd : (double) resl;
			if( All)									/* and notify	*/
				fprintf( stderr, "%s %s %s = %s stored in %d\n",
					d2str( xd, "%g", NULL), op,
					d2str( yd, "%g", NULL), d2str( stored_res[ind], "%g", NULL), ind
				);
			arg2+= 1;
			arg3+= 1;
		}
		else if( show_eqn( argv[arg2]) || 				/* else show if legal	*/
			( argc> arg3 && show_eqn( argv[arg3]) && next== 2)){
				fprintf( stderr, "%s %s", d2str( xd, "%g", NULL), op);
				if( ( argc> arg3 && show_eqn( argv[arg3]) && next== 2))
					fprintf( stderr, " %s", d2str( yd, "%g", NULL) );
				if( type)
					fprintf( stderr, " = %s\n", d2str( resd, "%g", NULL) );
				else
					fprintf( stderr, " = %ld\n", resl);
		}
	}
	if( arg1)									/* break with error: show previous result	*/
		fprintf( stderr, "previous result= ");	/* can be a NaN!	*/
	fflush( stderr);
	if( type)									/* print the most actual result	*/
		prido( resd);
	else
		prilo( resl, 1);
	return( 1);
}

show_line( argc, argv)
int argc;
char **argv;
{	int i;
	printf( "#%d# ", argc);
	for( i= 0; i< argc; i++)
		printf( "%s ", argv[i]);
	fputc( '\n', stdout);
}

int argc;
char *ARGV[MAX_ARGS], **argv;
char c_line[512];

main( Argc, Argv)
int Argc;
char **Argv;
{	int i= 0;
	char prompt[20];
extern int bright, bbottom, fcw, fcb;
	double timer;

	for( i= 0; i< Argc && i< MAX_ARGS; i++)
		ARGV[i]= Argv[i];
	argc= Argc;
	argv= &ARGV[0];

#ifdef MACINTOSH
#	ifdef _MAC_THINKC_
		Click_On( 0);
#	endif
#	ifdef CONSOLE_CONFIG
		ConsoleWindowName= "CalcWindow";
		MFMaxRow= 30;
#	endif CONSOLE_CONFIG
#	ifdef _MAC_MPWC_
		InitProgram();
#	endif
#endif

	InitProgram();
	Set_Timer();
	Elapsed_Time();
	i+= calc_parse( Argc, Argv);
	timer= Elapsed_Time();
	sprintf( prompt, "%s-%d? ", time_string(timer), i);
	Get_CommandLine( &argc, argv, MAX_ARGS, c_line, 512, prompt);
	while( *c_line && strncmp( argv[1], "quit", 2) && strncmp( argv[1], "exit", 2)){
		Args= 2;
		op2= 0;
		next= 2; arg1= 1; arg2= 2; arg3= 3; inpr= 0;
		if( argc> 1){
			if( strcmp( argv[1], "?")== 0)
				argc= 1;
			Elapsed_Time();
			timer= Tot_Time;
			i+= calc_parse( argc, argv);
			Elapsed_Time();
			timer= Tot_Time;
		}
		sprintf( prompt, "%s-%d? ", time_string(timer), i);
		Get_CommandLine( &argc, argv, MAX_ARGS, c_line, 512, prompt);
	}
	exit( 0);
}
