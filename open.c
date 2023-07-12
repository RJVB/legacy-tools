#include <stdio.h>
#include <stdlib.h>
#include <strings.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <errno.h>

int main( int argc, const char *argv[] )
{
	int fd, flags = 0;
	char *fname;

	if (argc > 1) {
		if (strcasecmp(argv[1], "--help") == 0) {
			fprintf(stderr, "%s <filename> [flags]\n"
				"where flags is the ORed (summed) combination (in decimal or hex) of:\n", argv[0]);
			fprintf(stderr, "O_RDONLY: %d\n", O_RDONLY);
			fprintf(stderr, "O_WRONLY: %d\n", O_WRONLY);
			fprintf(stderr, "O_RDWR: %d\n", O_RDWR);
			fprintf(stderr, "O_APPEND: %d\n", O_APPEND);
			fprintf(stderr, "O_ASYNC: %d\n", O_ASYNC);
			fprintf(stderr, "O_CLOEXEC: %d\n", O_CLOEXEC);
			fprintf(stderr, "O_CREAT: %d\n", O_CREAT);
#ifdef O_DIRECT
			fprintf(stderr, "O_DIRECT: %d\n", O_DIRECT);
#endif
			fprintf(stderr, "O_DIRECTORY: %d\n", O_DIRECTORY);
			fprintf(stderr, "O_EXCL: %d\n", O_EXCL);
#ifdef O_LARGEFILE
			fprintf(stderr, "O_LARGEFILE: %d\n", O_LARGEFILE);
#endif
#ifdef O_NOATIME
			fprintf(stderr, "O_NOATIME: %d\n", O_NOATIME);
#endif
			fprintf(stderr, "O_NOCTTY: %d\n", O_NOCTTY);
			fprintf(stderr, "O_NOFOLLOW: %d\n", O_NOFOLLOW);
			fprintf(stderr, "O_NONBLOCK: %d\n", O_NONBLOCK);
			fprintf(stderr, "O_NDELAY: %d\n", O_NDELAY);
#ifdef O_PATH
			fprintf(stderr, "O_PATH: %d\n", O_PATH);
#endif
			fprintf(stderr, "O_SYNC: %d\n", O_SYNC);
			fprintf(stderr, "O_TRUNC: %d\n", O_TRUNC);
			exit(0);
		}
		fname = argv[1];
		if (argc > 2) {
			if (strncasecmp(argv[2], "0x", 2) == 0) {
				sscanf(&argv[2][2], "%x", &flags);
			} else {
				sscanf(argv[2], "%d", &flags);
			}
		}
		errno = 0;
		fd = open(fname, flags);
		fprintf(stderr, "open(\"%s\",0x%x) = %d", fname, flags, fd);
		if (fd < 0 || errno) {
			perror(" failed");
			exit(-1);
		} else {
			fputs("\n", stderr);
			close(fd);
		}
	}
	exit(0);
}

