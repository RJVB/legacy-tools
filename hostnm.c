#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <errno.h>
#include <signal.h>

#define _INCLUDE_HPUX_SOURCE

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <netdb.h>

#ifndef _AIX
extern int h_errno;
#endif
extern char *h_errlist[];

#include "local/Macros.h"

IDENTIFY("hostname lookup utility, RJVB");

typedef struct StringList {
	char *text;
	struct StringList *last, *next;
} StringList;


/* name of executable */
char *exename;

/*
Copyright (c) 2001,2002                 RIPE NCC


All Rights Reserved

Permission to use, copy, modify, and distribute this software and its
documentation for any purpose and without fee is hereby granted,
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in
supporting documentation, and that the name of the author not be
used in advertising or publicity pertaining to distribution of the
software without specific, written prior permission.

THE AUTHOR DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS; IN NO EVENT SHALL
AUTHOR BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY
DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN
AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

/*
 * CONSTANTS
 */

/* default name to query - can be set at runtime via the "-h" option */
#define NICHOST "whois.ripe.net"

/* default port - only used if there is no entry for "whois" in the
   /etc/services file and no "-p" option is specified */
#define DEFAULT_WHOIS_PORT 43



/*
 * FUNCTIONS
 */


/*
  whois_query

  Writes the query string in "query" to the "out" descriptor, and reads
  the result in the "in" descriptor.  The "out" buffer may have either no
  buffering or line buffering, but must NOT have full buffering.

  The routine then outputs each line the server returns, until the server
  ends the connection.  If the "check_for_blank" variable is set to
  non-zero, then the routine will also return when two consecutive blank
  lines appear in the server response.

  If an error occurs sending or reading the query, -1 is returned.
 */

#include "ALLOCA.h"
#ifndef HAS_alloca
#	include "alloca.c"
#endif

int whois_query(FILE *in, FILE *out, const char *query, int check_for_blank)
{
	char buf[1024];
	int last_line_blank, lines = 0, dashk = !strcmp(query, "-k");
	ALLOCA(*qcopy, char, strlen(query) + 8, qlen);
	char *p, *query_copy = (char *) qcopy;

	/* manipulate a copy of the query */
	strcpy(query_copy, query);

	/* remove any newline or carriage return */
	p = strchr(query_copy, '\r');
	if (p != NULL) {
		*p = '\0';
	}
	p = strchr(query_copy, '\n');
	if (p != NULL) {
		*p = '\0';
	}

	/* add CR+LF */
	strcat(query_copy, "\r\n");

	/* send query */
	if (fputs(query_copy, out) == EOF) {
		fprintf(stderr, "%s: failure sending query '%s' (%s)\n", exename, query, strerror(errno));
		return -1;
	}

	/* wait for reply to finish, printing until then */
	last_line_blank = 0;
	for (;;) {
		/* read next line */
		if (fgets(buf, sizeof(buf), in) == NULL) {
			if (!lines) {
				fprintf(stderr, "%s: failure reading reply to '%s' (%s)\n", exename, query, strerror(errno));
			}
			return -1;
		}

		/* output the line */
		if (!dashk) {
			fputs(buf, stdout);
		}
		/* Accept the standard (??) "No match for " message as a failure. The return status is only used
		\ relative to the "-k" query that selects persistent mode on RIPE servers.
		\ Accepts some other responses too.
		*/
		if (dashk && (
		        strncasecmp(buf, "No match for ", 13) == 0 ||
		        (strncasecmp(buf, "% Server is running at low priority", 35) == 0 && strstr(buf, " -k "))
		        )
		   ) {
			return (-1);
		}

		/* if entire line fit in buffer */
		if (strchr(buf, '\n')) {
			/* if line is empty */
			if (!strcmp(buf, "\n")) {
				/* if the last line was also blank, we're done */
				if (check_for_blank && last_line_blank) {
					return 1;
				}
				last_line_blank = 1;
			}

			/* non-empty line */
			else {
				last_line_blank = 0;
			}
		}
		/* otherwise read until end of line */
		else {
			do {
				if (fgets(buf, sizeof(buf), in) == NULL) {
					return 0;
				}
				if (!dashk) {
					fputs(buf, stdout);
				}
			} while (!strchr(buf, '\n'));
			last_line_blank = 0;
		}
		lines += 1;
	}
	return (0);
}

int whois_fd = -1;
FILE *whois_in, *whois_out;

/* For closing down a connection. This is only called when persistent mode is not supported on the server.
 \ It is an attempt at cleanly closing; it is not clear from the manpages how to do this (e.g. fclose(whois_in)
 \ below *will* fail.
 */
void Close_WhoisS()
{
	if (whois_fd == -1) {
		return;
	}
	if (shutdown(whois_fd, 2)) {
		fprintf(stderr, "%s: failure shutting down socket: %s\n", exename, strerror(errno));
	}
	whois_fd = -1;
	if (fclose(whois_out)) {
#ifdef DEBUG
		fprintf(stderr, "%s: failure closing whois_out: %s\n", exename, strerror(errno));
#endif
	}
	if (fclose(whois_in)) {
#ifdef DEBUG
		fprintf(stderr, "%s: failure closing whois_in: %s\n", exename, strerror(errno));
#endif
	}
}

int NoPersistent(const char *whost)
{
	int r = 0;
	if (strcasecmp(whost, "whois.abuse.net") == 0 ||
	        strcasecmp(whost, "whois.geektools.com") == 0 ||
	        strcasecmp(whost, "whois.cyberabuse.org") == 0
	   ) {
		r = 1;
	}
	return (r);
}

/* query the whois-server at whost with either host <qhost>, or the hosts listed in <qhosts> */
int do_whois(const char *whost, const StringList *qhosts, const char *qhost)
{
	static signed char active = 0;

	/* server name and port to query */
	static long port = -1;

	/* connection information */
	static struct servent *serv;
	static struct hostent *h;
	static struct sockaddr_in host_addr;

	if (!qhosts && !qhost) {
		return (0);
	}

	if (whost == NULL) {
		whost = NICHOST;
	}
	/* active== 0 on 1st call, and -1 when selecting persistent mode failed (and connection has to be re-established): */
	if (!active || active == -1) {
		/* */
		/* arguments look good - connect to server */
		/* */

		if (!active) {
			/* blank out our address structure for broken (i.e. BSD) Unix variants */
			memset(&host_addr, 0, sizeof(struct sockaddr_in));

			/* get port address if not specified */
			if (port == -1) {
				serv = getservbyname("whois", "tcp");
				if (serv == NULL) {
					host_addr.sin_port = htons(DEFAULT_WHOIS_PORT);
				} else {
					host_addr.sin_port = serv->s_port;
				}
			} else {
				host_addr.sin_port = htons(port);
			}

			/* get server address (checking if it is an IP number first) */
			host_addr.sin_addr.s_addr = inet_addr(whost);
			if (host_addr.sin_addr.s_addr == -1) {
				h = gethostbyname(whost);
				if (h == NULL) {
#ifdef __CYGWIN__
					fprintf(stderr,
					        "%s: error getting server address for %s\n",
					        exename, whost);
#else
					fprintf(stderr,
					        "%s: error %d getting server address for %s\n",
					        exename, h_errno, whost);
#endif
					return (-1);
				}
				memcpy(&host_addr.sin_addr, h->h_addr, sizeof(host_addr.sin_addr));
			}

			/* fill in the rest of our socket structure */
			host_addr.sin_family = AF_INET;

		}
		/* create a socket */
		whois_fd = socket(AF_INET, SOCK_STREAM, 0);
		if (whois_fd == -1) {
			fprintf(stderr,
			        "%s: error %d creating a socket; %s\n",
			        exename,
			        errno,
			        strerror(errno));
			return (-1);
		}

		/* connect to the server */
		if (connect(whois_fd,
		            (struct sockaddr *)&host_addr,
		            sizeof(struct sockaddr_in)) != 0) {
			fprintf(stderr,
			        "%s: error %d connecting to server; %s\n",
			        exename,
			        errno,
			        strerror(errno));
			return (-1);
		}

		/* bind FILE structures to our file descriptor for easy handling */
		whois_in = fdopen(whois_fd, "r");
		if (whois_in == NULL) {
			fprintf(stderr,
			        "%s: error %d creating input stream; %s\n",
			        exename,
			        errno,
			        strerror(errno));
			return (-1);
		}
		setbuf(whois_in, NULL);
		whois_out = fdopen(whois_fd, "w");
		if (whois_out == NULL) {
			fprintf(stderr,
			        "%s: error %d creating input stream; %s\n",
			        exename,
			        errno,
			        strerror(errno));
			return (-1);
		}
		setbuf(whois_out, NULL);
		if (!active) {
			active = 1;
		}
		signal(SIGPIPE, SIG_IGN);
		if (active != -1) {
			int nocheck;
			errno = 0;
			if ((nocheck = NoPersistent(whost)) || whois_query(whois_in, whois_out, "-k", 1) == -1) {
				fprintf(stderr, "%s: unable to set multiple-query mode (%s)\n", exename, (nocheck) ? "known incompatibility" : strerror(errno));
				active = -1;
				/* Close down the connection from our side, and call ourselves with active=-1 */
				Close_WhoisS();
				return (do_whois(whost, qhosts, qhost));
			}
		}
	}

	{
		int hlen, llen, r = 0, n = 0;
		char *COLUMNS = getenv("COLUMNS");
		llen = (COLUMNS) ? atoi(COLUMNS) - 1 : 79;
		if (qhosts) {
			StringList *query = (StringList *) qhosts;
			while (query) {
				if (active == -1) {
					if (n == 0) {
						Close_WhoisS();
					}
					r += do_whois(whost, NULL, query->text);
					n += 1;
				} else {
					hlen = fprintf(stdout, "===== whois -h %s %s: ", whost, query->text);
					while (hlen < llen) {
						fputc('=', stdout);
						hlen += 1;
					}
					fputc('\n', stdout);
					r += whois_query(whois_in, whois_out, query->text, 1);
					n += 1;
					hlen = 0;
					while (query && hlen < llen) {
						fputc('=', stdout);
						hlen += 1;
					}
					fputc('\n', stdout);
				}
				query = query->next;
			}
		} else {
			hlen = fprintf(stdout, "===== whois -h %s %s: ", whost, qhost);
			while (hlen < llen) {
				fputc('=', stdout);
				hlen += 1;
			}
			fputc('\n', stdout);
			r = whois_query(whois_in, whois_out, qhost, 1);
			n = 1;
			hlen = 0;
			while (hlen < llen) {
				fputc('=', stdout);
				hlen += 1;
			}
			fputc('\n', stdout);
		}

		if (active == -1) {
			/* No persistent mode, thus we need to close down our side of the connection */
			Close_WhoisS();
		}

		return (r);
	}
}

/* ================================================== end whois code =============================================== */

int silent = 1, whois = 0, gai_protocol = IPPROTO_TCP;

StringList *wquery = NULL;

StringList *StringList_Delete(StringList **list)
{
	StringList *last = (*list);
	while (last) {
		(*list) = last;
		last = (*list)->next;
		if ((*list)->text) {
			free((*list)->text);
		}
		free((*list));
		(*list) = NULL;
	}
	return (last);
}

StringList *StringList_AddItem(StringList **list, char *text)
{
	StringList *new, *last;
	if (!list) {
		return (NULL);
	}
	if ((new = (StringList *) calloc(1, sizeof(StringList)))) {
		new->text = strdup(text);
		new->next = NULL;
		if ((last = (*list))) {
			if (last->last) {
				last = last->last;
			} else {
				while (last->next) {
					last = last->next;
				}
			}
			if (last) {
				last->next = new;
			}
			(*list)->last = new;
		} else {
			(*list) = new;
			(*list)->last = (*list);
		}
	}
	return ((*list));
}

char *fprint_h_errno(FILE *fp, char *name)
{
	static char msg[1024];
#ifdef __CYGWIN__
	snprintf(msg, sizeof(msg) / sizeof(msg[0]), "\t*** %s lookup error", name);
	herror(msg);
#else
	snprintf(msg, sizeof(msg) / sizeof(msg[0]), " [%s (%d): %s] ", name, h_errno, hstrerror(h_errno));
	fputs(msg, fp);
#endif
	fflush(fp);
	return (msg);
}

/* 20020903: copy routine for a hostent structure (gethostbyXXX() return a pointer to a static arena) */
struct hostent *hostcpy(struct hostent *host, struct hostent *Host)
{
	char **l;
	int n;
	*host = *Host;
	host->h_name = strdup(Host->h_name);
	l = Host->h_aliases;
	n = 1;
	while (*l) {
		n++;
		l++;
	}
	if ((host->h_aliases = (char **) calloc(n, sizeof(char *)))) {
		char **h = host->h_aliases;
		l = Host->h_aliases;
		while (*l) {
			*h = strdup(*l);
			h++;
			l++;
		}
	}
	l = Host->h_addr_list;
	n = 1;
	while (*l) {
		n++;
		l++;
	}
	if ((host->h_addr_list = (char **) calloc(n, sizeof(char *)))) {
		char **h = host->h_addr_list;
		l = Host->h_addr_list;
		while (*l) {
			*h = strdup(*l);
			h++;
			l++;
		}
	}
	return (host);
}

void free_host(struct hostent *host)
{
	free(host->h_name);
	if (host->h_aliases) {
		char **l = host->h_aliases;
		while (*l) {
			free(*l);
			l++;
		}
		free(host->h_aliases);
	}
	if (host->h_addr_list) {
		char **l = host->h_addr_list;
		while (*l) {
			free(*l);
			l++;
		}
		free(host->h_addr_list);
	}
	memset(host, 0, sizeof(struct hostent));
}

char *fprint_address(FILE *fp, char *iaddress, int length, int addrtype)
{
	static char address[INET_ADDRSTRLEN + INET6_ADDRSTRLEN + 1];
	struct hostent *Host, host2;
	void *iiaddr;
	socklen_t iiaddr_len = 0;
	struct in_addr addrv4;
	struct in6_addr addrv6;
	address[0] = '\0';
	switch (addrtype) {
		case AF_INET:
			inet_ntop(addrtype, iaddress, address, sizeof(address));
			break;
		case AF_INET6:
			inet_ntop(addrtype, iaddress, address, sizeof(address));
			break;
		default:
			snprintf(address, sizeof(address), "<unsupported addresstype %d>", addrtype);
			fprintf(fp, "%s", address);
			return (address);
	}
	if (silent == 2 || silent == 3) {
		fprintf(fp, "%s", address);
	} else {
		if (addrtype == AF_INET6) {
			if (inet_pton(AF_INET6, address, &addrv6) > 0) {
				iiaddr = &addrv6, iiaddr_len = sizeof(addrv6);
			}
		} else {
			if (inet_pton(AF_INET, address, &addrv4) > 0) {
				iiaddr = &addrv4, iiaddr_len = sizeof(addrv4);
			}
		}
		fprintf(fp, "%s (->", address);
		if (whois >= 3) {
			StringList_AddItem(&wquery, address);
		}
		fflush(fp);
		if ((Host = gethostbyaddr((char *) iiaddr, iiaddr_len, addrtype))) {
			hostcpy(&host2, Host);
			fprintf(fp, " %s) ", host2.h_name);
			if (whois >= 4) {
				StringList_AddItem(&wquery, host2.h_name);
			}
			free_host(&host2);
		} else {
			fprint_h_errno(fp, address);
			fputs(") ", fp);
		}
	}
	fflush(fp);
	return (address);
}


int main(int argc, char **argv)
{
	int i, addr, is_addr, is_ipv6;
	struct hostent *Host, host;
	char **l, *c, *whost = getenv("WHOISS");
	void *iaddr;
	socklen_t iaddr_len = 0;
	struct in_addr addrv4;
	struct in6_addr addrv6;
	static int Argc;

	if ((Argc = argc) < 2) {
		fprintf(stderr, "usage: [env WHOISS=whois.server.host] %s [-q|-qa|-v|-v2|-w|-w2|-w3|-w4] address..\n", argv[0]);
		exit(10);
	}
	exename = argv[0];
	sethostent(True);
	for (i = 1; i < Argc; i++) {
		if (!strcmp(argv[i], "-q")) {
			silent = 2;
			goto next_argument;
		} else if (!strcmp(argv[i], "-qa")) {
			silent = 3;
			goto next_argument;
		} else if (!strcmp(argv[i], "-v")) {
			silent = 1;
			goto next_argument;
		} else if (!strcmp(argv[i], "-v2")) {
			silent = 0;
			goto next_argument;
		} else if (!strcmp(argv[i], "-w")) {
			whois = 1;
			goto next_argument;
		} else if (!strcmp(argv[i], "-w2")) {
			whois = 2;
			goto next_argument;
		} else if (!strcmp(argv[i], "-w3")) {
			whois = 3;
			goto next_argument;
		} else if (!strcmp(argv[i], "-w4")) {
			whois = 4;
			goto next_argument;
		} else if (!strcmp(argv[i], "-u")) {
			gai_protocol = IPPROTO_UDP;
			goto next_argument;
		}

		is_addr = 1;
		for (c = argv[i]; *c && is_addr; c++) {
			if (!(isdigit(*c) || *c == '.' || *c == ':' || strchr("ABCDEFX", toupper(*c)))) {
				/* not an address, but a name	*/
				is_addr = 0;
// 				fprintf(stderr, "Not recognising \"%s\" as an address because of '%c'\n", argv[i], *c);
			}
		}
		iaddr = NULL, iaddr_len = 0;
		if (is_addr) {
			is_ipv6 = index(argv[i], ':') && !index(argv[i], '.');
			if (argv[i][strlen(argv[i]) - 1] != '.') {
				if (is_ipv6) {
					if (inet_pton(AF_INET6, argv[i], &addrv6) > 0) {
						iaddr = &addrv6, iaddr_len = sizeof(addrv6);
					}
				} else {
					if (inet_pton(AF_INET, argv[i], &addrv4) > 0) {
						iaddr = &addrv4, iaddr_len = sizeof(addrv4);
					}
				}
			}
			if (!iaddr) {
				fprintf(stdout, " [Can't construct an %s address from '%s' - trying as name]\n",
				        (is_ipv6) ? "IPv6" : "IPv4", argv[i]);
// 				goto next_argument;	
				is_addr = 0;
			}
		}
		if ((Host = (is_addr) ?
		            gethostbyaddr(iaddr, iaddr_len, (is_ipv6) ? AF_INET6 : AF_INET)
		            : gethostbyname(argv[i]))) {
			hostcpy(&host, Host);
			if (silent == 2 || silent == 3) {
				if (whois) {
					do_whois(whost, NULL, argv[i]);
				}
				if (!is_addr) {
					if ((l = host.h_addr_list)) {
						int j = 0, length = 0;
						char **alist;
						/* find the length of the list	*/
						while (l[length]) {
							length++;
						}
						if ((alist = (char **) calloc(length + 1, sizeof(char *)))) {
							for (j = 0; j < length; j++) {
								if ((alist[j] = (char *) calloc(host.h_length, 1))) {
									memcpy(alist[j], *l, host.h_length);
								}
								l++;
							}
							alist[j] = NULL;
							l = alist;
						}
						while (l && *l) {
							char *a = fprint_address(stdout, *l, host.h_length, host.h_addrtype);
							if (silent != 2 && l[1]) {
								fputs("  ", stdout);
							}
							if (whois >= 2) {
// 								do_whois( whost, NULL, a );
								StringList_AddItem(&wquery, a);
							}
							if (silent == 2) {
								l = NULL;
							} else {
								l++;
							}
						}
						if (alist) {
							for (j = 0; j < length; j++) {
								free(alist[j]);
							}
							free(alist);
							alist = NULL;
						}
					}
					fputc('\n', stdout);
				} else {
					fprintf(stdout, "%s\n", host.h_name);
					if (whois >= 2) {
// 						do_whois( whost, NULL, host.h_name );	
						StringList_AddItem(&wquery, host.h_name);
					}
				}
			} else {
				fprintf(stdout, "%s: %s\n", argv[i], host.h_name);
				if (whois) {
					StringList_AddItem(&wquery, argv[i]);
					if (whois >= 2) {
						StringList_AddItem(&wquery, host.h_name);
					}
				}
				l = host.h_aliases;
				if (*l) {
					fprintf(stdout, "\taliases:");
					while (*l) {
						fprintf(stdout, " %s", *l);
						l++;
					}
					fputc('\n', stdout);
				}
				addr = host.h_length;
				if (silent == 0) {
					fprintf(stdout, "\tAddress type/bytelength: %s/%d\n",
					        (host.h_addrtype == AF_INET6) ? "IPv6" : "IPv4", addr);
				}
				if ((l = host.h_addr_list)) {
					int j = 0, length = 0;
					char **alist;
					/* find the length of the list	*/
					while (l[length]) {
						length++;
					}
					fprintf(stdout, "\tAddress(es) (%d): ", length);
					fflush(stdout);
					if ((alist = (char **) calloc(length + 1, sizeof(char *)))) {
						for (j = 0; j < length; j++) {
							if ((alist[j] = (char *) calloc(addr, 1))) {
								memcpy(alist[j], *l, addr);
							}
							l++;
						}
						alist[j] = NULL;
						l = alist;
					}
					while (*l) {
						fprint_address(stdout, *l, addr, host.h_addrtype);
						l++;
					}
					fputc('\n', stdout);
					if (alist) {
						for (j = 0; j < length; j++) {
							free(alist[j]);
						}
						free(alist);
						alist = NULL;
					}
				}
				if (silent == 0 && iaddr && !is_ipv6) {
					fprintf(stdout, "\tEthernet address: 0x%lx\n", addrv4.s_addr);
				}
				/* we have shown anything we wanted, so now see what the reverse
				 \ mapping will do... Only if user supplied address: address2name
				 \ mapping of all addresses of supplied hostnames is already done.
				 */
				if (is_addr) {
					fprintf(stdout, "\tReverse mapping %s:\n", host.h_name);
					if ((Host = gethostbyname(host.h_name))) {
						fprintf(stdout, "\t%s -> %s ", host.h_name, Host->h_name);
						fflush(stdout);
						fprint_address(stdout, *(Host->h_addr_list), Host->h_length, Host->h_addrtype);
						fputc('\n', stdout);
					} else {
						fprint_h_errno(stdout, host.h_name);
						fputc('\n', stdout);
					}
				}
			}
			fflush(stdout);
			free_host(&host);
			if (whois) {
				do_whois(whost, wquery, NULL);
				StringList_Delete(&wquery);
			}
		} else {
			fprint_h_errno(stdout, argv[i]);
			fputc('\n', stdout);
			if (whois) {
				do_whois(whost, NULL, argv[i]);
			}
		}
		if (silent != 2 && silent != 3) {
			struct addrinfo *res = NULL, hints;
			memset(&hints, 0, sizeof(hints));
			hints.ai_flags = AI_ADDRCONFIG|AI_V4MAPPED|AI_ALL|AI_CANONNAME;
			hints.ai_family = AF_UNSPEC;
			hints.ai_protocol = gai_protocol;
			int gai_ret = getaddrinfo(argv[i], NULL, &hints, &res);
			if (gai_ret == 0) {
				struct addrinfo *r = res;
				int n = 0;
				fprintf(stdout, "## Data from getaddrinfo(\"%s\") for %s:\n",
						argv[i], (gai_protocol == IPPROTO_TCP)? "TCP" : "UDP");
				while (r) {
					char ipaddr[INET6_ADDRSTRLEN+INET_ADDRSTRLEN+1];
			        if (r->ai_addr->sa_family == AF_INET) {
			            struct sockaddr_in *addr4 = (struct sockaddr_in *) r->ai_addr;
			            inet_ntop(AF_INET, &addr4->sin_addr, ipaddr, INET_ADDRSTRLEN);
			        }
			        else if (r->ai_addr->sa_family == AF_INET6) {
			            struct sockaddr_in6 *addr6 = (struct sockaddr_in6 *) r->ai_addr;
			            inet_ntop(AF_INET6, &addr6->sin6_addr, ipaddr, INET6_ADDRSTRLEN);
			        }
					fprintf(stdout, "\t%s [%s] (flags=%d fam=%s prot=%d)\n",
							(n == 0)?
								(r->ai_canonname && strcmp(r->ai_canonname, ipaddr))?
									r->ai_canonname : "[Unknown host]"
								: "\t",
							ipaddr,
							r->ai_flags, (r->ai_family == AF_INET6)? "IPv6" : "IPv4",
							r->ai_protocol);
					r = r->ai_next, ++n;
				}
				freeaddrinfo(res);
			} else {
				fprintf(stderr, "## Error from getaddrinfo(\"%s\") for %s: %s\n",
						argv[i], (gai_protocol == IPPROTO_TCP)? "TCP" : "UDP", gai_strerror(gai_ret));
			}

			fprintf(stdout, "--------\n");
		}
next_argument:;
	}
	endhostent();
	exit(0);
}
