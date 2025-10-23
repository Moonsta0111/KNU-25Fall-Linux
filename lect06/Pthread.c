#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

int S[100], A[100], B[100];

void *TaskCode(void *argument){
    int tid;
    tid = *((int*)argument);        // integer 캐스팅
    for(int i = tid*25 ; i<(tid+1)*25; i++){ // 0 ~ 24개 25 ~ 49번 .. 100까지 총 4번 4개의 쓰레드 실행

        S[i] = A[i] + B[i]; 
    }
    return NULL;
}

int main(int argc, char *argv[]){

    pthread_t threads[4];
    int args[4];
    int i;

    for(i=0; i<100; i++){
        A[i] = i;
        B[i] = i;
    }

    // create all threads
    for(i=0; i<4; ++i){
        args[i] = i;
        pthread_create(&threads[i],NULL, TaskCode, (void*) &args[i]); //어떤 변수의 포인터를 보이드 포인터로 형변환해서 보내줌
        
    }
    // wait for all threads to complete
    for(i=0; i<4; i++){
        pthread_join(threads[i], NULL);
        // 메모리 어차피 공유하니까 나를 생성한 프로세스에게 전달할 필요가 없음
    }
    
    for(i =0; i<100; i++)
        printf("%d\n", S[i]);

    return 0;
}
    

