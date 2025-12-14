#include <stdio.h>
#include <unistd.h>

void alarm_hander(int signo){
    printf("wake up\n");
}

int main(){
	alarm(5);

	signal(SIGALRM, alarm_handler);

	printf("loop.. \n");

	while(1){
		sleep(1);
		printf("1 sec.. \n");
	}

	printf("End of main \n");
}
