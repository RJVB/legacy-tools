#include <stdio.h>
#include <stdlib.h>
#include <cxxabi.h>

const char *demangle(const char *name)
{
	char buf[1024];
	size_t size = sizeof(buf) / sizeof(char);
	int status;

	return abi::__cxa_demangle( name, buf, &size, &status );
}

int main( int argc, char *argv[] )
{
	for( int i = 1 ; i < argc ; ++i ){
		fprintf( stdout, "%s -> \"%s\"\n", argv[i], demangle(argv[i]) );
	}
	exit(0);
}
