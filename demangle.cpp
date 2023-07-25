#include <stdio.h>
#include <stdlib.h>
#include <cxxabi.h>

const char *demangle(const char *name, int *status)
{
	char buf[1024];
	size_t size = sizeof(buf) / sizeof(char);

	return abi::__cxa_demangle( name, buf, &size, status );
}

int main( int argc, char *argv[] )
{ int status;
	for( int i = 1 ; i < argc ; ++i ){
		status = 0;
		fprintf( stdout, "%s -> \"%s\" (%d)\n", argv[i], demangle(argv[i], &status), status );
	}
	exit(0);
}
