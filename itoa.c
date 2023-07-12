#include <stdio.h>
#include <stdlib.h>

void reverse(char *s)
{
	char *c;
	int i;
	
	c = s + strlen(s) - 1;
	while (s < c)
	{
		i = *s;
		*s++ = *c;
		*c-- = i;
	}
}

/*-----------------20/03/2002 12:14-----------------
*  fonction de conversion integer to decimal string
*  V0.1 / Olivier GALLAY
* --------------------------------------------------*/
char *itoa(int n, char *s)
{
	int sign;
	char *ptr;
	
	ptr = s;
	if ((sign = n) < 0)
		n = -n;
	do
	{
		*ptr++ = n % 10 + '0';
	}
	while ((n = n / 10) > 0);
	if (sign < 0)
		*ptr++ = '-';
	*ptr = '\0';
	reverse(s);
	return s;
}

/*-----------------16/06/2008 18:05-----------------
*  fonction de conversion unsigned integer to decimal string
*  V0.1 / RJVB
* --------------------------------------------------*/
char *utoa(unsigned int n, char *s)
{
	register char *ptr;
	
	ptr = s;
	do
	{
		*ptr++ = n % 10 + '0';
	}
	while ((n = n / 10) > 0);
	*ptr = '\0';
	reverse(s);
	return s;
}

main(int argc, char *argv[])
{ int i, val;
	for( i=0; i< argc; i++ ){
		if( sscanf( argv[i], "%d", &val)> 0 ){
		  char it[256], ut[256];
			fprintf( stdout, "%s(%d) itoa=%s utoa=%s\n", argv[i], val, itoa(val,it), utoa(val,ut) );
		}
	}
	exit(0);
}

