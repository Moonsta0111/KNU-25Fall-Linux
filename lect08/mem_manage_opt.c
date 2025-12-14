#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/resource.h>
#include <sys/wait.h>

#define MEM_SIZE (100 * 1024 * 1024)
#define PAGE_SIZE 4096

long get_minor_page_faults(){
	struct rusage usage;
	
	getrusage(RUSAGE_SELF, &usage);
	return usage.ru_minflt;
}

int main(){
	char *data = malloc(MEM_SIZE);
	memset(data, 1, MEM_SIZE);
	pid_t pid =fork();

	if(pid == 0) { // child
		long pf_before = get_minor_page_faults();
		volatile char tmp;
		for (int i = 0; i < MEM_SIZE; i += PAGE_SIZE) tmp = data[i];
		long pf_after_read =get_minor_page_faults();
		for (int i = 0; i < MEM_SIZE; i += PAGE_SIZE) data[i] = 2;
		long pf_after_write = get_minor_page_faults();
		free(data);
		printf("before_write : %ld\n",pf_before);
		printf("after_read   : %ld\n",pf_after_read);
		printf("after_write  : %ld\n",pf_after_write);
		exit(0);
	}else{
		wait(NULL);
	}
	free(data);
	return 0;
}

		
