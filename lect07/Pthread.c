#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

int acc = 0;

pthread_mutex_t mtx;

void *TaskCode(void *argument){
    int tid;
    tid = *((int*)argument);        // integer를 캐스팅
    int partial_acc = 0;
    for(int i = 0 ; i<1000000; i++){
        partial_acc++;
    }

    pthread_mutex_lock(&mtx);
    acc += partial_acc;
    pthread_mutex_unlock(&mtx);
    return NULL;
}

int main(int argc, char *argv[]){

    pthread_t threads[4];
    int args[4];
    int i;
    
    pthread_mutex_init(&mtx, NULL);

    // create all threads
    for(i=0; i<4; ++i){
        args[i] = i;
        pthread_create(&threads[i],NULL, TaskCode, (void*) &args[i]); //어떤 변수의 포인터를 보이드 포인터로 형변환해서 보내줌
    }

    // wait for all threads to complete
    for(i=0; i<4; i++){
        pthread_join(threads[i], NULL);
    }

    printf("%d \n", acc);    
    
    return 0;
}
    

