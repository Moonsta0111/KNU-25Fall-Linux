#include <stdio.h>
#include <stdlib.h>

int main() {
	int* arr = (int*)malloc(10*sizeof(int));
	for(int i = 0; i < 10; i++) arr[i] = i + 1;
	for(int i = 0; i < 10; i++) printf("%d\n",arr[i]);

	free(arr);

	for(int i = 0; i < 10; i++) printf("%d\n",arr[i]);
	return 0;
}
