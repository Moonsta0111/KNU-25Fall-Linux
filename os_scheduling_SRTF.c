#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <stdbool.h>
#include <errno.h>
#include <limits.h>

#define NUM_CHILDREN 10
#define DEFAULT_TIME_QUANTUM 1   // SRTF는 QUANTUM 값 중요 X
#define DEFAULT_SEED 82          // 고정 시드 기본값 (42, 52, 62, 72, 82 등)


// 프로세스 상태 정의
typedef enum {
    READY,
    RUNNING,
    SLEEP,
    DONE
} ProcessState;

// 자식 -> 부모 메시지
typedef enum {
    MSG_INIT,       // 초기 CPU 버스트 보고
    MSG_BURST_DEC,  // 1 tick 실행 완료
    MSG_IO_REQ,     // I/O 요청
    MSG_FINISHED    // 종료
} ChildMsgType;

// 파이프 메시지 구조체
typedef struct {
    pid_t pid;
    ChildMsgType type;
    int remaining_burst; // 자식이 보고하는 "남은 CPU 버스트"
} PipeMessage;

// PCB (부모가 관리)
typedef struct {
    pid_t pid;
    ProcessState state;
    int io_wait_time;
    int total_waiting_time;
    bool active;

    int remaining_burst;   // SRTF 핵심: 남은 CPU 버스트(부모가 알고 있어야 함)
} PCB;

// 전역 변수
PCB pcb_table[NUM_CHILDREN];
int current_running_idx = -1;
int global_time_quantum;
int pipe_fd[2];
int active_process_count = NUM_CHILDREN;
int time_ticks = 0;

unsigned int global_seed = DEFAULT_SEED;

// 함수
void parent_process(void);
void child_process(int id, int write_fd);
void handle_alarm(int sig);
void print_performance(void);

static int find_idx_by_pid(pid_t pid);
static void drain_init_messages(void);
static int pick_srtf_ready(void);

// 메인
int main(int argc, char *argv[]) {
    // 1) 타임 퀀텀(호환용)
    if (argc > 1) global_time_quantum = atoi(argv[1]);
    else global_time_quantum = DEFAULT_TIME_QUANTUM;

    // 2) 시드
    if (argc > 2) global_seed = (unsigned int)strtoul(argv[2], NULL, 10);
    else global_seed = DEFAULT_SEED;

    printf("[초기화] 시뮬레이션 시작! (SRTF) tq인자: %d, 시드: %u\n",
           global_time_quantum, global_seed);

    // 파이프 생성 (자식 -> 부모)
    if (pipe(pipe_fd) == -1) {
        perror("pipe error");
        exit(1);
    }

    // 부모 RNG 고정 (부모가 I/O 대기시간 rand() 씀)
    srand(global_seed + 9999u);

    // 자식 생성 + PCB 초기화
    for (int i = 0; i < NUM_CHILDREN; i++) {
        pid_t pid = fork();

        if (pid == 0) {
            // 자식
            close(pipe_fd[0]); // 읽기 닫기

            // 자식 RNG 고정: 자식마다 난수 흐름 분리
            srand(global_seed + (unsigned int)(i * 1000u) + 7u);

            child_process(i, pipe_fd[1]);
            exit(0);

        } else if (pid > 0) {
            // 부모: PCB 초기화
            pcb_table[i].pid = pid;
            pcb_table[i].state = READY;
            pcb_table[i].io_wait_time = 0;
            pcb_table[i].total_waiting_time = 0;
            pcb_table[i].active = true;
            pcb_table[i].remaining_burst = -1; // 아직 모름(MSG_INIT로 채움)

        } else {
            perror("fork error");
            exit(1);
        }
    }

    // 부모는 읽기만
    close(pipe_fd[1]);

    // 자식들이 보내는 초기 CPU 버스트(MSG_INIT) 10개를 먼저 받아서
    // pcb_table[].remaining_burst 채움
    drain_init_messages();

    // 부모(커널) 실행
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

    // 1) 초기 버스트를 부모에게 먼저 보고(MSG_INIT)
    PipeMessage init;
    init.pid = getpid();
    init.type = MSG_INIT;
    init.remaining_burst = cpu_burst;
    write(write_fd, &init, sizeof(init));

    // SIGUSR1 핸들러 등록
    struct sigaction sa;
    sa.sa_handler = child_signal_handler;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGUSR1, &sa, NULL);

    while (1) {
        // 부모가 SIGUSR1 보내면 1 tick 실행
        pause();

        // 1 tick 실행
        cpu_burst--;

        PipeMessage msg;
        msg.pid = getpid();

        if (cpu_burst <= 0) {
            // 버스트 종료 -> 종료 or I/O 요청(랜덤)
            int choice = rand() % 2; // 0: 종료, 1: I/O

            if (choice == 0) {
                msg.type = MSG_FINISHED;
                msg.remaining_burst = 0;
                write(write_fd, &msg, sizeof(msg));
                exit(0);
            } else {
                msg.type = MSG_IO_REQ;

                // I/O 후 다음 CPU 버스트 재할당(시뮬레이션)
                cpu_burst = (rand() % 5) + 1;

                // 부모가 "다음 남은 버스트"를 알 수 있게 같이 보냄
                msg.remaining_burst = cpu_burst;
                write(write_fd, &msg, sizeof(msg));
            }
        } else {
            msg.type = MSG_BURST_DEC;
            msg.remaining_burst = cpu_burst; // SRTF용으로 남은 버스트 보고
            write(write_fd, &msg, sizeof(msg));
        }
    }
}

// 부모 프로세스
void parent_process(void) {
    // 타이머(SIGALRM) 핸들러
    struct sigaction sa;
    sa.sa_handler = handle_alarm;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGALRM, &sa, NULL);

    // 1초마다 tick
    alarm(1);

    while (active_process_count > 0) {
        pause();
    }

    print_performance();

    while (wait(NULL) > 0) {}
    printf("[커널] 시스템 종료!\n");
}

// 타이머 핸들러: SRTF 스케줄러
void handle_alarm(int sig) {
    (void)sig;

    time_ticks++;
    printf("\n============================\n");
    printf("=== 틱 %d (SRTF) ===\n", time_ticks);
    printf("============================\n");

    // 1) SLEEP 처리 + READY 대기시간 증가
    for (int i = 0; i < NUM_CHILDREN; i++) {
        if (!pcb_table[i].active) continue;

        if (pcb_table[i].state == SLEEP) {
            pcb_table[i].io_wait_time--;
            if (pcb_table[i].io_wait_time <= 0) {
                pcb_table[i].state = READY;
                printf("[I/O] 프로세스 %d I/O 완료 -> READY\n", pcb_table[i].pid);
            }
        } else if (pcb_table[i].state == READY) {
            pcb_table[i].total_waiting_time++;
        }
    }

    // 2) 이전 tick에 RUNNING이었던 프로세스의 결과 수신
    if (current_running_idx != -1) {
        PCB *curr = &pcb_table[current_running_idx];

        PipeMessage msg;
        ssize_t bytes = read(pipe_fd[0], &msg, sizeof(msg));

        if (bytes > 0) {
            // 정상적으로 "현재 RUNNING이었던 프로세스"가 보낸 메시지라고 가정
            if (msg.type == MSG_FINISHED) {
                curr->remaining_burst = 0;
                curr->state = DONE;
                curr->active = false;
                active_process_count--;
                printf("[종료] 프로세스 %d 종료됨\n", curr->pid);
                current_running_idx = -1;

            } else if (msg.type == MSG_IO_REQ) {
                // 자식이 다음 버스트를 msg.remaining_burst로 함께 보고
                curr->remaining_burst = msg.remaining_burst;

                curr->state = SLEEP;
                curr->io_wait_time = (rand() % 5) + 1; // 부모 rand()도 시드 고정
                printf("[I/O] 프로세스 %d I/O 요청 (대기 %d초), 다음 버스트=%d\n",
                       curr->pid, curr->io_wait_time, curr->remaining_burst);

                current_running_idx = -1;

            } else if (msg.type == MSG_BURST_DEC) {
                // 남은 버스트 업데이트
                curr->remaining_burst = msg.remaining_burst;

                // SRTF는 매 tick마다 "남은 버스트 최소"를 다시 선택하므로
                // 현재 프로세스도 READY로 돌려놓고 후보로 포함
                curr->state = READY;

                printf("[실행] 프로세스 %d 1틱 실행, 남은 버스트=%d\n",
                       curr->pid, curr->remaining_burst);

                current_running_idx = -1;
            }
        } else {
            // 메시지를 못 읽은 경우(이상 상황) - 다음 tick에 재시도
            current_running_idx = -1;
        }
    }

    // 3) SRTF 선택: READY 중 남은 버스트가 가장 작은 프로세스 선택
    if (active_process_count > 0) {
        int next = pick_srtf_ready();

        if (next != -1) {
            current_running_idx = next;
            pcb_table[next].state = RUNNING;

            kill(pcb_table[next].pid, SIGUSR1);
            printf("[디스패치] SRTF 선택 -> 프로세스 %d (남은 버스트=%d)\n",
                   pcb_table[next].pid, pcb_table[next].remaining_burst);
        } else {
            printf("[유휴] READY 프로세스가 없음\n");
        }
    }

    if (active_process_count > 0) alarm(1);
}

// pid -> idx 찾기
static int find_idx_by_pid(pid_t pid) {
    for (int i = 0; i < NUM_CHILDREN; i++) {
        if (pcb_table[i].pid == pid) return i;
    }
    return -1;
}

// 유틸: 자식들의 초기 버스트(MSG_INIT) 10개 수신
static void drain_init_messages(void) {
    int got = 0;
    while (got < NUM_CHILDREN) {
        PipeMessage msg;
        ssize_t bytes = read(pipe_fd[0], &msg, sizeof(msg));
        if (bytes <= 0) continue;

        if (msg.type != MSG_INIT) {
            // 초기화 단계에서 다른 메시지가 오면 무시(과제용 단순 처리)
            continue;
        }

        int idx = find_idx_by_pid(msg.pid);
        if (idx == -1) continue;

        pcb_table[idx].remaining_burst = msg.remaining_burst;
        // 초기 상태는 READY 유지
        got++;

        printf("[초기버스트] 프로세스 %d 초기 CPU 버스트=%d\n",
               pcb_table[idx].pid, pcb_table[idx].remaining_burst);
    }
}

static int pick_srtf_ready(void) {
    int best = -1;
    int best_burst = INT_MAX;

    for (int i = 0; i < NUM_CHILDREN; i++) {
        if (!pcb_table[i].active) continue;
        if (pcb_table[i].state != READY) continue;

        int rb = pcb_table[i].remaining_burst;
        if (rb < 0) continue; 

        if (rb < best_burst) {
            best_burst = rb;
            best = i;
        } else if (rb == best_burst && best != -1) {
            // 동률이면 인덱스가 작은 쪽(결정 규칙)
            if (i < best) best = i;
        }
    }

    return best;
}

// 성능 출력
void print_performance(void) {
    printf("\n======================================\n");
    printf(" 성능 분석 (SRTF) (tq인자: %d, 시드: %u)\n", global_time_quantum, global_seed);
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
