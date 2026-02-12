#include "proc.h"

struct ptable_t ptable;
void
print_queues(void);
static struct proc *initproc;

int nextpid = 1;
extern void forkret(void);
extern void trapret(void);

static void wakeup1(void *chan);

const int time_quantum[] = {10, 20, 40, 80}; 

int sys_set_proc_info() {
  int q_level, cpu_burst, cpu_wait_time, io_wait_time, end_time;
  struct proc *p = myproc();

  if (argint(0, &q_level) < 0 ||
      argint(1, &cpu_burst) < 0 ||
      argint(2, &cpu_wait_time) < 0 ||
      argint(3, &io_wait_time) < 0 ||
      argint(4, &end_time) < 0)
      return -1;
    
  acquire(&ptable.lock);

  p->q_level = q_level;
  p->cpu_burst = cpu_burst;
  p->cpu_wait = cpu_wait_time;
  p->io_wait_time = io_wait_time;
  p->end_time = end_time;
  // #ifdef DEBUG
  // cprintf("DEBUG: set_proc_info called - PID: %d, end_time: %d\n", 
  //         p->pid, p->end_time);
  // #endif

  release(&ptable.lock);

  return 0;
}

//0. 프로세스 유틸 함수
//0-1. 프로세스의 타이머 업데이트
// 역할 : ptable.proc 배열을 순회하며, 각 프로세스의 상태에 따라 적절한 카운터를 1씩 증가시킨다.
// 호출 시점 : trap.c의 타이머 인터럽트 핸들러 내에서 yield() 호출 전에 호출
void update_all_proc_timers(void) {
  struct proc *p;

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
    if (p->pid == 0) {
      continue;
    }
    if (p->state == RUNNING) {
      p->cpu_burst++;
      p->total_cpu_time++;
    }
    else if (p->state == RUNNABLE) {
      p->cpu_wait++;
    }
    else if (p->state == SLEEPING) {
      p->io_wait_time++;
    }
  }
}


//1. 큐 관리 함수
void init_qeueus() {
  ptable.queues[0].head = ptable.queues[0].tail = 0;
  ptable.queues[1].head = ptable.queues[1].tail = 0;
  ptable.queues[2].head = ptable.queues[2].tail = 0;
  ptable.queues[3].head = ptable.queues[3].tail = 0;
}

//1-1. 프로세스 p를 특정 lv의 우선순위 큐에 추가하는 함수
/*
프로세스의 상태가 RUNNABLE이 될 때마다 호출된다.

- 새로운 프로세스 생성 : allocproc() 함수가 새로운 프로세스를 초기화 하고 EMBRYO -> RUNNABLE로 바꿀 때 추가
- I/O 대기 후 복귀 : wakeup 될 때
- 자발적 양보 : yield() 호출 시



*/
void add_to_queue(struct proc *p, int level) {
  if(level < 0 || level >= 4) panic("invalid queue level");

  struct proc_list *q = &ptable.queues[level];
  p->q_level = level;
  p->queue_entry_time = ticks;
  p->next = p->prev = 0;

  // 1. 큐가 비어있을 경우
  if (q->head == 0) {
    q->head = q->tail = p;
    return;
  }
  
  // 2. 우선순위 비교 후 삽입 (I/O > entry_time)
  for (struct proc *curr = q->head; curr != 0; curr = curr->next) {
    int p_is_higher = 0;
    if (p->io_wait_time > curr->io_wait_time) {
      p_is_higher = 1;
    } else if (p->io_wait_time == curr->io_wait_time && p->queue_entry_time > curr->queue_entry_time) {
      p_is_higher = 1;
    }

    if (p_is_higher) {
      // curr 앞에 삽입
      p->next = curr;
      p->prev = curr->prev;
      if (curr->prev) curr->prev->next = p;
      else q->head = p; // p가 새로운 head
      curr->prev = p;
      return;
    }
  }

  // 3. 우선순위가 가장 낮으므로 tail에 삽입
  q->tail->next = p;
  p->prev = q->tail;
  q->tail = p;
}

//1-2. 프로세스 p를 현재 큐에서 제거하는 함수
// 기존 remove_from_queue 함수를 이 코드로 교체하세요.
void remove_from_queue(struct proc *p)
{
  // 큐에 연결되어 있지 않다면 아무것도 하지 않음
  if (p->prev == 0 && p->next == 0 && ptable.queues[p->q_level].head != p) {
      return;
  }

  struct proc_list *q = &ptable.queues[p->q_level];

  // p의 이전 노드가 p의 다음 노드를 가리키도록 함
  if (p->prev) {
    p->prev->next = p->next;
  } else { // p가 head였을 경우, head를 p의 다음 노드로 변경
    q->head = p->next;
  }

  // p의 다음 노드가 p의 이전 노드를 가리키도록 함
  if (p->next) {
    p->next->prev = p->prev;
  } else { // p가 tail이었을 경우, tail을 p의 이전 노드로 변경
    q->tail = p->prev;
  }

  // p의 연결 정보를 완전히 초기화
  p->next = 0;
  p->prev = 0;
}

//2. MLFQ 로직 함수
//2-1. 스케줄러가 다음으로 실행할 프로세스를 찾는다.
// 사용 시점 : scheduler() 함수 내부에서
// 설명 : lv0 -> lv3까지 순서대로 큐를 탐색하여 가장 높은 우선순위 큐에 있는 첫 번째 프로세스를 반환한다. (모두 Runnable임을 보장)
struct proc *get_next_proc() {
  struct proc *tmp;

  for(int lv = 0; lv < 4; lv++) {
    tmp = ptable.queues[lv].head;
    if (tmp) {
      return tmp;
    }
  }
  return 0;
}

//2-2. 프로세스의 p의 우선순위를 낮춘다. (할당된 Time Quantum을 모두 소진시)
// 사용 시점 : trap() 함수에서 타이머 인터럽트 발생시, 할당된 time quantum을 모두 소진했을 때
// 설명 : p를 현재 큐에서 제거하고, p->q_level을 1 증가 시킨 후 새로운 큐에 추가한다.
void demote_process(struct proc *p) {
  if (p->type == INIT || p->type == SHELL)
    return ;

  if (p->q_level == 3) {
    p->cpu_burst = 0;
    return ;
  }

  p->q_level++;

  p->cpu_burst = 0;
  p->cpu_wait = 0;
  p->io_wait_time = 0;
}

//2-3. 프로세스의 p의 우선순위를 높인다 (2-4의 메인 로직 분리)
void promote_process(struct proc *p) {
  if (p->type == INIT || p->type == SHELL || p->q_level == 0) 
    return ;

  remove_from_queue(p);
  p->q_level--;
  p->cpu_wait = 0;
  p->cpu_burst = 0;
  add_to_queue(p, p->q_level);
}

//2-4. 낮은 우선순위 큐에 있는 프로세스들을 확인하여, 특정 시간 이상 대기했을 경우 우선순위를 높여준다.
// 사용 시점 : scheduler()가 호출되거나 타이머 인터럽트 시 주기적으로
// 설명 : lv1 ~ 3 까지 큐를 순회하며 각 프로세스의 대기 시간을 확인하고, 조건(250tick 대기)에 맞으면 promote_process()를 호출한다.
// RUNNABLE 상태(큐에 있는 상태)이기에, 큐에서 직접 제거 및 삽입하는 로직이 필요 -> Promote_process
void check_aging() {
  struct proc *tmp;
  struct proc *next_proc;

  for(int lv = 1; lv < 4; lv++) {
    tmp = ptable.queues[lv].head;
    while (tmp) {
      next_proc = tmp->next;
      if (tmp->cpu_wait >= 250) {
        #ifdef DEBUG
        if(tmp->pid >= 4) {
          cprintf("PID: %d Aging\n", tmp->pid);
        }
        #endif
        promote_process(tmp);
      }
      tmp = next_proc;
    }
  }
}

//2-5. RUNNING 상태의 프로세스 p가 할당된 CPU 시간을 추적한다. -> trap() 함수에서 타이머 인터럽트 발생 시
// 사용 시점 : trap() 함수에서 인터럽트 발생 시
// 설명 :  해당 lvdlm Time Quantum을 초과했는지 확읺여 demote_process를 호출할지 결정한다.
void check_time_quantum(struct proc *p) {
  if (p->cpu_burst >= time_quantum[p->q_level]) {
    #ifdef DEBUG
    if(p->pid >= 4) {
      cprintf("PID: %d uses %d ticks in mlfq[%d], total (%d/%d)\n", 
              p->pid, p->cpu_burst, p->q_level, p->total_cpu_time, p->end_time);
    }
    #endif
    demote_process(p);
  }
}


void
pinit(void)
{
  initlock(&ptable.lock, "ptable");
  init_qeueus();
}

// Must be called with interrupts disabled
int
cpuid() {
  return mycpu()-cpus;
}

// Must be called with interrupts disabled to avoid the caller being
// rescheduled between reading lapicid and running through the loop.
struct cpu*
mycpu(void)
{
  int apicid, i;
  
  if(readeflags()&FL_IF)
    panic("mycpu called with interrupts enabled\n");
  
  apicid = lapicid();
  // APIC IDs are not guaranteed to be contiguous. Maybe we should have
  // a reverse map, or reserve a register to store &cpus[i].
  for (i = 0; i < ncpu; ++i) {
    if (cpus[i].apicid == apicid)
      return &cpus[i];
  }
  panic("unknown apicid\n");
}

// Disable interrupts so that we are not rescheduled
// while reading proc from the cpu structure
struct proc*
myproc(void) {
  struct cpu *c;
  struct proc *p;
  pushcli();
  c = mycpu();
  p = c->proc;
  popcli();
  return p;
}

//PAGEBREAK: 32
// Look in the process table for an UNUSED proc.
// If found, change state to EMBRYO and initialize
// state required to run in the kernel.
// Otherwise return 0.
static struct proc*
allocproc(void)
{
  struct proc *p;
  char *sp;

  acquire(&ptable.lock);

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == UNUSED)
      goto found;

  release(&ptable.lock);
  return 0;

found:
  p->state = EMBRYO;
  p->pid = nextpid++;

  //cprintf("DEBUG_ALLOC: PID %d allocated\n", p->pid);
  p->q_level = 0;
  p->cpu_burst = 0;
  p->cpu_wait = 0;
  p->io_wait_time = 0;
  p->end_time = -1;       // end_time이 설정되지 않았음을 의미하는 -1로 초기화
  p->total_cpu_time = 0;
  p->queue_entry_time = 0;
  p->next = 0;
  p->prev = 0;

  release(&ptable.lock);

  // Allocate kernel stack.
  if((p->kstack = kalloc()) == 0){
    p->state = UNUSED;
    return 0;
  }
  sp = p->kstack + KSTACKSIZE;

  // Leave room for trap frame.
  sp -= sizeof *p->tf;
  p->tf = (struct trapframe*)sp;

  // Set up new context to start executing at forkret,
  // which returns to trapret.
  sp -= 4;
  *(uint*)sp = (uint)trapret;

  sp -= sizeof *p->context;
  p->context = (struct context*)sp;
  memset(p->context, 0, sizeof *p->context);
  p->context->eip = (uint)forkret;

  return p;
}

//PAGEBREAK: 32
// Set up first user process.
void
userinit(void)
{
  struct proc *p;
  extern char _binary_initcode_start[], _binary_initcode_size[];

  p = allocproc();
  
  initproc = p;
  if((p->pgdir = setupkvm()) == 0)
    panic("userinit: out of memory?");
  inituvm(p->pgdir, _binary_initcode_start, (int)_binary_initcode_size);
  p->sz = PGSIZE;
  memset(p->tf, 0, sizeof(*p->tf));
  p->tf->cs = (SEG_UCODE << 3) | DPL_USER;
  p->tf->ds = (SEG_UDATA << 3) | DPL_USER;
  p->tf->es = p->tf->ds;
  p->tf->ss = p->tf->ds;
  p->tf->eflags = FL_IF;
  p->tf->esp = PGSIZE;
  p->tf->eip = 0;  // beginning of initcode.S

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  // this assignment to p->state lets other cores
  // run this process. the acquire forces the above
  // writes to be visible, and the lock is also needed
  // because the assignment might not be atomic.
  acquire(&ptable.lock);

  //init 프로세스
  //shell은 init 프로세스가 exec 콜을 통해 실행 -> shell만을 위한 별도의 초기화 코드는 필요하지 않음.
  p->state = RUNNABLE;
  p->q_level = 3;
  p->cpu_burst = 0;
  p->cpu_wait = 0;
  p->io_wait_time = 0;
  p->next = p->prev = 0;
  p->queue_entry_time = 0;
  p->type = INIT;
  add_to_queue(p, p->q_level);

  release(&ptable.lock);
}

// Grow current process's memory by n bytes.
// Return 0 on success, -1 on failure.
int
growproc(int n)
{
  uint sz;
  struct proc *curproc = myproc();

  sz = curproc->sz;
  if(n > 0){
    if((sz = allocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  } else if(n < 0){
    if((sz = deallocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  }
  curproc->sz = sz;
  switchuvm(curproc);
  return 0;
}

// Create a new process copying p as the parent.
// Sets up stack to return as if from system call.
// Caller must set state of returned proc to RUNNABLE.
int
fork(void)
{
  int i, pid;
  struct proc *np;
  struct proc *curproc = myproc();

  // Allocate process.
  if((np = allocproc()) == 0){
    return -1;
  }

  // Copy process state from proc.
  if((np->pgdir = copyuvm(curproc->pgdir, curproc->sz)) == 0){
    kfree(np->kstack);
    np->kstack = 0;
    np->state = UNUSED;
    return -1;
  }
  np->sz = curproc->sz;
  np->parent = curproc;
  *np->tf = *curproc->tf;

  // Clear %eax so that fork returns 0 in the child.
  np->tf->eax = 0;

  for(i = 0; i < NOFILE; i++)
    if(curproc->ofile[i])
      np->ofile[i] = filedup(curproc->ofile[i]);
  np->cwd = idup(curproc->cwd);

  safestrcpy(np->name, curproc->name, sizeof(curproc->name));

  pid = np->pid;

  acquire(&ptable.lock);

  np->state = RUNNABLE;

  if (curproc->pid == 1) {
    //shell 예외처리
    np->q_level = 3;
    np->type = SHELL;
  }
  else {
    np->q_level = 0;
  }
  np->cpu_burst = 0;
  np->cpu_wait = 0;
  np->io_wait_time = 0;
  np->next = np->prev = 0;
  np->queue_entry_time = 0;
  np->end_time = -1;
  np->total_cpu_time = 0;
  add_to_queue(np, np->q_level);
  print_queues();
  //cprintf("DEBUG_FORK: Parent %d created Child %d\n", curproc->pid, np->pid);
  
  release(&ptable.lock);

  return pid;
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
void
exit(void)
{
  struct proc *curproc = myproc();
  struct proc *p;
  int fd;

  //cprintf("DEBUG_EXIT: PID %d is exiting\n", curproc->pid);
  if(curproc == initproc)
    panic("init exiting");

  // Close all open files.
  for(fd = 0; fd < NOFILE; fd++){
    if(curproc->ofile[fd]){
      fileclose(curproc->ofile[fd]);
      curproc->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(curproc->cwd);
  end_op();
  curproc->cwd = 0;

  acquire(&ptable.lock);

  remove_from_queue(curproc);

  // Parent might be sleeping in wait().
  wakeup1(curproc->parent);

  // Pass abandoned children to init.
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->parent == curproc){
      p->parent = initproc;
      if(p->state == ZOMBIE)
        wakeup1(initproc);
    }
  }

  // Jump into the scheduler, never to return.
  curproc->state = ZOMBIE;
  sched();
  panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int
wait(void)
{
  struct proc *p;
  int havekids, pid;
  struct proc *curproc = myproc();
  
  acquire(&ptable.lock);
  for(;;){
    // Scan through table looking for exited children.
    havekids = 0;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->parent != curproc)
        continue;
      havekids = 1;
      if(p->state == ZOMBIE){
        // Found one.
        pid = p->pid;
        kfree(p->kstack);
        p->kstack = 0;
        freevm(p->pgdir);
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        p->state = UNUSED;
        release(&ptable.lock);
        return pid;
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || curproc->killed){
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(curproc, &ptable.lock);  //DOC: wait-sleep
  }
}

// proc.c 파일에 추가

void
print_queues(void)
{
  // struct proc *p;
  
  // cprintf("===== QUEUE STATE =====\n");
  // for (int i = 0; i < 4; i++) {
  //   cprintf("Queue[%d]: ", i);
  //   p = ptable.queues[i].head;
  //   if (!p) {
  //     cprintf("EMPTY\n");
  //     continue;
  //   }
  //   while(p) {
  //     cprintf("[PID %d] -> ", p->pid);
  //     p = p->next;
  //   }
  //   cprintf("NULL\n");
  // }
  // cprintf("=======================\n");
}

//PAGEBREAK: 42
// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run
//  - swtch to start running that process
//  - eventually that process transfers control
//      via swtch back to the scheduler.
void
scheduler(void)
{
  struct proc *p;
  struct cpu *c = mycpu();
  c->proc = 0;
  
  for(;;){
    // Enable interrupts on this processor.
    sti();

    // Loop over process table looking for process to run.
    acquire(&ptable.lock);

    //print_queues();
    //idle 프로세스는 CPU가 할일이 없을 때 실행하는데, 할일이 없으면 lv3의, init, shell이 실행되니 알아서 처리됨
    p = get_next_proc();
    // if (p->pid >= 3) {
    //   print_queues();
    // }
    if (p && p->state == RUNNABLE && p->kstack != 0) {
      // Switch to chosen process.  It is the process's job
      // to release ptable.lock and then reacquire it
      // before jumping back to us.
      //cprintf("DEBUG_SCHED: Picking PID %d (Q%d)\n", p->pid, p->q_level);
      c->proc = p;
      switchuvm(p);
      p->state = RUNNING;
      remove_from_queue(p);

      swtch(&(c->scheduler), p->context);
      switchkvm();

      // Process is done running for now.
      // It should have changed its p->state before coming back.
      c->proc = 0;
    }

    release(&ptable.lock);
  }
}

// Enter scheduler.  Must hold only ptable.lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->ncli, but that would
// break in the few places where a lock is held but
// there's no process.
void
sched(void)
{
  int intena;
  struct proc *p = myproc();

  if(!holding(&ptable.lock))
    panic("sched ptable.lock");
  if(mycpu()->ncli != 1)
    panic("sched locks");
  if(p->state == RUNNING)
    panic("sched running");
  if(readeflags()&FL_IF)
    panic("sched interruptible");
  intena = mycpu()->intena;
  swtch(&p->context, mycpu()->scheduler);
  mycpu()->intena = intena;
}

// Give up the CPU for one scheduling round.
void
yield(void)
{
  acquire(&ptable.lock);  //DOC: yieldlock
  myproc()->state = RUNNABLE;
  add_to_queue(myproc(), myproc()->q_level);
      
  // #ifdef DEBUG
  // if(myproc()->pid >= 4) {
  //     cprintf("YIELD: PID %d back to queue %d\n", 
  //             myproc()->pid, myproc()->q_level);
  // }
  // #endif
  
  sched();
  release(&ptable.lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch here.  "Return" to user space.
void
forkret(void)
{
  static int first = 1;
  // Still holding ptable.lock from scheduler.
  release(&ptable.lock);

  if (first) {
    // Some initialization functions must be run in the context
    // of a regular process (e.g., they call sleep), and thus cannot
    // be run from main().
    first = 0;
    iinit(ROOTDEV);
    initlog(ROOTDEV);
  }

  // Return to "caller", actually trapret (see allocproc).
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void
sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();
  
  if(p == 0)
    panic("sleep");

  if(lk == 0)
    panic("sleep without lk");

  // Must acquire ptable.lock in order to
  // change p->state and then call sched.
  // Once we hold ptable.lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup runs with ptable.lock locked),
  // so it's okay to release lk.
  if(lk != &ptable.lock){  //DOC: sleeplock0
    acquire(&ptable.lock);  //DOC: sleeplock1
    release(lk);
  }
  // Go to sleep.
  p->chan = chan;
  p->state = SLEEPING;
  //RUNNING -> SLEEPING이기 때문에 큐에서 빼는 코드 필요 없음

  sched();

  // Tidy up.
  p->chan = 0;

  // Reacquire original lock.
  if(lk != &ptable.lock){  //DOC: sleeplock2
    release(&ptable.lock);
    acquire(lk);
  }
}

//PAGEBREAK!
// Wake up all processes sleeping on chan.
// The ptable lock must be held.
static void
wakeup1(void *chan)
{
  struct proc *p;

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
    if(p->state == SLEEPING && p->chan == chan) {
      p->state = RUNNABLE;
      add_to_queue(p, p->q_level);
    }
  }
}

// Wake up all processes sleeping on chan.
void
wakeup(void *chan)
{
  acquire(&ptable.lock);
  wakeup1(chan);
  release(&ptable.lock);
}

// Kill the process with the given pid.
// Process won't exit until it returns
// to user space (see trap in trap.c).
int
kill(int pid)
{
  struct proc *p;

  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->pid == pid){
      p->killed = 1;
      // Wake process from sleep if necessary.
      if(p->state == SLEEPING) {
        p->state = RUNNABLE;
        add_to_queue(p, p->q_level);
      }
      release(&ptable.lock);
      return 0;
    }
  }
  release(&ptable.lock);
  return -1;
}

//PAGEBREAK: 36
// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void
procdump(void)
{
  static char *states[] = {
  [UNUSED]    "unused",
  [EMBRYO]    "embryo",
  [SLEEPING]  "sleep ",
  [RUNNABLE]  "runble",
  [RUNNING]   "run   ",
  [ZOMBIE]    "zombie"
  };
  int i;
  struct proc *p;
  char *state;
  uint pc[10];

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    cprintf("%d %s %s", p->pid, state, p->name);
    if(p->state == SLEEPING){
      getcallerpcs((uint*)p->context->ebp+2, pc);
      for(i=0; i<10 && pc[i] != 0; i++)
        cprintf(" %p", pc[i]);
    }
    cprintf("\n");
  }
}
