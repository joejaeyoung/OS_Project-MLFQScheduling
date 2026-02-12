#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"
#include "traps.h"
#include "spinlock.h"

// Interrupt descriptor table (shared by all CPUs).
struct gatedesc idt[256];
extern uint vectors[];  // in vectors.S: array of 256 entry pointers
struct spinlock tickslock;
uint ticks;

void
tvinit(void)
{
  int i;

  for(i = 0; i < 256; i++)
    SETGATE(idt[i], 0, SEG_KCODE<<3, vectors[i], 0);
  SETGATE(idt[T_SYSCALL], 1, SEG_KCODE<<3, vectors[T_SYSCALL], DPL_USER);

  initlock(&tickslock, "time");
}

void
idtinit(void)
{
  lidt(idt, sizeof(idt));
}

//PAGEBREAK: 41
void
trap(struct trapframe *tf)
{
  if(tf->trapno == T_SYSCALL){
    if(myproc()->killed)
      exit();
    myproc()->tf = tf;
    syscall();
    if(myproc()->killed)
      exit();
    return;
  }

  switch(tf->trapno){
  case T_IRQ0 + IRQ_TIMER:
    if(cpuid() == 0){
      acquire(&tickslock);
      ticks++;
      wakeup(&ticks);
      release(&tickslock);
    }
    /* scheduler 구현 로직 */
    if (myproc() && myproc()->state == RUNNING) {
      acquire(&ptable.lock);

      //1. 모든 프로세스의 타이머를 1씩 증가시킴
      update_all_proc_timers();

      // 디버깅: 현재 프로세스 상태 출력
      // #ifdef DEBUG
      // if(myproc()->pid >= 4) {  // 테스트 프로세스만
      //     if(myproc()->cpu_burst % 10 == 0) {  // 10 tick마다 출력
      //         cprintf("TIMER: PID %d, burst %d/%d, level %d\n", 
      //                 myproc()->pid, myproc()->cpu_burst, 
      //                 -1, myproc()->q_level);
      //     }
      // }
      // #endif




      //CPU 총 사용시간 갱신 및 end time 확인
      if(myproc()->end_time > 0 && myproc()->total_cpu_time >= myproc()->end_time){
        #ifdef DEBUG
        cprintf("PID: %d uses %d ticks in mlfq[%d], total (%d/%d)\n", 
          myproc()->pid, myproc()->cpu_burst, myproc()->q_level, myproc()->total_cpu_time, myproc()->end_time);
        cprintf("PID: %d, used %d ticks. terminated\n", myproc()->pid, myproc()->total_cpu_time);
        #endif
        myproc()->killed = 1; // 프로세스 종료 플래그 설정
      }
      //2. RUNNING 상태의 프로세스의 Time Quantum 소진 여부 확인
      else if (myproc()->killed == 0) {
        check_time_quantum(myproc());
      }
      
      //3. 기아 방지
      check_aging();

      release(&ptable.lock);
    }

    lapiceoi();
    break;
  case T_IRQ0 + IRQ_IDE:
    ideintr();
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_IDE+1:
    // Bochs generates spurious IDE1 interrupts.
    break;
  case T_IRQ0 + IRQ_KBD:
    kbdintr();
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_COM1:
    uartintr();
    lapiceoi();
    break;
  case T_IRQ0 + 7:
  case T_IRQ0 + IRQ_SPURIOUS:
    cprintf("cpu%d: spurious interrupt at %x:%x\n",
            cpuid(), tf->cs, tf->eip);
    lapiceoi();
    break;

  //PAGEBREAK: 13
  default:
    if(myproc() == 0 || (tf->cs&3) == 0){
      // In kernel, it must be our mistake.
      cprintf("unexpected trap %d from cpu %d eip %x (cr2=0x%x)\n",
              tf->trapno, cpuid(), tf->eip, rcr2());
      panic("trap");
    }
    // In user space, assume process misbehaved.
    cprintf("pid %d %s: trap %d err %d on cpu %d "
            "eip 0x%x addr 0x%x--kill proc\n",
            myproc()->pid, myproc()->name, tf->trapno,
            tf->err, cpuid(), tf->eip, rcr2());
    myproc()->killed = 1;
  }

  // Force process exit if it has been killed and is in user space.
  // (If it is still executing in the kernel, let it keep running
  // until it gets to the regular system call return.)
  if(myproc() && myproc()->killed && (tf->cs&3) == DPL_USER)
    exit();

  // Force process to give up CPU on clock tick.
  // If interrupts were on while locks held, would need to check nlock.
  if(myproc() && myproc()->state == RUNNING &&
     tf->trapno == T_IRQ0+IRQ_TIMER)
    yield();

  // Check if the process has been killed since we yielded
  if(myproc() && myproc()->killed && (tf->cs&3) == DPL_USER)
    exit();
}
