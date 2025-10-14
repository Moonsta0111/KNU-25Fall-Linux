#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#define MAXLINE 100

int main(){
	int n, length, fd[2];
	int pid;
	char message[MAXLINE], line[MAXLINE];

	pipe(fd); /*파이프 생성*/

	if ((pid = fork()) == 0){
			/* 자식 프로세스 생성*/
		close(fd[0]);
		sprintf(message, "Hello from PID %d\n",getpid());
		length = strlen(message)+1;
		write(fd[1], message, length);
	} else {
		/* 부모 프로세스 */
		close(fd[1]);
		n = read(fd[0], line, MAXLINE);
		printf("[%d] %s", getpid(), line);
	}

	exit(0);
}
