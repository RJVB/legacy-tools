#ifndef ALLOCA

/* NB: some gcc versions (linux 2.9.?) don't like a declaration of the type 
 \ type x[len+1]
 \ claiming that the size of x is too large. Bug or feature, it can be remedied
 \ by using x[len+2] instead of x[len+1], *or* by x[LEN], where LEN=len+1.
 */

#define CHKLCA(len,type)	len

#define HAS_alloca

#	ifdef __GNUC__
#		define XGALLOCA(x,type,len,xlen,dum)	type x[CHKLCA(len,type)]; int xlen= sizeof(x)
#		define ALLOCA(x,type,len,xlen)	type x[CHKLCA(len,type)]
#		define _ALLOCA(x,type,len,xlen)	type x[CHKLCA(len,type)]; int xlen= sizeof(x)
#		define GCA()	/* */
#	else
	/* #	define LMAXBUFSIZE	MAXBUFSIZE*3	*/
#		ifndef STR
#		define STR(name)	# name
#		endif
#		define XGALLOCA(x,type,len,xlen,dum)	static int xlen= -1; static type *x=NULL; type *dum=(type*) alloca((void**)&x,CHKLCA(len,type),&xlen,sizeof(type),STR(x))

#		ifdef HAS_alloca
#			include <alloca.h>
#			define ALLOCA(x,type,len,xlen)	int xlen=(CHKLCA(len,type))* sizeof(type); type *x= (type*) alloca(xlen)
#			define _ALLOCA(x,type,len,xlen)	ALLOCA(x,type,CHKLCA(len,type),xlen)
#			define GCA()	/* */
#		else
			extern void *xgalloca(unsigned int n);
#			define ALLOCA(x,type,len,xlen)	int xlen=(CHKLCA(len,type))* sizeof(type); type *x= (type*) xgalloca(xlen,__FILE__,__LINE__)
#			define _ALLOCA(x,type,len,xlen)	ALLOCA(x,type,CHKLCA(len,type),xlen)
#			define GCA()	xgalloca(0,__FILE__,__LINE__)
#		endif

#	endif

#if !defined(sgiKK) && !defined(alloca)
#	define _REALLOCA(x,type,len,xlen)	xlen=(CHKLCA(len,type))* sizeof(type); x= (type*) xgalloca(xlen,__FILE__,__LINE__)
#else
#	define _REALLOCA(x,type,len,xlen)	xlen=(CHKLCA(len,type))* sizeof(type); x= (type*) xgalloca(xlen)
#endif

#if !defined(__GNUC__) && !defined(HAS_alloca)
#	undef alloca
#	define alloca(n) xgalloca((n),__FILE__,__LINE__)
#endif

#endif
