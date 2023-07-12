/* command line calculator	
 * (C)(R) R. J. Bertin, 20/11/1989
 * operands will work on the result of the operation on the left;
 * calc 1 arg 1 sin == calc 1 arg 1 | calc i sin == sin(45)

 ******** calc function *********
 :ts=4
 */

#include <stdio.h>
#include <local/cx.h>
#include <local/mxt.h>
#include "local/Macros.h"

IDENTIFY("small calculator, functions");

#define ERROR puts( "EEEEEEEEEEEE")
#define OP(a) (strcmp(op,a)==0)

#define ExArg(x)	{}

extern double xd, yd, rd, stored_res;
extern unsigned long xl, yl, rl;
extern char op[80];
extern int op2, All;
extern int next, arg1, arg2, arg3, args, OpCheck();

#ifdef MCH_AMIGA

double fmod( x, y)
double x, y;
{	double z;
#ifdef MFFP_AZTEC3_6_a
	return( -y* modf( x/ y, &z));	/* Aztec modf(x) returns -frac of x n McFFP format*/
#else
	return( y* modf( x/ y, &z));	
#endif
}

#endif

double fac( x)
register double x;
{	register double y= 1.0;
	
	/* actually, using 10 byte floats, 1755! already yields
	 * INF. We put the limit at 1e6 to have some timing:
	 * on a MacIIx, MPW C with direct coprocessor code, fac(1754)
	 * runs in 0.12 sec; fac(1e6) in 16.15 seconds.
	 * under A/UX2.01 with gcc: 0.07 vs. 8.6 seconds
	 * With 8 byte floats: 171!== Inf
	 * AUX: 0.05 seconds
	 */
	if( (sizeof(double)==8 && x> 170.0) || x> 1e6 )
#ifdef _MAC_MPWC_
		return( HUGE_VAL);
#else
{
		double dum;
		set_Inf( dum, 1);
		return(dum);
}
#endif
	while( x> 1)
		y*= x--;
	return( y);
}

#if !defined(linux) && !defined(__MACH__) && !defined(__APPLE_CC__)
extern char *sys_errlist[];
#endif

int errnocheck( xd, op, yd)
double xd, yd;
char *op;
{
	/* error while calculating	*/
		fprintf( stderr, "Error: %s - %g %s %g\n", serror(), xd, op, yd);
		return( 200);
}

int errnocheck1( xd, op)
double xd;
char *op;
{
	/* error while calculating (machine dependant!)	*/
		fprintf( stderr, "Error: %s - %g %s\n", serror(), xd, op);
		return( 200);
}

int calc( xd, op, yd, resd, resl, type)
double xd, yd, *resd;
long *resl;
int *type;
char *op;
{
	errno= 0;
    if( OP( "+")){
		if( OpCheck( &xd, op, &yd))			/* check for 2nd operand	*/
			return( 200);
        *resd=( xd+ yd);					/* calculation	*/
		*type= 1;							/* double result	*/
		next= 2;							/* next operand two ahead	*/
        return( 0);
    }
    if( OP( "-")){
		if( OpCheck( &xd, op, &yd))
			return( 200);
        *resd=( xd- yd);
		*type= 1;
		next= 2;
        return( 0);
    }
    if( OP( "x") || OP( "*") ){
		if( OpCheck( &xd, op, &yd))
			return( 200);
        *resd=( xd* yd);
		*type= 1;
		next= 2;
        return( 0);
    }
    if( OP( "/")){
		if( OpCheck( &xd, op, &yd))
			return( 200);
        if( yd== 0.){						/* check for errors	*/
            fprintf( stderr, "Error: Division by Zero - %g / %g\n", xd, yd);
#ifndef _UNIX_C_
            return( 200);
#endif
        }
        *resd=( xd/ yd);
		*type= 1;
		next= 2;
        return( 0);
    }
    if( OP( ":")){
		if( OpCheck( &xd, op, &yd))
			return( 200);
        if( yd== 0.){						/* check for errors	*/
            fprintf( stderr, "Error: Division by Zero - %g / %g\n", xd, yd);
#ifndef _UNIX_C_
            return( 200);
#endif
        }
		*resl= ( (long) xd / (long) yd );
		*type= 0;								/* result is a long	*/
		next= 2;
        return( 0);
    }
	if( OP( "mod")){
		if( OpCheck( &xd, op, &yd))
			return( 200);
		if( yd== 0.){
            fprintf( stderr, "Error: Division by Zero - %g mod %g\n", xd, yd);
            return( 200);
        }
		*resd= fmod( xd, yd);
        if( errno){							/* error while calculating (machine dependant!)	*/
            return( errnocheck( xd, op, yd));
        }
		*type= 1;
		next= 2;
		return( 0);
	}
    if( OP( "xx") || OP( "pow") || OP( "**")){
		if( OpCheck( &xd, op, &yd))
			return( 200);
        if( yd== 2.)						/* square (root)'s in the most accurate way	*/
            rd= xd* xd;
        else if( yd== 0.5)
            rd= sqrt( xd);
        else
            rd= SIGN(xd)* pow( FABS(xd), yd);
        if( errno){							/* error while calculating (machine dependant!)	*/
            return( errnocheck( xd, op, yd));
        }
        *resd=( rd);
		*type= 1;
		next= 2;
        return( 0);
    }
	if( OP( "exp")){
		ExArg( yd);							/* use only one operand	*/
		rd= exp( xd );
		if( errno)
			return( errnocheck( xd, op, yd));
		*resd= rd;
		*type= 1;
		next= 1;							/* next operand one ahead	*/
		return( 0);
	}
if( OP( "log")){
	if( OpCheck( &xd, op, &yd))
		return( 200);
if( yd<= 0.0 ){
    fprintf( stderr, "Error: invalid log argument - %g log >%g<\n", xd, yd);
    return( 200);
}
if( xd<= 0.0 || xd== 1.0 || log( xd)== 0.0){
    fprintf( stderr, "Error: invalid log argument - >%g< log %g\n", xd, yd);
    return( 200);
}
	if( errno)
		return( errnocheck( xd, op, yd));
*resd=( log( yd)/ log( xd) );
	*type= 1;
	next= 2;
return( 0);
}
if( OP( "arg")){
double tan;
	if( OpCheck( &xd, op, &yd))
		return( 200);
	tan= degrees( atan2( yd, xd));
	if( errno)
		return( errnocheck( xd, op, yd));
	*resd=( (tan< 0.0)? tan+ 360. : tan);
	*type= 1;
	next= 2;
	return( 0);
}
if( OP( "len")){
double len;
	if( OpCheck( &xd, op, &yd))
		return( 200);
	len= sqrt( xd*xd + yd*yd );
	if( errno)
		return( errnocheck( xd, op, yd));
	*resd= len;
	*type= 1;
	next= 2;
	return( 0);
}
if( OP( "atan")){						/* gonio functions: use degrees!	*/
double tan;
	ExArg( yd);							/* use only one operand	*/
	tan= degrees( atan2( xd, 1.));
	if( errno)
		return( errnocheck( xd, op, yd));
	*resd=( (tan< 0.0)? tan+ 360. : tan);
	*type= 1;
	next= 1;							/* next operand one ahead	*/
	return( 0);
}
if( OP( "tan")){
	ExArg( yd);
	*resd=( ( tan( radians( xd))));
	if( errno)
		return( errnocheck1( xd, op));
	*type= 1;
	next= 1;
	return( 0);
}
if( OP( "acos")){
	ExArg( yd);
	*resd=( degrees( acos( xd)));
	if( errno)
		return( errnocheck1( xd, op));
	*type= 1;
	next= 1;
	return( 0);
}
if( OP( "cos")){
	ExArg( yd);
	*resd=( ( cos( radians( xd))));
	if( errno)
		return( errnocheck1( xd, op));
	*type= 1;
	next= 1;
	return( 0);
}
if( OP( "asin")){
	ExArg( yd);
	*resd=( degrees( asin( xd)));
	if( errno)
		return( errnocheck1( xd, op));
	*type= 1;
	next= 1;
	return( 0);
}
if( OP( "sin")){
	ExArg( yd);
	*resd=( ( sin( radians( xd))));
	if( errno)
		return( errnocheck1( xd, op));
	*type= 1;
	next= 1;
	return( 0);
}
if( OP( "fac")){
	ExArg( yd);
	*resd= fac(xd);
	if( errno)
		return( errnocheck1( xd, op));
	*type= 1;
	next= 1;
	return( 0);
}
if( OP( "shr")){							/* functions working with integers	*/
	if( OpCheck( &xd, op, &yd))
		return( 200);
xl= (unsigned long) xd; yl= (unsigned long) yd;
*resl= ( xl >> yl);
		*type= 0;								/* result is a long	*/
		next= 2;
        return( 0);
    }
    if( OP( "shl")){
		if( OpCheck( &xd, op, &yd))
			return( 200);
        xl= (unsigned long) xd; yl= (unsigned long) yd;
        *resl= ( xl << yl);
		*type= 0;
		next= 2;
        return( 0);
    }
    if( OP( "or")){
		if( OpCheck( &xd, op, &yd))
			return( 200);
        xl= (unsigned long) xd; yl= (unsigned long) yd;
        *resl= ( xl | yl);
		*type= 0;
		next= 2;
        return( 0);
    }
    if( OP( "xor")){
		if( OpCheck( &xd, op, &yd))
			return( 200);
        xl= (unsigned long) xd; yl= (unsigned long) yd;
        *resl= ( xl ^ yl);
		*type= 0;
		next= 2;
        return( 0);
    }
    if( OP( "and")){
		if( OpCheck( &xd, op, &yd))
			return( 200);
        xl= (unsigned long) xd; yl= (unsigned long) yd;
        *resl= ( xl & yl);
		*type= 0;
		next= 2;
        return( 0);
    }
    if( OP( "not")){
        xl= (unsigned long) xd;
		ExArg( yd);
        *resl= ( ~xl);
		*type= 0;
		next= 1;
        return( 0);
    }
    fprintf( stderr, "Error: unknown operator - %g  >%s<  %g\n", xd, op, yd);
    return( 200);
}

