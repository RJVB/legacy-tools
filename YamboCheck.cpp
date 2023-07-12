#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>
#include <string>
#include <sstream>

#include <strings.h>
#include <netdb.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#ifndef _AIX
	extern int h_errno;
#endif
extern char *h_errlist[];

#include "local/Macros.h"

IDENTIFY( "Little hack to lookup IP addresses for Yambo Financials sites that block DNS access from SpamCop.net. (c) 2007 RJVB" );


#if !defined(__MACH__) && !defined(__APPLE_CC__) && !defined(linux)

// case-insensitive substring lookup
char *strcasestr( const char *a,  const char *b)
{ unsigned int len= strlen(b), lena= strlen(a);
  int nomatch= 0, n= len;

	while( ( nomatch= (strncasecmp(a, b, len)) ) && n< lena ){
		a++;
		n+= 1;
	}
	return( (nomatch)? (char*) NULL : (char*) a );
}
#endif


// is <c> a valid character for a CNAME
int isiaddr(char c)
{
	return( isalnum(c) || (c== '-') || (c== '.') );
}

// for a CNAME (URI) in <c>, do we need to attempt to lookup the address(es) ourselves?
// This is the case for Yambo Financials sites, for instance, which block DNS lookups from SpamCop.net
// YF sites are typically in the .hk superdomain.
int name_needs_lookup( const char *c )
{ int ret= 0;
	if( c ){
		if( strncasecmp( &c[ strlen(c)-3 ], ".hk", 3)== 0 ){
			ret= 1;
		}
		else if( strncasecmp( &c[ strlen(c)-3 ], ".eu", 3)== 0 ){
			ret= -1;
		}
		else if( strncasecmp( &c[ strlen(c)-8 ], ".chat.ru", 8)== 0 ){
			ret= 1;
		}
		else if( strncasecmp( &c[ strlen(c)-7 ], ".com.es", 7)== 0 ){
			ret= 1;
		}
		else if( strncasecmp( &c[ strlen(c)-13 ], ".blogspot.com", 13)== 0 ){
			ret= 1;
		}
		else if( strncasecmp( &c[ strlen(c)-11 ], ".interia.pl", 11)== 0 ){
			ret= 1;
		}
		else if( strncasecmp( &c[ strlen(c)-14 ], ".geocities.com", 14)== 0 ){
			ret= 1;
		}
		else if( strcasestr( c, "putwish.com")
				|| strcasestr( c, "railweather.com")
				|| strcasestr( c, "drinkthought.com")
				|| strcasestr( c, "xpsoft.com")
				|| strcasestr( c, "comerica.")
				|| strcasestr( c, "estraynil.com")
				|| strcasestr( c, "bestratemedical.com")
				|| strcasestr( c, "coatyes.com")
				|| strcasestr( c, "geocities.com")
				|| strcasestr( c, "groups.yahoo.com")
				|| strcasestr( c, "profiles.yahoo.com")
				|| strcasestr( c, "googlegroups.com")
		){
		  // not actually a Yambo site, but one that could benefit from a similar treatment:
		  // (blogspot.com spamvertised sites)
			ret= 1;
		}
		if( ret ){
			std::cerr << "YamboCheck matching URI \"" << c << "\"\n";
		}
	}
	return( ret );
}

const char *addresslist= "200.171.139.77 ";

int listed_address( std::ostringstream &strm )
{ int ret= 0;
	if( strcasestr( addresslist, strm.str().c_str() ) ){
		ret= 1;
	}
	return(ret);
}

static int URIs= 0, IPs= 0;

// parse the message. If URIs are found that require or merit local DNS lookups, we add a link/reference to the message body.
// If the URI is given "texto" (not in HTML), just add the http://IP.AD.DR.E.SS , if given in HTML,
// add <a href="http://IP.AD.DR.E.SS">or here</a> hyperlinks after the </a> closing tag of the original hyperlink.

std::string parse_msg( char *msg, int maxCount )
{ std::ostringstream parsed;
  char *c= msg, *p= c;
  struct hostent *hent;

	  // we include the original link, so that would increase the total count with one. Avoid.
	maxCount-= 1;

	  // loop through the message
	while( p ){
		  // searching for http:// hyperlink tags
		  // NB: one could add support for www. implicit hyperlinks
		if( (p= strcasestr( c, "http://")) && p[7] ){
		  std::string name= "";
		  int onIP;
			c= &p[7];
			  // find the end of the URI's CNAME
			while( *c && isiaddr(*c) ){
				name+= *c++;
			}
			  // an HTML hyperlink (<a href="http://...."> etc) has a parenthesis just before the http tag:
// 			if( p== msg || p[-1]!= '"' )
			if( p== msg || strncasecmp( (const char*) &p[-6], "href=\"",6) )
			{
			  char last[2]= "";
				  // "texto" URI
				  // find the URI end, cut the message there...
				if( *c && isiaddr(c[-1]) ){
					last[0]= c[-1], last[1]= '\0';
					c[-1] = '\0';
					p= c;
				}
				else{
					p= (char*) NULL;
				}
				  // and queue the currently unqueued part:
 				parsed << msg << last;
				  // add the remainder of the URI:
				{ char *r= p;
					while( p && *r && !isspace(*r) ){
						parsed << *r;
						r++;
					}
				}
				if( (onIP= name_needs_lookup( name.c_str() )) || onIP<0 ){
					  // do the IP lookup, and add the link(s)
					URIs+= 1;
					hent= gethostbyname( name.c_str() );
					if( hent!= NULL && hent->h_addr_list ){
					  char **ha;
					  int n= 0;
 						parsed << " ";
						ha= hent->h_addr_list;
						while( *ha && (maxCount< 0 || n< maxCount) ){
						  char *a;
						  int j;
						  std::ostringstream adr;
							for( a= *ha, j= 0; j< hent->h_length; j++, a++ ){
//  								parsed << (unsigned int) (*a & 0x00ff);
 								adr << (unsigned int) (*a & 0x00ff);
								if( j< hent->h_length-1 ){
//  									parsed << ".";
 									adr << ".";
								}
							}
							if( onIP> 0 || listed_address(adr) ){
								parsed << "or http://";
								parsed << adr.str();
								  // add the remainder of the URI:
								{ char *r= p;
									while( !isspace(*r) && *r ){
										parsed << *r;
										r++;
									}
								}
								parsed << " ";
								IPs+= 1;
							}
							ha++;
							n+= 1;
						}
					}
					else{
#if defined(__MACH__) || defined(__APPLE_CC__) || defined(__CYGWIN__)
						herror( name.c_str() );
#endif
					}
				}
				  // if no lookup needs to be done (or it fails), just do nothing.
				msg= p;
			}
			else{
			  char *R= c;
				while( *c && strncasecmp( c, "</a>", 4) ){
					c++;
				}
				if( strncasecmp( c, "</a>", 4)== 0 ){
					c+= 3;
					*c++= '\0';
					p= c;
 					parsed << msg << ">";
				}
				else{
					p= (char*) NULL;
 					parsed << msg;
				}
				if( (onIP=name_needs_lookup( name.c_str() )) || onIP<0 ){
					URIs+= 1;
					hent= gethostbyname( name.c_str() );
					if( hent!= NULL && hent->h_addr_list ){
					  char **ha;
					  int n= 0;
 						parsed << " ";
						ha= hent->h_addr_list;
						while( *ha && (maxCount< 0 || n< maxCount) ){
						  char *a;
						  int j;
						  std::ostringstream adr;
							for( a= *ha, j= 0; j< hent->h_length; j++, a++ ){
//  								parsed << (unsigned int) (*a & 0x00ff);
 								adr << (unsigned int) (*a & 0x00ff);
								if( j< hent->h_length-1 ){
//  									parsed << ".";
 									adr << ".";
								}
							}
							if( onIP> 0 || listed_address(adr) ){
								parsed << "<a href=\"http://";
								parsed << adr.str();
								 // add the remainder of the URI:
								{ char *r= R;
									while( *r!= '"' && *r ){
										parsed << *r;
										r++;
									}
								}
								parsed << "\">or here</a> ";
								IPs+= 1;
							}
							ha++;
							n+= 1;
						}
					}
					else{
#if defined(__MACH__) || defined(__APPLE_CC__) || defined(__CYGWIN__)
						herror( name.c_str() );
#endif
					}
				}
				msg= p;
			}
		}
		else{
 			parsed << msg;
			p= (char*) NULL;
		}
	}

 	return( parsed.str() );
}

int main( int argc, char *argv[] )
{ char c;
  std::string msg;
  int i, maxCount= -1;
	
	for( i= 1; i< argc; i++ ){
		if( strcmp( argv[i], "-max")== 0 && i+1< argc ){
			maxCount= atoi(argv[i+1]);
		}
	}

	// read the full message from stdin:
	while( std::cin.get(c) ){
		msg+= c;
	}

	sethostent(1);
	std::cout << parse_msg( (char*) msg.c_str(), maxCount );
	endhostent();

	std::cerr << "YamboCheck: " << URIs << " matching URI(s), " << IPs << " IP(s)\n";
	exit(0);
}
