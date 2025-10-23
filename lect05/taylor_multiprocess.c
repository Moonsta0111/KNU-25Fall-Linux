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

	for(int i=0; i< num_elements; i++){
		pipe(fd+2*i);

		child_id = i;
		pid = fork();

		// 자식 프로세스에서 또 fork 되는 것을 방지
		if(pid == 0){
			break;
		} else {
			// 부모 프로세스에서만 쓰기 닫기
			close(fd[2*i+1]);
		}
	}

	// 자식 프로세스에서 병렬적으로 sinx 테일러 급수 수행
	if(pid == 0){
		close(fd[2*child_id]);

		double value = x[child_id];
		double numer = x[child_id] * x[child_id] * x[child_id];
		double denom = 6;
		int sign = -1;

		for(int j = 1; j <= terms ; j++){
			
			value += (double)sign * numer / denom;
			numer *= x[child_id] * x[child_id];
			denom *= (2.*(double)j + 2.) * (2.*(double)j + 3.);
			sign *= -1;
		}
		
		result[child_id] = value;
		sprintf(message, "&lf", result[child_id]);
		length =strlen(message) + 1;
		write(fd[2*child_id+1], message, length);

		exit(child_id);
	}
	else{

		for( int i = 0; i < num_elements; i++){
			int status;
			// 자식프로세스 중에 하나가 끝날 때까지 기다린다.
			// wait함수는 어떤 자식하나만 끝나도 프로세스가 종료되게하는 것.
			// status로 지정해주어 자식 프로세스 4개가 모두 끝나면 종료.
			wait(&status); 
			int child_id = status >> 8;
			read(fd[2*child_id], line, MAXLINE);
			result[child_id] = atof(line); // atof -> ASCII to Float
		}
	}
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


/*
# 설명
밑 코드는 자식프로세스를 4개 생서한다.
하지만, 4개를 생성하고 처리하지만, for문 안에서 로직이 돌아가므로
결과는 똑같지만 각 로직은 순차적으로 실행된다.

void sinx_taylor(int num_elements, int terms, double* x, double* result){
	int fd[2*N], length;
	int child_id, pid;
	char message[MAXLINE], line[MAXLINE];

    
    for(int i=0; i<num_elements; i++){
        pipe(fd+2*i);

        int pid = fork();
    
        if( pid==0 ){ //child
            close(fd[2*i]);
            
            double value = x[i];
            double numer = x[i] * x [i] * x[i];
            double denom = 6;
            int sign = -1;
            
            for (int j =1; j <= terms; j++){

                value += (double)sign * numer /denom;
                numer *= x[i] * x[i];
                denom *= (2.(double)j +2.) * (2.*(double)j +3.);
                sign *= -1;
            }
            result[i] = value;
            sprintf(message, "&lf", result[i]);
            length = strlen(message) + 1;
            write(fd[2*i+1], message, length);

            exit(0);

        }
        else { //parent
            close(fd[2*i + 1]);
            read(fd[2*i], line, MAXLINE);

            result[i] = atof(line);
        
        }
	}
}
*/