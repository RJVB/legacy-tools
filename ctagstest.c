#define PIETJE

#ifdef __STDC__
int kk( char a, char b)
#else
int kk( a, b)
char a, b;
#endif
{}

#ifndef __STDC__
int kkk( a, b)
char a, b;
#else
int kkk( char a, char b)
#endif
{}

#ifdef __STDC__
Main( int ac, char *av[])
#else
main(ac,av)
int	ac;
char	*av[];
#endif
{}

#ifndef __STDC__
find_entries(file)
char	*file;
#else
find_entries( char *file)
#endif
{}
