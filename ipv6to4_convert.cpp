#include <stdio.h>
#include <iostream>
#include <string>
#include <sstream>

#include <string.h>
#include <strings.h>

#include <sys/types.h>

static int IPs= 0;

// Find and convert IPv6to4 addresses on *[Rr]eceived: lines, "unspoofing" them
// This is required currently for SpamCop's parser which can fail on such lines
// (and will, making it impossible to report spam received on GMail accounts).

std::string parse_msg( std::string &msg, int maxCount )
{   const char *pattern = "eceived: by 2002:";

    // we include the original link, so that would increase the total count with one. Avoid.
    maxCount-= 1;

    size_t ip6t4Header = msg.find(pattern);
    if (ip6t4Header != std::string::npos) {
        size_t spos, len = msg.length();
        do {
            size_t patBegin = msg.find("2002:", ip6t4Header);
            size_t patEnd = msg.find(" ", patBegin);
            if (patEnd != std::string::npos) {
//                 std::string ip6t4Address(msg.substr(patBegin, patEnd - patBegin));
                // get a stream on the start of the actual IPv4 address (= after the 2002: opcode)
                std::istringstream adrStream(msg.substr(patBegin+5, patEnd - patBegin));
                std::string adrHigh, adrLow;
                getline(adrStream, adrHigh, ':');
                getline(adrStream, adrLow, ':');
                if (adrHigh.length() <= 4 && adrLow.length() <= 4) {
                    // left-pad with 0(s) to reach length 4
                    size_t missing = 4 - adrHigh.length();
                    if (missing > 0) {
                        adrHigh.insert(0, missing, '0');
                    }
                    missing = 4 - adrLow.length();
                    if (missing > 0) {
                        adrLow.insert(0, missing, '0');
                    }
                    unsigned char ip4High[2], ip4Low[2];
                    // convert from hex to decimal, avoiding endianness:
                    ip4High[0] = stoi(adrHigh.substr(0,2), NULL, 16);
                    ip4High[1] = stoi(adrHigh.substr(2,4), NULL, 16);
                    ip4Low[0] = stoi(adrLow.substr(0,2), NULL, 16);
                    ip4Low[1] = stoi(adrLow.substr(2,4), NULL, 16);
                    std::string ip4Adr = std::to_string(ip4High[0]) + "." + std::to_string(ip4High[1])
                        + "." + std::to_string(ip4Low[0]) + "." + std::to_string(ip4Low[1]);
//                     fprintf( stderr, "IPv6=%s: ip4High=%d.%d ", ip6t4Address.c_str(), ip4High[0], ip4High[1] );
//                     fprintf( stderr, "ip4Low=%d.%d => %s\n", ip4Low[0], ip4Low[1], ip4Adr.c_str() );
                    // replace the IP6to4 address with a proper IPv4 address
                    msg.replace(patBegin, patEnd - patBegin, ip4Adr);
                    IPs += 1;
                }
                // jump to the next, if any
                spos = patEnd;
                ip6t4Header = msg.find(pattern, spos);
            } else {
                spos = len;
            }
        } while (spos < len && ip6t4Header != std::string::npos);
    }
    return msg;
}

int main( int argc, char *argv[] )
{   char c;
    std::string msg;
    int i, maxCount= -1;

    for( i= 1; i< argc; i++ ) {
        if( strcmp( argv[i], "-max")== 0 && i+1< argc ) {
            maxCount= atoi(argv[i+1]);
        }
    }

    // read the full message from stdin:
    while( std::cin.get(c) ) {
        msg+= c;
    }

    std::cout << parse_msg( msg, maxCount );

    std::cerr << "ipv6to4_convert: " << IPs << " *[rR]eceived: IP(s)\n";
    exit(0);
}
