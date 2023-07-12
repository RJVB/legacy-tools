/* Ported to gcc/sun4/sunOS4 by Marshall Cline, sun.soe.clarkson.edu */
/*
From LISTSERV%UCF1VM.BITNET@ucf1vm.cc.ucf.edu Thu Mar 21 10:29:22 1991
Return-Path: <LISTSERV@ucf1vm.cc.ucf.edu>
Date:         Mon, 18 Mar 91 20:18:06 EDT
From: Revised List Processor (1.6e) <LISTSERV%UCF1VM.BITNET@ucf1vm.cc.ucf.edu>
Subject:      File: "XXDECODE C" being sent to you
To: cline@CHEETAH.ECE.CLARKSON.EDU

  *** XXDECODE  Version 1.0  date:  29. Jun 90

                  Karl-L. NOELL <NOELL@DWIFH1.BITNET>


The XX-coding scheme has been developed by Phil Howard <PHIL@UIUCVMD.BITNET>.

David J. Camp <david%wubios@wucfua.wustl.edu> contributed  C-sources firstly
dated July 1988.  These programs had shown some problems when compiled with
Borland Turbo-C 2.0

Meanwhile David Camp has modified his package (xxinstal.bat) for host and
PC environment.

Derived from David Camp's first sources, I've written a version for PCs under
MS-DOS.  Tests with Borland Turbo-C 2.0 and Microsoft C 5.1 had shown no
problems so far.

The following special characters are frequently in error due to
ASCII <--> EBCDIC conversion.  Please make the necessary corrections:

           curly brace         (opening): <{>   (closing): <}>
           rectangular bracket (opening): <[>   (closing): <]>
           vertical bar:                  <|>
           exclamation mark:              <!>
           backslash:                     <\>

----------------------------------------------------------------------------
   Revision History   -----xxdecode.c-----

(1.0):
90/06/29   o  Replaced printf by fprintf(stderr,
           +  tab[128] now: tab[256]  (handles ASCII and EBCDIC)
           +  several improvements concerning program structure.
           +  Indention only by leading blanks, no tabs to prevent
              e-mailing problems.
Beta Tests:
(0.9):
90/06/15   o  Substituted nonstandard fsize = (filelength (fileno(in));
              by: fseek (in,0L,SEEK_END);  fsize = ftell(in);
           +  Amount of progress (bargraph) calculated precisely.
           +  XXE files with fixed legth records (e.g. 61 XXE chars in
              "punchcard images", RECFM F, LRECL 80) couldn't be decoded
              because of trailing blanks.  Fixed by buf[80] --> buf[256]
              Bargraph 100% level not reachable in that case.
(0.8):
90/02/28   o  modified error check
(0.7):
90/02/12   o  Replace-flag-option removed.
           +  Improved prompting (replace existing file ?)
           +  Cancelled: chmod(dest,mode)
(0.6):
89/06/23   o  Added improved checks for illegal XXE characters.
89/06/07   o  Progress of XXE decoding is displayed by a bar graph
              (the 100 % level is inexact for small input files)
           +  Added checks to detect write errors (disk full).
--------------------------------------------------------------------------*/

/*      XXDECODE reads input from file.XXE,
*       decodes it into the binary stuff
*       and writes it out to the file, allocated
*       under the name specified in the XXE-Header
*/

#define VERSION "1.0  (29. Jun 90)"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

/* added by Marshall Cline, cline@sun.soe.clarkson.edu, March 1991: */
#ifndef SEEK_END
# define SEEK_END 2
#endif

static char set[] =
       "+-0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

static char table[256];     /*  handles ASCII and EBCDIC  */
long fsize,nrem;
int nrec=0;
char dest[128];

/* ==========================- start of main -========================= */
main (argc, argv)
int argc;
char *argv[];

{
    FILE *in, *out;
    int mode;
    char source[128], buf[256];

    fprintf (stderr,"\n***  XXDECODE  version %s\n",VERSION);

    if (argc != 2)     /* filename passed as command line parameter */
    {
        fprintf (stderr, "\nUsage: XXDECODE [d:][path]<xxefile>\n");
        exit(2);
    }

    strcpy (source, argv[1]);

    if ((in = fopen(source, "r")) == NULL)
    {
        perror(source);
        exit(1);
    }

    fseek (in,0L,SEEK_END);         /* determine size of sourcefile */
    fsize = ftell(in);
    fprintf(stderr,"\nsize of %s: %ld\n",source,fsize);
    rewind(in);

    for (;;)                   /* search for header line: begin ... */
    {
        nrec++;
        if (fgets(buf, sizeof buf, in) == NULL)
        {
             fprintf (stderr,
                      "'\abegin' line missing, XXE file corrupted ?\n");
             exit (3);
        }

        if (strncmp(buf, "begin ", 6) == 0)
             break;            /*  header line 'begin...' found     */

    }

    sscanf(buf, "begin %o %s", &mode, dest);
    nrem = (fsize - ftell(in) -8)/50;   /* 8: + CR LF end CR LF     */

    out = fopen (dest, "r");   /* check if outfile already exists   */

    if (out != NULL)
    {
         fprintf (stderr, "File <%s> already exists.\n", dest);

         do                    /*  prompting for replace            */
         {
              fprintf (stderr, "Replace? (Y or N): ");
              fscanf (stdin, "%s", buf);
              buf[0] = toupper (buf[0]);
         } while (buf[0] != 'N' && buf[0] != 'Y');

         fclose (out);
         if(buf[0] != 'Y') exit (7);
    }

    /* create or overwrite output file */
    out = fopen(dest, "wb");

    if (out == NULL)
    {
        perror(dest);
        exit(4);
    }

    fprintf (stderr, "Binary output goes to file: %s\n", dest);

    fprintf (stderr,
            "\n0..............................................100%%\n");


    decode(in, out);

    nrec++;
    if (fgets(buf,sizeof (buf), in) == NULL || strcmp (buf, "end\n"))
    {
         fprintf(stderr,
                "\n\a'end' line missing, XXE file corrupted ?\n");
         exit(5);
    }

    fclose (in);
    fclose (out);
    fprintf(stderr,"\ndone\n");

    exit (0);

} /* =======================- end   of main -========================= */


/***********************************************************************
*
*    decode:
*
***********************************************************************/

decode(in, out)

FILE *in, *out;

{
    char buf[256], *bp;
    int n,index;
    static int nk=0;

    /*  Initialization of the XXD conversion table: */

    for( n=0 ; n<256 ; n++ )
       table[n] = 99;               /*  clear table */

    for( n=0 ; n<64 ; n++ )
    {
        index = set[n] & 0xFF;

        if (index<0 || index>255)
        {
            fprintf(stderr,"\n\aillegal index: %d",index);
            exit (9);
        }

        table[index] = n;           /*  fill table  */
    }


    for (;;) /* . . . . . . . . . .  main loop  . . . . . . . . . . */
    {
        nrec++;     /* for each input line */

        if (fgets(buf, sizeof buf, in) == NULL)
        {
            fprintf(stderr,
                    "\n\aXXE-file corrupted: last '+' record missing\n");
           exit(10);
        }

        n = DEC(buf[0]);
        if (n == 0)
            break;            /*  last XXE record: '+' encountered  */

        /*   Confirmation, that the xxdecode is running,
             every (nrem = fsize/50) bytes a dot is displayed:      */

        nk = nk + 63;         /*  61 XXE chars + CR + LF            */

        if(nk>=nrem)
        {
            putchar('I');
            nk=nk - nrem;
        }

        bp = &buf[1];

        while (n > 0)
        {
            outdec(bp, out, n);
            bp += 4;
            n  -= 3;
        }
    }/* . . . . . . . . . . . bottom of main loop . . . . . . . . . */

} /*------------------------ end of decode ------------------------ */


/***********************************************************************
*
*       outdec
*
*  output a group of 3 bytes (4 input characters).
*  the input chars are pointed to by p, they are to
*  be output to file f.  n is used to tell us not to
*  output all of them at the end of the file.
*
***********************************************************************/

outdec(p, f, n)

char *p;
FILE *f;
int n;

{
    int b1, b2, b3,putflag;

    b1 = (DEC(  *p  ) << 2) | (DEC(*(p+1)) >> 4);
    b2 = (DEC(*(p+1)) << 4) | (DEC(*(p+2)) >> 2);
    b3 = (DEC(*(p+2)) << 6) | (DEC(*(p+3)));

    if (n >= 1)  putflag = putc(b1, f);
    if (n >= 2)  putflag = putc(b2, f);
    if (n >= 3)  putflag = putc(b3, f);

    if (putflag == EOF)
    {
        fprintf(stderr,
                "\n\aError: EOF writing %s (probably disk full)\n",dest);
        exit(6);
    }

} /* ------------------------- end of outdec ------------------------ */


/***********************************************************************
*
*    DEC:
*
*    Purpose:  decoding ( XXE character is entry to table[ ] )
*
***********************************************************************/

DEC (c)

int c;

{
    int num;

    num = table[c & 0x00FF];

    if (num == 99)
    {
        fprintf(stderr,
                "\a\n illegal character in XXE-File: %c (%02Xh)\n",c,c);
        fprintf(stderr," position is record no.: %d\n",nrec);
        exit(7);
    }

    return (num);

} /* -------------------------- end of DEC -------------------------- */


