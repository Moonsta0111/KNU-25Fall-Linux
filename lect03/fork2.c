#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

/*부모 프로세스가 자식 프로세스를 생성하고 서로 다른 메시지를 프린트*/
int main(){
	int pid;
	pid = fork();
	if(pid == 0) { // 자식프로세스
		printf("[Child] : Hello, pid = %d\n",getpid());
	}
	else{ // 부모 프로세스
	      printf("[Parent] : Hello, pid = %d\n",getpid());
	}
}
