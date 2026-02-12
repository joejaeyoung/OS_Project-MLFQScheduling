#ifndef _PROC_H_
#define _PROC_H_

#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "spinlock.h"
// Per-CPU state
struct cpu {
  uchar apicid;                // Local APIC ID
  struct context *scheduler;   // swtch() here to enter scheduler
  struct taskstate ts;         // Used by x86 to find stack for interrupt
  struct segdesc gdt[NSEGS];   // x86 global descriptor table
  volatile uint started;       // Has the CPU started?
  int ncli;                    // Depth of pushcli nesting.
  int intena;                  // Were interrupts enabled before pushcli?
  struct proc *proc;           // The process running on this cpu or null
};

extern struct cpu cpus[NCPU];
extern int ncpu;

//PAGEBREAK: 17
// Saved registers for kernel context switches.
// Don't need to save all the segment registers (%cs, etc),
// because they are constant across kernel contexts.
// Don't need to save %eax, %ecx, %edx, because the
// x86 convention is that the caller has saved them.
// Contexts are stored at the bottom of the stack they
// describe; the stack pointer is the address of the context.
// The layout of the context matches the layout of the stack in swtch.S
// at the "Switch stacks" comment. Switch doesn't save eip explicitly,
// but it is on the stack and allocproc() manipulates it.
struct context {
  uint edi;
  uint esi;
  uint ebx;
  uint ebp;
  uint eip;
};

enum procstate { UNUSED, EMBRYO, SLEEPING, RUNNABLE, RUNNING, ZOMBIE };
enum proctype { IDLE, INIT, SHELL };

// Per-process state
struct proc {
  uint sz;                     // Size of process memory (bytes)
  pde_t* pgdir;                // Page table
  char *kstack;                // Bottom of kernel stack for this process
  enum procstate state;        // Process state
  int pid;                     // Process ID
  struct proc *parent;         // Parent process
  struct trapframe *tf;        // Trap frame for current syscall
  struct context *context;     // swtch() here to run process
  void *chan;                  // If non-zero, sleeping on chan
  int killed;                  // If non-zero, have been killed
  struct file *ofile[NOFILE];  // Open files
  struct inode *cwd;           // Current directory
  char name[16];               // Process name (debugging)

  int q_level;                 // 속해있는 큐 레벨
  int cpu_burst;              // 프로세스당 Time Quantum 내에서 CPU 사용 시간
  int cpu_wait;               // 프로세스 당 Runnable 상태된 후 해당 큐에서의 대기 시간
  int io_wait_time;           // 해당 큐에서 sleeping 상태 시간
  int end_time;               // 응용 프로그램의 CPU 총 사용 할당량
  int total_cpu_time;          // CPU 총 사용량
  uint queue_entry_time;       // queue 진입 시간
  struct proc *next;           // 링크드 리스트를 위한 연결고리
  struct proc *prev;
  enum proctype type;
};

struct proc_list {
  int proc_count;
  struct proc *head;
  struct proc *tail;
};

struct ptable_t{
  struct spinlock lock;
  struct proc proc[NPROC];
  struct proc_list queues[4];
};

extern struct ptable_t ptable;

// Process memory is laid out contiguously, low addresses first:
//   text
//   original data and bss
//   fixed-size stack
//   expandable heap

// ssu_scheduler 함수
void update_all_proc_timers(void);
void init_qeueus();
void add_to_queue(struct proc *p, int level);
void remove_from_queue(struct proc *p);
struct proc *get_next_proc();
void demote_process(struct proc *p);
void promote_process(struct proc *p);
void check_aging(void);
void check_time_quantum(struct proc *p);


#endif