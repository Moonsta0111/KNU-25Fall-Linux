#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#define _USE_MATH_DEFINES
#define N 4

void sinx_taylor(int num_elements, int terms, double* x, double* result){
	int pipes[num_elements][2];    // 각각 수행할 자식의 개수
	pid_t pids[num_elements];      // 자식 PID 저장
	
	for(int i=0; i< num_elements; i++){
		pipe(pipes[i]);
		pid_t pid = fork();
		

		// 자식프로세스
		if (pid == 0) {
			close(pipes[i][0]);  // 읽기 닫기
			
			// 테일러 급수
			double value = x[i];
			double numer = x[i] * x[i] * x[i];
			double denom = 6;
			int sign = -1;

			for( int j = 1; j <= terms; j++){
				value += (double)sign * numer / denom;
				numer *= x[i] * x[i];
				denom *= (2. *(double)j+2.) * (2.*(double)j+3.);
				sign *= -1;
			}
			
			// 쓰기 수행
			write(pipes[i][1], &value, sizeof(value));

			close(pipes[i][1]); // 쓰기 종료
			exit(0);	    // 자식 종료
		}
		// 부모프로세스
		else {
			pids[i] = pid;
			close(pipes[i][1]); // 쓰기 종료	
		}
	}

	// 자식으로부터 파이프 결과 받기
	for(int i = 0; i < num_elements; i++){
		double val;
		read(pipes[i][0], &val, sizeof(val)); // 결과 읽기 수행
		close(pipes[i][0]); // 읽기 종료
		result[i] = val;
	}
	
	// 자식 회수
	for (int i = 0; i < num_elements; i++){
		waitpid(pids[i], NULL, 0);
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
