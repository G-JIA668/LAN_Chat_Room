#include <unistd.h>
#include <stdlib.h>	   //exit(0)
#include <stdio.h>	   //fgets
#include <cstring>	   //strlen
#include <sys/types.h> //waitpid
#include <sys/stat.h>  //mkfifo
#include <sys/wait.h>  //waitpid
#include <fcntl.h>	   //O_RDONLY
#include <errno.h>	   //errno
#include "include/fifo.h"

void inputbox(int writefd)
{
	size_t len;
	ssize_t n;
	char buff[MAXLINE];

	while (1)
	{
		fgets(buff, MAXLINE, stdin);
		len = strlen(buff);
		if(len==0)break;
		if (buff[len - 1] == '\n')
			len--;

		write(writefd, buff, len);
		
	}
}

int main()
{
	int writefd;

	if ((mkfifo(FIFO1, FILE_MODE) < 0) && (errno != EEXIST))
	{
		printf("can't create %s\n", FIFO1);
		return 0;
	}

	writefd = open(FIFO1, O_WRONLY, 0);

	inputbox(writefd);

	close(writefd);

	unlink(FIFO1);

	return 0;
}
//----------------------
