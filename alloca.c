/* 
	alloca -- (mostly) portable public-domain implementation -- D A Gwyn
	20020501: renamed to xgalloca by RJVB

	last edit:	86/05/30	rms
	   include config.h, since on VMS it renames some symbols.

	This implementation of the PWB library alloca() function,
	which is used to allocate space off the run-time stack so
	that it is automatically reclaimed upon procedure exit, 
	was inspired by discussions with J. Q. Johnson of Cornell.

	It should work under any C implementation that uses an
	actual procedure stack (as opposed to a linked list of
	frames).  There are some preprocessor constants that can
	be defined when compiling for your specific system, for
	improved efficiency; however, the defaults should be okay.

	The general concept of this implementation is to keep
	track of all alloca()-allocated blocks, and reclaim any
	that are found to be deeper in the stack than the current
	invocation.  This heuristic does not reclaim storage as
	soon as it becomes invalid, but it will do so eventually.

	As a special case, alloca(0) reclaims storage without
	allocating any.  It is a good idea to use alloca(0) in
	your main control loop, etc. to force garbage collection.
*/
#ifndef lint
static char	SCCSid[] = "$Id: @(#)alloca.c	1.1 $";	/* for the "what" utility */
#endif

#ifdef HAVE_CONFIG_H
#	include "config.h"
#endif

#ifndef _XGRAPH_H
#	include <stdio.h>
#	include <stdlib.h>

#endif

/* 20020418: a different way of allocation, that should never cause alignment problems: */
#undef RJVB

#ifdef emacs
#	ifdef static
/* actually, only want this if static is defined as ""
   -- this is for usg, in which emacs must undefine static
   in order to make unexec workable
   */
#		ifndef STACK_DIRECTION
you
lose
-- must know STACK_DIRECTION at compile-time
#		endif /* STACK_DIRECTION undefined */
#	endif /* static */
#endif /* emacs */

extern void	free();

/*
	Define STACK_DIRECTION if you know the direction of stack
	growth for your system; otherwise it will be automatically
	deduced at run-time.

	STACK_DIRECTION > 0 => grows toward higher addresses
	STACK_DIRECTION < 0 => grows toward lower addresses
	STACK_DIRECTION = 0 => direction of growth unknown
*/

#ifndef STACK_DIRECTION
#	define	STACK_DIRECTION	0		/* direction unknown */
#endif

#if STACK_DIRECTION != 0

#	define	STACK_DIR	STACK_DIRECTION	/* known at compile-time */

#else	/* STACK_DIRECTION == 0; need run-time code */

static int	stack_dir;		/* 1 or -1 once known */
#	define	STACK_DIR	stack_dir

static void find_stack_direction (/* void */)
{
  static char	*addr = NULL;	/* address of first
				   `dummy', once known */
  auto char	dummy;		/* to get stack address */

  if (addr == NULL)
    {				/* initial entry */
      addr = &dummy;

      find_stack_direction ();	/* recurse once */
    }
  else				/* second entry */
    if (&dummy > addr)
      stack_dir = 1;		/* stack grew upward */
    else
      stack_dir = -1;		/* stack grew downward */
}

#endif	/* STACK_DIRECTION == 0 */

/*
	An "alloca header" is used to:
	(a) chain together all alloca()ed blocks;
	(b) keep track of stack depth.

	It is very important that sizeof(header) agree with malloc()
	alignment chunk size.  The following default should work okay.
	20020418 RJVB: the default was just sizeof(double); this would of course
	\ not work since the struct{}h in the header was larger than a single double.
	\ Increasing the alignment correction to 2 doubles corrects this.
*/

#ifndef	ALIGN_SIZE
#	define	ALIGN_SIZE	2*sizeof(double)
#endif

#ifdef RJVB
 /* A header definition that has no alignment problems, but requires 2 allocations: */
typedef struct header{
	struct{
		struct header *next;
		char *deep;
		unsigned long size;
	} h;
	void *memory;
} header;
#else
typedef union hdr{
  char	align[ALIGN_SIZE];	/* to force sizeof(header) */
  struct
    {
      union hdr *next;		/* for chaining headers */
      char *deep;		/* for stack depth measure */
	 unsigned long size;
    } h;
} header;
#endif

/*
	alloca( size ) returns a pointer to at least `size' bytes of
	storage which will be automatically reclaimed upon exit from
	the procedure that called alloca().  Originally, this space
	was supposed to be taken from the current stack frame of the
	caller, but that method cannot be made to work for some
	implementations of C, for example under Gould's UTX/32.
*/

static header *last_alloca_header = NULL; /* -> last alloca header */

void *xgalloca (size, file, lineno)			/* returns pointer to storage */
     unsigned	int size;		/* # bytes to allocate */
	char *file;
	int lineno;
{
  auto char	probe;		/* probes stack depth: */
  register char	*depth = &probe;

#if STACK_DIRECTION == 0
  if (STACK_DIR == 0)		/* unknown growth direction */
    find_stack_direction ();
#endif

				/* Reclaim garbage, defined as all alloca()ed storage that
				   was allocated from deeper in the stack than currently. */

  {
    register header	*hp;	/* traverses linked list */

    for (hp = last_alloca_header; hp != NULL;)
      if ((STACK_DIR > 0 && hp->h.deep > depth)
	  || (STACK_DIR < 0 && hp->h.deep < depth))
	{
	  register header	*np = hp->h.next;

#ifdef DEBUG
	if( size ){
	  int i;
#ifdef RJVB
	  char *c= hp->memory;
#else
	  char *c= (char*) hp+ sizeof(header);
#endif
		fprintf(stderr,"xgalloca(%d)-%s,line %d: Freeing 0x%lx[%lu] { ", size, file, lineno, c, hp->h.size );
		for( i= 0; i< sizeof(int); i++ ){
			fprintf( stderr, "%x ", c[i] );
		}
		fprintf( stderr, "} \"" );
		for( i= 0; i< sizeof(int); i++ ){
			fprintf( stderr, "%c", c[i] );
		}
		fputs( "\"\n", stderr );
	}
#endif
#ifdef RJVB
	  free( hp->memory );
	  hp->memory= NULL;
#endif
	  free ((void *) hp);	/* collect garbage */

	  hp = np;		/* -> next header */
	}
      else
	break;			/* rest are not deeper */

    last_alloca_header = hp;	/* -> last valid storage */
  }

  if (size == 0)
    return NULL;		/* no allocation required */

  /* Allocate combined header + user data storage. */

  {
#ifdef RJVB
    register void *	new = malloc( sizeof (header) );
	if( new ){
		((header*)new)->memory= calloc( 1, size );
	}
#else
    register void *	new = calloc (1, sizeof (header) + size);
#endif
    /* address of header */

    ((header *)new)->h.next = last_alloca_header;
    ((header *)new)->h.deep = depth;
    ((header *)new)->h.size = size;

    last_alloca_header = (header *)new;

#ifdef RJVB
	return( ((header*)new)->memory );
#else
    /* User storage begins just after header. */
    return (void *)((char *)new + sizeof(header));
#endif
  }
}
