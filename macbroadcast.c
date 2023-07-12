/*
 * macbroadcast.c -- idea:  send a broadcast to mac whenever I receive
 * mail.
 * eventually, send the mail.

 * loosely based on atlook.c which is included in cap50 distribution.
 * by Patrick Beard
 * Lawrence Berkeley Laboratory
 *
 * Edit History:
 *
 *  1988	Beard		Created.
 *  2 Dec 88	bowman		Cleaned up and reworked.
 *
 * Beard is beard@ux1.lbl.gov (Patrick C Beard)
 * bowman is bowman@science.utah.edu (Pieter Bowman)
 */

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/param.h>
#include <netinet/in.h>         	/* so htons() works for non-vax */
#include <netat/appletalk.h>        /* include appletalk definitions */
#if defined(AUX)
/* AUX wants this:	*/
#	include <at/appletalk.h>
#	include <at/atp.h>
#	include <at/nbp.h>
#	include <mac/sysequ.h>
#elif defined(__APPLE_CC__) || defined(__MACH__)
#	include <netat/appletalk.h>
#	include <netat/atp.h>
#	include <netat/nbp.h>
#endif

typedef struct BroadcastPacket {	/* what the mac wants to see */
	char message[256];
	long icon[4];
} BroadcastPacket;

BroadcastPacket bcPacket;
ABusRecord abRecord;
BDS myBDS;


/* routines from atlook.
 * atlook - UNIX AppleTalk test program: lookup entities
 * with "ATLOOKLWS" defined, will also get pap status and confirm address.
 * with "ATPINGER" defined will "ping" remote with "echo request"
 *
 * AppleTalk package for UNIX (4.2 BSD).
 *
 * Copyright (c) 1986, 1987 by The Trustees of Columbia University in the
 * City of New York.
 *
 * Edit History:
 *
 *  June 13, 1986    Schilit	Created.
 *  Dec  23, 1986    Schilit	Sort result, display in columns, clean up.
 *				add usage and options.
 *  Mar  16, 1987    CCKim	Clean up some more, merge with looks and pinger
 *
 */

#define NUMNBPENTRY 100			/* max names we can lookup */

/* vars */
char *deftype = "BroadCast";		/* default entity type */

int nbpretry = 3;		/* 3 retries */
int nbptimeout = 3;		/* 3/4 second by default */


usage(s)
char *s;
{
  fprintf(stderr,"usage: %s [-d FLAGS] [-t <n>] [-r <n>] \
<name to send to> <message to send>\n",s);
  fprintf(stderr,"\t <name to send to> is a registered Broadcast name.\n");
  fprintf(stderr,"\t                   eg. \"bowman:@Mathematics\"\n");
  fprintf(stderr,"\t                   The zone name may be a name or\n");
  fprintf(stderr,"\t                   either \"*\" (my zone) or \"=\"\n");
  fprintf(stderr,"\t                   all zones.\n");
  fprintf(stderr,"\t <message to send> text you want to send.\n");
  fprintf(stderr,"\t -r <n> - specifies number of retries (>=0)\n");
  fprintf(stderr,"\t -t <n> - timeout between retries\n");
  fprintf(stderr,"\t -d FLAGS - cap library debugging flags\n");
  fprintf(stderr,"\tTimeouts are in ticks (1/4 units)\n");
  exit(1);
}

getnum(s)
char *s;
{
  int r;
  if (*s == '\0')
    return(0);
  r = atoi(s);
  if (r == 0 && *s != '0')
    return(-1);
  return(r);
}

tickout(n)
int n;
{
  int t = n % 4;		/* ticks */
  int s = n / 4;		/* seconds */
  int m = s / 60;		/* minutes */

  if (m != 0) {
    if (m >= 60)		/* an hour??????? */
      printf(" (are you crazy?)");
    printf(" %d %s", m, m > 1 ? "minutes" : "minute");
    s %= 60;			/* reset seconds to remainder */
  }
  if (s)			/* print seconds if any */
    printf(" %d",s);
  if (t) {			/* print ticks */
    if (s)
      printf(" and");
    if (t == 2) 
      printf(" one half");
    else
      printf(" %d fourths",t);
  }
  if (s || t)
    printf(" second");
  if (s > 1 || (t && s))
    printf("s");
}

doargs(argc,argv)
int argc;
char **argv;
{
  char *whoami = argv[0];
  extern char *optarg;
  extern int optind;
  int c;

  while ((c = getopt(argc, argv, "d:D:r:t:")) != EOF) {
    switch (c) {
    case 'd':
    case 'D':
      dbugarg(optarg);		/* some debug flags */
      break;
    case 'r':
      nbpretry = getnum(optarg);
      if (nbpretry <= 0)
	usage(whoami);
      printf("Number of NBP retries %d\n",nbpretry);
      break;
    case 't':
      nbptimeout = getnum(optarg);
      if (nbptimeout < 0)
	usage(whoami);
      printf("NBP Timeout");
      tickout(nbptimeout);
      putchar('\n');
      break;
    case '?':
    default:
      usage(whoami);
    }
  }
  return(optind);
}

my_create_entity(s, en)
char *s;
EntityName *en;
{
  create_entity(s, en);	/* must be fully specified name */
  if (*en->objStr.s == '\0')
    en->objStr.s[0] = '=';
  if (*en->typeStr.s == '\0')
    strcpy(en->typeStr.s, deftype); /*  to lookup... */
  if (*en->zoneStr.s == '\0')
    en->zoneStr.s[0] = '*';
}

#define NUMZONES 100

void zone_dolookup(enp, message)
  EntityName *enp;			/* network entity name */
  char *message;
{
  char *zones[NUMZONES];	/* room for pointers to zone names */
  OSErr err;
  int i, cnt;
  EntityName en;
  char user[L_cuserid];		/* who is sending the packet */
  char host[MAXHOSTNAMELEN];	/* where sent from */
  char from[L_cuserid+1+MAXHOSTNAMELEN];
  char *tptr;

  (void)cuserid(user);		/* Return the sending user's name */
  if (gethostname(host, sizeof(host))) {
    fprintf(stderr, "Unable to get host name\n");
    return;
  }
  if ((tptr = strchr(host, '.')) != NULL) /* Truncate host name */
    *tptr = '\0';

  strcpy(from, user);		/* Build from string foo@bar */
  strcat(from, "@");
  strcat(from, host);

  /* get message to send */
  strcpy(bcPacket.message+1, message, sizeof(bcPacket.message) - strlen(from) - 3);
  strcat(bcPacket.message+1, "\321");/* delimiting character 0xd1 */
  strcat(bcPacket.message+1, from);
  bcPacket.message[0] = strlen(bcPacket.message+1);

  if (strcmp(enp->zoneStr.s, "=") == 0) {
    if ((err = GetZoneList(zones, NUMZONES, &cnt)) != noErr) {
      fprintf(stderr, "error %d getting zone list\n", err);
      return;
    }
    for (i = 0; i < cnt; i++) {
      strcpy(en.objStr.s, enp->objStr.s);
      strcpy(en.typeStr.s, enp->typeStr.s);
      strcpy(en.zoneStr.s, zones[i]);
      dolookup(&en, message);
    }
  } else {
    dolookup(enp, message);
  }
}

main(argc,argv)
int argc;
char **argv;
{
  int i;
  EntityName en;			/* network entity name */

  abInit(TRUE);				/* initialize appletalk driver */
  nbpInit();				/* initialize nbp */
  checksum_error(FALSE);		/* ignore these errors */

  i = doargs(argc,argv);		/* handle arguments */

  if (i+2 > argc) {
    usage(argv[0]);
  } else {
    my_create_entity(argv[i], &en);
    zone_dolookup(&en, argv[i+1]);
  }
}

dolookup(en,message)
EntityName *en;			/* network entity name */
char *message;
{
  int err,i, len;
  AddrBlock addr;		/* Address of entity */
  NBPTEntry nbpt[NUMNBPENTRY];	/* table of entity names */
  nbpProto nbpr;		/* nbp protocol record */
  char name[sizeof(EntityName)*4*3+3];	/* for formatted entity name */
				/* 3 entries, of size entity name */
				/* each char can be up to 4 in length */
				/* +3 for :@ and null */
  char zone_name[sizeof(EntityName)*4];
  char *calloc();

  /* allocate memory for the BDS */
  myBDS.buffSize = 384;
  if ((myBDS.buffPtr = calloc(1, 384)) == NULL) {
    fprintf(stderr, "Error allocating BDS buffer\n");
    return;
  }

  dumptostr(zone_name, en->zoneStr.s); /* save the zone name for later */

  nbpr.nbpRetransmitInfo.retransInterval = nbptimeout;
  nbpr.nbpRetransmitInfo.retransCount = nbpretry;
  nbpr.nbpBufPtr = nbpt;		/* place to store entries */
  nbpr.nbpBufSize = sizeof(nbpt);	/* size of above */
  nbpr.nbpDataField = NUMNBPENTRY;	/* max entries */
  nbpr.nbpEntityPtr = en;		/* look this entity up */

  if (dbug.db_flgs) {
    len = dumptostr(name, en->objStr.s);
    name[len++] = ':';
    len += dumptostr(name+len, en->typeStr.s);
    name[len++] = '@';
    dumptostr(name+len, en->zoneStr.s);
    printf("Looking for %s ...\n",name);
  }

  /* Find all objects in specified zone */
  err = NBPLookup(&nbpr,FALSE);		/* try synchronous */
  if (err != noErr) {
    fprintf(stderr,"NBPLookup returned err %d\n",err);
    return;
  }

  /* Extract, print and send the message(s) */
  for (i = 1; i <= nbpr.nbpDataField; i++) {
    NBPExtract(nbpt,nbpr.nbpDataField,i,en,&addr);
    len = dumptostr(name, en->objStr.s);
    name[len++] = ':';
    len += dumptostr(name+len, en->typeStr.s);
    name[len++] = '@';
    dumptostr(name+len, en->zoneStr.s);

    if (dbug.db_flgs) {
      printf("%02d - %-40s [Net:%3d.%02d Node:%3d Skt:%3d]\n",
	     i, name, ntohs(addr.net)>>8, ntohs(addr.net)&0xff, addr.node,
	     addr.skt);
    }

    strcpy(name+len, zone_name);
    printf("%-60s", name);

#define ABRECORD abRecord.proto.atp
    /* set up the data structures */
    ABRECORD.atpSocket = 0;
    ABRECORD.atpAddress = addr;
    ABRECORD.atpReqCount = 1 + strlen(bcPacket.message+1); /* sizeof packet */
    ABRECORD.atpDataPtr = (char*) &bcPacket;
    ABRECORD.atpRspBDSPtr = &myBDS;
    ABRECORD.atpUserData = 0;
    ABRECORD.fatpXO = 1;
    ABRECORD.fatpEOM = 1;
    ABRECORD.atpTimeOut = 10;
    ABRECORD.atpRetries = 1;	/* was 8 */
    ABRECORD.atpNumBufs = 1;

    /* send the message */
    ATPSndRequest(&abRecord, FALSE);
    if (abRecord.abResult == noErr)
      printf(" sent\n");
    else
      printf(" result:  %d\n",abRecord.abResult);

  }
  free(myBDS.buffPtr);
}

dumptostr(str, todump)
char *str;
char *todump;
{
  char c;
  int i = 0;

  while ((c = *todump++)) {
    if (c > 0 && isprint(c)) {
      *str++ = c;
      i++;
    } else {
      sprintf(str,"\\%3o",c&0xff);
      str+=4;
      i+=4;
    }
  }
  *str = '\0';
  return(i);
}
