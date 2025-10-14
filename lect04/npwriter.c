#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#define MAXLINE 100
/* 이름 있는 파이프를 통해 메시지를 출력한다. */

int main(){
	int fd, length, i;
	char message[MAXLINE];

	sprintf(message, "Hello from PID %d", getpid());

	length = strlen(message)+1;

	do{
		fd = open("myPipe", O_WRONLY);
		if(fd == -1) sleep(1);
	}while (fd == -1);

	for (i = 0; i < 3; i++){
		write(fd, message, length);
		sleep(3);
	}
	close(fd);

	return 0;
}
