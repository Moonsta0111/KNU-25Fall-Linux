#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <stdbool.h>
#include <errno.h>

#define NUM_CHILDREN 10
#define DEFAULT_TIME_QUANTUM 5   // 실험시 1, 2, 3, 4, 5 바꿔가며 수행
#define DEFAULT_SEED 82  // 고정 시드 기본값 실험시(42, 52, 62, 72, 82) 바꿔가며 반복 수행

// 프로세스 상태 정의
typedef enum {
    READY,
    RUNNING,
    SLEEP,
    DONE
} ProcessState;

// 자식 -> 부모 메시지 타입
typedef enum {
    MSG_BURST_DEC,
    MSG_IO_REQ,
    MSG_FINISHED
} ChildMsgType;

// 파이프 메시지 구조체
typedef struct {
    pid_t pid;
    ChildMsgType type;
} PipeMessage;

// PCB (부모가 관리)
typedef struct {
    pid_t pid;
    int remaining_tq;
    ProcessState state;
    int io_wait_time;
    int total_waiting_time;
    bool active;
    bool in_ready_q;   // READY 큐 중복 삽입 방지
} PCB;

// 전역 변수
PCB pcb_table[NUM_CHILDREN];
int current_running_idx = -1;
int global_time_quantum;
int pipe_fd[2];
int active_process_count = NUM_CHILDREN;
int time_ticks = 0;

unsigned int global_seed = DEFAULT_SEED; // 실험용 고정 시드


// READY FIFO 큐(원형 큐)
typedef struct {
    int data[NUM_CHILDREN];
    int front;
    int rear;
    int count;
} ReadyQueue;

static ReadyQueue rq;

static void rq_init(void) {
    rq.front = 0;
    rq.rear = 0;
    rq.count = 0;
}

static bool rq_empty(void) {
    return rq.count == 0;
}

static bool rq_full(void) {
    return rq.count == NUM_CHILDREN;
}

// READY 큐에 넣기(중복 삽입 방지)
static void rq_push(int idx) {
    if (idx < 0 || idx >= NUM_CHILDREN) return;
    if (!pcb_table[idx].active) return;
    if (pcb_table[idx].in_ready_q) return;
    if (rq_full()) return;

    rq.data[rq.rear] = idx;
    rq.rear = (rq.rear + 1) % NUM_CHILDREN;
    rq.count++;

    pcb_table[idx].in_ready_q = true;
}

// READY 큐에서 빼기
static int rq_pop(void) {
    if (rq_empty()) return -1;

    int idx = rq.data[rq.front];
    rq.front = (rq.front + 1) % NUM_CHILDREN;
    rq.count--;

    if (idx >= 0 && idx < NUM_CHILDREN) {
        pcb_table[idx].in_ready_q = false;
    }
    return idx;
}


// 함수 프로토타입
void parent_process(void);
void child_process(int id, int write_fd);
void handle_alarm(int sig);
void reset_all_time_quantums(void);
void print_performance(void);

// 메인
int main(int argc, char *argv[]) {
    // 1) 타임 퀀텀
    if (argc > 1) global_time_quantum = atoi(argv[1]);
    else global_time_quantum = DEFAULT_TIME_QUANTUM;

    // 2) 시드
    if (argc > 2) {
        global_seed = (unsigned int)strtoul(argv[2], NULL, 10);
    } else {
        global_seed = DEFAULT_SEED;
    }

    printf("[초기화] 시뮬레이션 시작! 타임 퀀텀: %d, 시드: %u\n",
           global_time_quantum, global_seed);

    rq_init();

    if (pipe(pipe_fd) == -1) {
        perror("pipe error");
        exit(1);
    }

    // 부모 RNG도 고정(부모가 I/O 대기시간 rand() 씀)
    srand(global_seed + 9999u);

    for (int i = 0; i < NUM_CHILDREN; i++) {
        pid_t pid = fork();

        if (pid == 0) {
            // 자식
            close(pipe_fd[0]);

            // 자식 RNG 고정: (같은 시드라도 자식마다 다른 난수 흐름이 나오게 id로 분기)
            srand(global_seed + (unsigned int)(i * 1000u) + 7u);

            child_process(i, pipe_fd[1]);
            exit(0);

        } else if (pid > 0) {
            // 부모 PCB 초기화
            pcb_table[i].pid = pid;
            pcb_table[i].remaining_tq = global_time_quantum;
            pcb_table[i].state = READY;
            pcb_table[i].io_wait_time = 0;
            pcb_table[i].total_waiting_time = 0;
            pcb_table[i].active = true;
            pcb_table[i].in_ready_q = false;

            rq_push(i); // 처음엔 전부 READY 큐에 넣기

        } else {
            perror("fork error");
            exit(1);
        }
    }

    close(pipe_fd[1]); // 부모는 읽기만
    parent_process();
    return 0;
}

// 자식 프로세스
static void child_signal_handler(int sig) {
    (void)sig;
}

void child_process(int id, int write_fd) {
    (void)id;

    int cpu_burst = (rand() % 10) + 1;

    // 시그널 핸들러
    struct sigaction sa;
    sa.sa_handler = child_signal_handler;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGUSR1, &sa, NULL);

    while (1) {
        pause();

        cpu_burst--;

        PipeMessage msg;
        msg.pid = getpid();

        if (cpu_burst <= 0) {
            int choice = rand() % 2; // 0: 종료, 1: I/O

            if (choice == 0) {
                msg.type = MSG_FINISHED;
                write(write_fd, &msg, sizeof(msg));
                exit(0);
            } else {
                msg.type = MSG_IO_REQ;
                cpu_burst = (rand() % 5) + 1;
                write(write_fd, &msg, sizeof(msg));
            }
        } else {
            msg.type = MSG_BURST_DEC;
            write(write_fd, &msg, sizeof(msg));
        }
    }
}

// 부모 프로세스
void parent_process(void) {
    struct sigaction sa;
    sa.sa_handler = handle_alarm;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGALRM, &sa, NULL);

    alarm(1);

    while (active_process_count > 0) {
        pause();
    }

    print_performance();

    while (wait(NULL) > 0) {}
    printf("[커널] 시스템 종료!\n");
}

// 타이머 핸들러
void handle_alarm(int sig) {
    (void)sig;

    time_ticks++;
    printf("\n============================\n");
    printf("=== 틱 %d ===\n", time_ticks);
    printf("============================\n");

    // 1) SLEEP 처리 + READY 대기시간 증가 + I/O 완료면 READY 큐로
    for (int i = 0; i < NUM_CHILDREN; i++) {
        if (!pcb_table[i].active) continue;

        if (pcb_table[i].state == SLEEP) {
            pcb_table[i].io_wait_time--;
            if (pcb_table[i].io_wait_time <= 0) {
                pcb_table[i].state = READY;
                rq_push(i);
                printf("[I/O] 프로세스 %d I/O 완료 -> READY(큐로)\n", pcb_table[i].pid);
            }
        } else if (pcb_table[i].state == READY) {
            pcb_table[i].total_waiting_time++;
        }
    }

    // 2) 현재 RUNNING 결과 처리
    if (current_running_idx != -1) {
        PCB *curr = &pcb_table[current_running_idx];

        PipeMessage msg;
        ssize_t bytes = read(pipe_fd[0], &msg, sizeof(msg));

        if (bytes > 0) {
            // 1틱 실행했으니 TQ 감소
            curr->remaining_tq--;

            if (msg.type == MSG_FINISHED) {
                curr->state = DONE;
                curr->active = false;
                active_process_count--;
                printf("[종료] 프로세스 %d 종료됨\n", curr->pid);
                current_running_idx = -1;

            } else if (msg.type == MSG_IO_REQ) {
                curr->state = SLEEP;
                curr->io_wait_time = (rand() % 5) + 1; // 부모 rand()도 시드 고정됨
                printf("[I/O] 프로세스 %d I/O 요청 (대기 %d초)\n", curr->pid, curr->io_wait_time);
                current_running_idx = -1;

            } else { // MSG_BURST_DEC
                printf("[실행] 프로세스 %d 1틱 실행 (남은 TQ: %d)\n", curr->pid, curr->remaining_tq);

                if (curr->remaining_tq <= 0) {
                    curr->state = READY;
                    rq_push(current_running_idx);
                    printf("[스케줄] 프로세스 %d TQ 소진 -> READY(큐 뒤로)\n", curr->pid);
                    current_running_idx = -1;
                }
            }
        }
    }

    // 3) 라운드 형태 TQ 전체 리셋
    bool all_zero_tq = true;
    for (int i = 0; i < NUM_CHILDREN; i++) {
        if (!pcb_table[i].active) continue;
        if (pcb_table[i].state == READY || pcb_table[i].state == RUNNING) {
            if (pcb_table[i].remaining_tq > 0) {
                all_zero_tq = false;
                break;
            }
        }
    }
    if (all_zero_tq && active_process_count > 0) {
        printf("[스케줄] 모든 프로세스 TQ가 0이라서 전체 TQ 초기화!\n");
        reset_all_time_quantums();
    }

    // 4) 디스패치: READY FIFO 큐에서 하나 꺼내기
    if (current_running_idx == -1 && active_process_count > 0) {
        int dispatched = 0;

        int tries = rq.count;
        for (int t = 0; t < tries; t++) {
            int idx = rq_pop();
            if (idx == -1) break;

            if (!pcb_table[idx].active) continue;
            if (pcb_table[idx].state != READY) continue;

            if (pcb_table[idx].remaining_tq <= 0) {
                rq_push(idx);
                continue;
            }

            current_running_idx = idx;
            pcb_table[idx].state = RUNNING;

            kill(pcb_table[idx].pid, SIGUSR1);
            printf("[디스패치] 프로세스 %d 실행 시작!\n", pcb_table[idx].pid);

            dispatched = 1;
            break;
        }

        if (!dispatched) {
            printf("[유휴] 지금 실행할 READY 프로세스가 없음\n");
        }
    } else if (current_running_idx != -1) {
        kill(pcb_table[current_running_idx].pid, SIGUSR1);
    }

    if (active_process_count > 0) alarm(1);
}

void reset_all_time_quantums(void) {
    for (int i = 0; i < NUM_CHILDREN; i++) {
        if (pcb_table[i].active) {
            pcb_table[i].remaining_tq = global_time_quantum;
        }
    }
}

void print_performance(void) {
    printf("\n======================================\n");
    printf(" 성능 분석 (타임 퀀텀: %d, 시드: %u)\n", global_time_quantum, global_seed);
    printf("======================================\n");
    printf("PID\tREADY 대기시간\n");
    printf("--------------------------------------\n");

    double total_wait = 0;
    int cnt = 0;

    for (int i = 0; i < NUM_CHILDREN; i++) {
        printf("%d\t%d초\n", pcb_table[i].pid, pcb_table[i].total_waiting_time);
        total_wait += pcb_table[i].total_waiting_time;
        cnt++;
    }

    printf("--------------------------------------\n");
    if (cnt > 0) printf("평균 대기시간: %.2f초\n", total_wait / cnt);
    else printf("실행된 프로세스가 없습니다.\n");
    printf("======================================\n");
}
