#include <stdio.h>
#include <stdlib.h>

int glob = 1;
int main(){
	static int stat = 2;
	int stack;
	char *heap = (char *)malloc(100);
	printf("Stack	= %p", &stack);
	printf("Heap	= %p", heap);
	printf("Global	= %p", &glob);
	printf("Static	= %p", &stat);
	printf("main	= %p", main);

	return 0;
}
