#include "types.h"
#include "stat.h"
#include "user.h"

#define NUM_CHILD 3

int main(int argc, char *argv[])
{
    int pid;
    // int child_pids[NUM_CHILD];
    
    printf(1, "start scheduler_test\n");
    
    // 자식 프로세스 생성
    for(int i = 0; i < NUM_CHILD; i++) {
        pid = fork();
        if (pid < 0) {
            printf(1, "error: fork fail\n");
            exit();
        }
        else if (pid == 0) {
            // 자식 프로세스
            int my_pid = getpid();
            
            printf(1, "PID: %d created\n", my_pid);
            
			// 약간의 지연을 추가하여 출력 충돌 방지
			for(int j = 0; j < 1000000 * (i + 1); j++);

            // set_proc_info 호출
            if (set_proc_info(2, 0, 0, 0, 300) < 0) {
                printf(1, "error: set_proc_info failed for %d\n", my_pid);
                exit();
            }
            
            printf(1, "Set process %d's info complete\n", my_pid);
            
            // CPU를 계속 사용
            while(1) {
                // Time quantum 테스트를 위한 busy wait
            }
            exit();
        }
        else {
            // 부모는 자식 PID 저장
            // child_pids[i] = pid;
        }
    }
    
    // 부모 프로세스는 모든 자식이 종료될 때까지 대기
    for (int i = 0; i < NUM_CHILD; i++) {
        wait();
    }
    
    printf(1, "end of scheduler_test\n");
    exit();
}