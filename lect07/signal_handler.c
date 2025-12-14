#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>

void intHandler();
void exit_cnt();
int cnt = 0;

int main(){
	signal(SIGINT, intHandler);	

	while(1)
		pause();
	printf("End of main\n");
}

void intHandler(int signo){
	printf("SIGINT \n");
	printf("Sig No.: %d\n", signo);
	signal(SIGINT, SIG_DFL);

	exit(0);
}

