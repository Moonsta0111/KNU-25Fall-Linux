#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#define _USE_MATH_DEFINES
#define N 4
#define MAXLINE 100


void sinx_taylor(int num_elements, int terms, double* x, double* result){
    int fd[2*N], length;
    int child_id, pid;
    char message[MAXLINE], line[MAXLINE];

    // 진짜 병렬이려면 -> fork를 먼저 수행해서 자식을 만들어놓고
    for(int i=0; i<num_elements; i++){
        pipe(fd+2*i);

        child_id = i;
        pid = fork();

        if(pid == 0){ break; }
        else{ close(fd[2*i+1]); }
    }

    // 자식들 수행 후
    if( pid == 0 ){ //child
        int i = child_id; // 나중에는 바꿔주기
        close(fd[2*i]);
        
        double value = x[i];
        double numer = x[i] * x [i] * x[i];
        double denom = 6; 
        int sign = -1;
        
        for (int j =1; j <= terms; j++){

            value += (double)sign * numer /denom;
            numer *= x[i] * x[i];
            denom *= (2.*(double)j +2.) * (2.*(double)j +3.);
            sign *= -1;
        }
        result[i] = value;
        sprintf(message, "&lf", result[i]);
        length = strlen(message) + 1;
        write(fd[2*i+1], message, length);

        exit(child_id);
    }
    else { //parent
        // 자식 프로세스가 끝날떄까지 기다린다.
        for(int i = 0; i<num_elements; i++){
            int status;
            wait(&status); // 자식프로세스 중에 하나가 끝날 떄까지 기다린다. - status 수행하면서 4개 지나면 끝나게해줌
            int child_id = status >> 8;
            read(fd[2*child_id], line, MAXLINE);
            result[child_id] = atof(line);
        }
    
    }


    // 밑 코드는 자식 병렬 처리 후 수행
    // for(int i=0; i<num_elements; i++){
    //     pipe(fd+2*i);

    //     int pid = fork();
    
    //     if( pid==0 ){ //child
    //         close(fd[2*i]);
            
    //         double value = x[i];
    //         double numer = x[i] * x [i] * x[i];
    //         double denom = 6;
    //         int sign = -1;
            
    //         for (int j =1; j <= terms; j++){

    //             value += (double)sign * numer /denom;
    //             numer *= x[i] * x[i];
    //             denom *= (2.(double)j +2.) * (2.*(double)j +3.);
    //             sign *= -1;
    //         }
    //         result[i] = value;
    //         sprintf(message, "&lf", result[i]);
    //         length = strlen(message) + 1;
    //         write(fd[2*i+1], message, length);

    //         exit(0);

    //     }
    //     else { //parent
    //         close(fd[2*i + 1]);
    //         read(fd[2*i], line, MAXLINE);

    //         result[i] = atof(line);
        
    //     }

    // }

}

int main(){
        double x[N] = {0, M_PI/6., M_PI/3., 0.314};
        double res[N];

        sinx_taylor(N,3, x, res);

        for(int i = 0; i<N; i++){
                printf("sin(%.2f) by Taylor series = %f\n", x[i], res[i]);
                printf("sin(%.2f) = %f\n", x[i], sin(x[i]));
        }

        return 0;
}
            