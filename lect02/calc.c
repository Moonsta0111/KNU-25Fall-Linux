#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[])
{
	char op = argv[2][0];
	int x = atoi(argv[1]);
	int y = atoi(argv[3]);
	
	switch(op){
		case '+':
			printf("%d\n", x + y);
			break;
		case '-':
			printf("%d\n", x - y);
			break;
		case 'x':
			printf("%d\n", x * y);
			break;
		case '/':
			printf("%d\n", x / y);
			break;
	}

	return 0;
}
