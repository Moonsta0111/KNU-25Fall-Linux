#include <stdlib.h>
#include <stdio.h>

void cleanup1(){
	printf("Cleanup 1 is called.\n");
}

void cleanup2(){
	printf("Cleanup 2 is called.\n");
}

int main(){
	atexit(cleanup1);
	atexit(cleanup2);

	exit(0);
}
