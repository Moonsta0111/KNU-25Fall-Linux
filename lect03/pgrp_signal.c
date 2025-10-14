#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>

int main(int argc, char *argv[])
{
	int child, pid, status;
	pid = fork();
	if(pid == 0){
		//자식 프로세스
		printf("PGRP of child = %d\n",getpgrp());

		while(1){
			printf("Child is waiting.. \n");
			sleep(1);
		}
	}
	else{//부모 프로세스
	     printf("PGRP of parent = %d\n", getpgrp());
	     
	     sleep(5);
	     //kill(pid, 9);
	     kill(-getpid(),9);
	     
	     printf("[%d] Child %d is teminated \n", getpid(), pid);
	     printf("\t ...with status %d\n", status>>8);

	     printf("Parent is sleeping...\n");
	     sleep(5);
	}
	return 0;
}

