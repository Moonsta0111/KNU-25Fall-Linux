#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>


int t = 10;
int ans = 123;
int in = 0;
void alarm_handler(){
	printf("fire ~~!!\n");

	exit(0);
}

void intHandler(int signo){
	printf("10 sec reset...\n");
	alarm(10);

}

int main(){

	printf("폭탄 실행\n");
        
	alarm(10);
	signal(SIGALRM, alarm_handler);
	signal(SIGINT, intHandler);
	scanf("%d", &in);
	if( in == ans ){
		printf("Correct! \n");
	}
}
