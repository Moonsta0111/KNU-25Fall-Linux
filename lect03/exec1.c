#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

/* echo 명령어를 실행한다. */
int main()
{
	printf("-> Before exec()\n");
	execl("/bin/echo","echo","hello",NULL);
	printf("--> After exec()\n");
	return 0;

}
