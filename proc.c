#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"
#ifdef CS333_P2
#include "uproc.h"
#endif //CS333_P2

#define PER_LINE  15
#define PER_LINE_Z (PER_LINE/2)

static char *states[] = {
  [UNUSED]    "unused",
  [EMBRYO]    "embryo",
  [SLEEPING]  "sleep ",
  [RUNNABLE]  "runble",
  [RUNNING]   "run   ",
  [ZOMBIE]    "zombie"
};


#ifdef CS333_P3
#define statecount NELEM(states)
#endif	//CS333_P3

#ifdef CS333_P3
// record with head and tail pointer for constant-time access to the beginning
// and end of a linked list of struct procs.  use with stateListAdd() and
// stateListRemove().
struct ptrs {
  struct proc* head;
  struct proc* tail;
};
#endif

static struct {
  struct spinlock lock;
  struct proc proc[NPROC];
#ifdef CS333_P3
  struct ptrs list[statecount];
#endif
#ifdef CS333_P4
  struct ptrs ready[MAXPRIO+1]; //for holding a list of ready processes in my case 5+1
  uint PromoteAtTime;          //for promoting at a certain tick value
#endif  //CS333_P4
} ptable;

// list management function prototypes
#ifdef CS333_P3
static void initProcessLists(void);
static void initFreeList(void);
static void stateListAdd(struct ptrs*, struct proc*);
static int  stateListRemove(struct ptrs*, struct proc* p);
static void assertState(struct proc*, enum procstate, const char *, int);
#endif // CS333_P3
#ifdef CS333_P4
static void printReadyLists(  );  //what is this for P4?
static void printReadyList(struct proc *, int); //also for P4
#endif // CS333_P4

static struct proc *initproc;

uint nextpid = 1;
extern void forkret(void);
extern void trapret(void);
static void wakeup1(void* chan);

  void
pinit(void)
{
  initlock(&ptable.lock, "ptable");
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
    if (cpus[i].apicid == apicid) {
      return &cpus[i];
    }
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
#ifdef CS333_P3
static struct proc*
allocproc(void)
{
  struct proc *p;
  char *sp;

  acquire(&ptable.lock);
  int found = 0;
  for(p = ptable.list[UNUSED].head; p != ptable.list[UNUSED].tail; p = p->next){
    if(p->state == UNUSED) {
      found = 1;
      break;
    }
  }
  if (!found) {
    release(&ptable.lock);
    return 0;
  }
#ifdef CS333_P3
  if(stateListRemove(&ptable.list[UNUSED], p) == -1)
    panic("\nFailed to remove from UNUSED list after kernel stack allocation failure in allocproc()\n");
  assertState(p, UNUSED, __FUNCTION__, __LINE__);
#endif
  p->state = EMBRYO;
#ifdef CS333_P4
  p->priority = MAXPRIO;
  p->budget = DEFAULT_BUDGET;
#endif
#ifdef CS333_P3
  stateListAdd(&ptable.list[EMBRYO], p);
#endif
  p->pid = nextpid++;
  release(&ptable.lock);

  // Allocate kernel stack.
  if((p->kstack = kalloc()) == 0){
#ifdef CS333_P3
    if(stateListRemove(&ptable.list[EMBRYO], p) == -1)
      panic("\nFailed to remove from EMBRYO list after kernel stack allocation failure in allocproc()\n");
    assertState(p, EMBRYO, __FUNCTION__, __LINE__);
#endif
    p->state = UNUSED;
#ifdef CS333_P3
    stateListAdd(&ptable.list[UNUSED], p);
    release(&ptable.lock);
#endif
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

  //To initialize the start_ticks
  p->start_ticks = ticks;
  p->cpu_ticks_total = 0;
  p->cpu_ticks_in = 0;

  return p;
}

#else
static struct proc*
allocproc(void)
{
  struct proc *p;
  char *sp;

  acquire(&ptable.lock);
  int found = 0;
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == UNUSED) {
      found = 1;
      break;
    }
  if (!found) {
    release(&ptable.lock);
    return 0;
  }
  p->state = EMBRYO;
  p->pid = nextpid++;
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

  //To initialize the start_ticks
  p->start_ticks = ticks;
  p->cpu_ticks_total = 0;
  p->cpu_ticks_in = 0;

  return p;
}
#endif

//PAGEBREAK: 32
// Set up first user process.
void
userinit(void)
{
  struct proc *p;
  extern char _binary_initcode_start[], _binary_initcode_size[];

#ifdef CS333_P3
  acquire(&ptable.lock);
  initProcessLists();
  initFreeList();
  release(&ptable.lock);
#endif

  p = allocproc();
  initproc = p;
  p->uid = UID;
  p->gid = GID;
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
#ifdef CS333_P3
  if(stateListRemove(&ptable.list[EMBRYO], p) == -1)
    panic("\nFailed to remove from EMBRYO list after sccessful allocation in userinit()\n");
  assertState(p, EMBRYO, __FUNCTION__, __LINE__);
#endif
  p->state = RUNNABLE;
#ifdef CS333_P4
  stateListAdd(&ptable.ready[p->priority], p);
  ptable.PromoteAtTime = ticks + TICKS_TO_PROMOTE;
#elif CS333_P3
  stateListAdd(&ptable.list[RUNNABLE], p);
#endif
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
  int i;
  uint pid;
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
#ifdef CS333_P3
    acquire(&ptable.lock);
    if(stateListRemove(&ptable.list[EMBRYO], np) == -1)
      panic("\nFailed to remove from EMBYO list in fkrk() after page directory allocation failure\n");
    assertState(np, EMBRYO, __FUNCTION__, __LINE__);
#endif
    np->state = UNUSED;
#ifdef CS333_P3
    stateListAdd(&ptable.list[UNUSED], np);
#endif
    release(&ptable.lock);
    return -1;
  }
  np->sz = curproc->sz;
  np->parent = curproc;
  *np->tf = *curproc->tf;
#ifdef CS333_P2
  np->uid = curproc->uid;
  np->gid = curproc->gid;
#endif	//CS333_P2

  // Clear %eax so that fork returns 0 in the child.
  np->tf->eax = 0;

  for(i = 0; i < NOFILE; i++){
    if(curproc->ofile[i])
      np->ofile[i] = filedup(curproc->ofile[i]);
  }
  np->cwd = idup(curproc->cwd);

  safestrcpy(np->name, curproc->name, sizeof(curproc->name));

  pid = np->pid;

  acquire(&ptable.lock);
#ifdef CS333_P3
  if(stateListRemove(&ptable.list[EMBRYO], np) == -1)
    panic("\nFailed to remove from EMBYO on succesful fork\n");
  assertState(np, EMBRYO, __FUNCTION__, __LINE__);
#endif
  np->state = RUNNABLE;
#ifdef CS333_P4
  stateListAdd(&ptable.ready[np->priority], np);
#elif CS333_P3
  stateListAdd(&ptable.list[RUNNABLE], np);
#endif
  release(&ptable.lock);

  return pid;
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
#ifdef CS333_P4
void
exit(void)
{
  struct proc *curproc = myproc();
  struct proc *p;
  int fd;

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

  // Parent might be sleeping in wait().
  wakeup1(curproc->parent);

  // Pass abandoned children to init.
  for(int i = 0; i < NELEM(states); i++){
    p = ptable.list[i].head;
    while(p){
      if(p->parent == curproc){
        p->parent = initproc;
        if(p->state == ZOMBIE)
          wakeup1(initproc);
      }
      p = p->next;
    }
  }
  for(int i = MAXPRIO; i >=PRIO_MIN; --i)
  {
    p = ptable.ready[i].head;
    while(p){
      if(p->parent == curproc){
        p->parent = initproc;
        if(p->state == ZOMBIE)
          wakeup1(initproc);
      }
      p = p->next;
    }
  }

  // Jump into the scheduler, never to return.
  stateListRemove(&ptable.list[RUNNING], curproc);
  assertState(curproc, RUNNING, __FUNCTION__, __LINE__);
  curproc->state = ZOMBIE;
  stateListAdd(&ptable.list[ZOMBIE], curproc);
#ifdef PDX_XV6
  curproc->sz = 0;
#endif // PDX_XV6
  sched();
  panic("zombie exit");
}

#elif CS333_P3
void
exit(void)
{
  struct proc *curproc = myproc();
  struct proc *p;
  int fd;

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

  // Parent might be sleeping in wait().
  wakeup1(curproc->parent);

  // Pass abandoned children to init.
  for(int i = 0; i < NELEM(states); i++){
    p = ptable.list[i].head;
    while(p){
      if(p->parent == curproc){
        p->parent = initproc;
        if(p->state == ZOMBIE)
          wakeup1(initproc);
      }
      p = p->next;
    }
  }

  // Jump into the scheduler, never to return.
  stateListRemove(&ptable.list[RUNNING], curproc);
  assertState(curproc, RUNNING, __FUNCTION__, __LINE__);
  curproc->state = ZOMBIE;
  stateListAdd(&ptable.list[ZOMBIE], curproc);
#ifdef PDX_XV6
  curproc->sz = 0;
#endif // PDX_XV6
  sched();
  panic("zombie exit");
}
#else	//CS333_P1,P2
void
exit(void)
{
  struct proc *curproc = myproc();
  struct proc *p;
  int fd;

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
#ifdef PDX_XV6
  curproc->sz = 0;
#endif // PDX_XV6
  sched();
  panic("zombie exit");
}
#endif	//CS333_P3

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
#ifdef CS333_P3
int
wait(void){
  struct proc *p;
  int havekids;
  uint pid;
  struct proc *curproc = myproc();

  acquire(&ptable.lock);
  for(;;){
    // Scan through table looking for exited children.
    havekids = 0;
    for(int i = UNUSED+1; i < NELEM(states); i++){
      p = ptable.list[i].head;
      while(p){
        if(p->parent != curproc){
          p = p->next;
          continue;
        }
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
          if(stateListRemove(&ptable.list[ZOMBIE], p) == -1)
            panic("\nFailed to remove from ZOMBIE list in wait()\n");
          assertState(p, ZOMBIE, __FUNCTION__, __LINE__);
          p->state = UNUSED;
          stateListAdd(&ptable.list[UNUSED], p);
          release(&ptable.lock);
          return pid;
        }
        p = p->next;
      }
    }

#ifdef CS333_P4
    for(int i = MAXPRIO; i >= PRIO_MIN; --i)
    {
      p = ptable.ready[i].head;
      while(p){
        if(p->parent != curproc){
          p = p->next;
          continue;
        }
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
          if(stateListRemove(&ptable.list[ZOMBIE], p) == -1)
            panic("\nFailed to remove from ZOMBIE list in wait()\n");
          assertState(p, ZOMBIE, __FUNCTION__, __LINE__);
          p->state = UNUSED;
          stateListAdd(&ptable.list[UNUSED], p);
          release(&ptable.lock);
          return pid;
        }
        p = p->next;
      } 
    }
#endif //CS333_P4

    // No point waiting if we don't have any children.
    if(!havekids || curproc->killed){
      release(&ptable.lock);
      return -1;
    }
    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(curproc, &ptable.lock);  //DOC: wait-sleep
  }
}

#else	//CS333_P1,P2
int
wait(void)
{
  struct proc *p;
  int havekids;
  uint pid;
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
#endif

//PAGEBREAK: 42
// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run
//  - swtch to start running that process
//  - eventually that process transfers control
//      via swtch back to the scheduler.
#ifdef CS333_P4
void
scheduler(void)
{
  struct proc *p = NULL;
  struct cpu *c = mycpu();
  int i;
  c->proc = 0;
#ifdef PDX_XV6
  int idle;  // for checking if processor is idle
#endif // PDX_XV6

  for(;;){
    // Enable interrupts on this processor.
    sti();
#ifdef PDX_XV6
    idle = 1;  // assume idle unless we schedule a process
#endif // PDX_XV6

    
    if(ticks >= ptable.PromoteAtTime)
    {
      ptable.PromoteAtTime = ticks + TICKS_TO_PROMOTE;  //to push the ticks ahead
      promoteProcs();
    }

    // Loop over process table looking for process to run.
    acquire(&ptable.lock);
    for(i = MAXPRIO; i >= PRIO_MIN; --i)
    {
      p = ptable.ready[i].head;
      if(p)
      {
        p = ptable.ready[i].head;   //points to head,
        break;
      }
    }

    if(p){
      //error here!
      if(stateListRemove(&ptable.ready[p->priority], p) == -1)
        panic("\nFailed to remove process we will run from RUNNABLE in scheduler()\n");
      assertState(p, RUNNABLE, __FUNCTION__, __LINE__);

      //and add to the RUNNING list
#ifdef PDX_XV6
      idle = 0;  // not idle this timeslice
#endif // PDX_XV6
      c->proc = p;
      switchuvm(p);
      p->state = RUNNING;

      stateListAdd(&ptable.list[RUNNING], p); //void return type, ALWAYS SUCCEEDS

      p->cpu_ticks_in = ticks;
      swtch(&(c->scheduler), p->context);
      switchkvm();

      // Process is done running for now.
      // It should have changed its p->state before coming back.
      c->proc = 0;
    }
    release(&ptable.lock);
#ifdef PDX_XV6
    // if idle, wait for next interrupt
    if (idle) {
      sti();
      hlt();
    }
#endif // PDX_XV6
  }
}

#elif CS333_P3
void
scheduler(void)
{
  struct proc *p;
  struct cpu *c = mycpu();
  c->proc = 0;
#ifdef PDX_XV6
  int idle;  // for checking if processor is idle
#endif // PDX_XV6

  for(;;){
    // Enable interrupts on this processor.
    sti();

#ifdef PDX_XV6
    idle = 1;  // assume idle unless we schedule a process
#endif // PDX_XV6

    // Loop over process table looking for process to run.
    acquire(&ptable.lock);

#ifdef CS333_P4
    if(ticks >= ptable.PromoteAtTime)
    {
      ptable.PromoteAtTime = ticks + TICKS_TO_PROMOTE;
      promoteProcs();
    }
#elif CS333_P3
    p = ptable.list[RUNNABLE].head;   //points to head,
#endif
    //or first proc in the list
    //as it follow FIFO
    //check for a valid process
    if(p){
      //remove from the RUNNABLE list
      if(stateListRemove(&ptable.list[RUNNABLE], p) == -1)
        panic("\nFailed to remove process we will run from RUNNABLE in scheduler()\n");
      assertState(p, RUNNABLE, __FUNCTION__, __LINE__);

      //and add to the RUNNING list
      p->state = RUNNING;
      stateListAdd(&ptable.list[RUNNING], p); //void return type, ALWAYS SUCCEEDS
#ifdef PDX_XV6
      idle = 0;  // not idle this timeslice
#endif // PDX_XV6
      c->proc = p;
      switchuvm(p);

      p->cpu_ticks_in = ticks;
      swtch(&(c->scheduler), p->context);
      switchkvm();

      // Process is done running for now.
      // It should have changed its p->state before coming back.
      c->proc = 0;
    }
    release(&ptable.lock);
#ifdef PDX_XV6
    // if idle, wait for next interrupt
    if (idle) {
      sti();
      hlt();
    }
#endif // PDX_XV6
  }
}
#else	//CS333_P1,P2
  void
scheduler(void)
{
  struct proc *p;
  struct cpu *c = mycpu();
  c->proc = 0;
#ifdef PDX_XV6
  int idle;  // for checking if processor is idle
#endif // PDX_XV6

  for(;;){
    // Enable interrupts on this processor.
    sti();

#ifdef PDX_XV6
    idle = 1;  // assume idle unless we schedule a process
#endif // PDX_XV6
    // Loop over process table looking for process to run.
    acquire(&ptable.lock);
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->state != RUNNABLE)
        continue;

      // Switch to chosen process.  It is the process's job
      // to release ptable.lock and then reacquire it
      // before jumping back to us.
#ifdef PDX_XV6
      idle = 0;  // not idle this timeslice
#endif // PDX_XV6
      c->proc = p;
      switchuvm(p);
      p->state = RUNNING;
      p->cpu_ticks_in = ticks;
      swtch(&(c->scheduler), p->context);
      switchkvm();

      // Process is done running for now.
      // It should have changed its p->state before coming back.
      c->proc = 0;
    }
    release(&ptable.lock);
#ifdef PDX_XV6
    // if idle, wait for next interrupt
    if (idle) {
      sti();
      hlt();
    }
#endif // PDX_XV6
  }
}
#endif


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
  p->cpu_ticks_total += (ticks-p->cpu_ticks_in);
  swtch(&p->context, mycpu()->scheduler);
  mycpu()->intena = intena;
}

// Give up the CPU for one scheduling round.
#ifdef CS333_P4
void
yield(void)
{
  struct proc *curproc = myproc();

  acquire(&ptable.lock);  //DOC: yieldlock
  stateListRemove(&ptable.list[RUNNING], curproc);
  assertState(curproc, RUNNING, __FUNCTION__, __LINE__);
  curproc->state = RUNNABLE;

  curproc->budget -= (ticks - curproc->cpu_ticks_in);
  if(curproc->budget <= 0)
  {
    if(curproc->priority > 0)
      --(curproc->priority);
    stateListAdd(&ptable.ready[curproc->priority], curproc);
    curproc->budget = DEFAULT_BUDGET;
  }
  else
  {
    stateListAdd(&ptable.ready[curproc->priority], curproc);
  }

  sched();
  release(&ptable.lock);
}
#elif CS333_P3
void
yield(void)
{
  struct proc *curproc = myproc();

  acquire(&ptable.lock);  //DOC: yieldlock
  stateListRemove(&ptable.list[RUNNING], curproc);
  assertState(curproc, RUNNING, __FUNCTION__, __LINE__);
  curproc->state = RUNNABLE;
  stateListAdd(&ptable.list[RUNNABLE], curproc);

  sched();
  release(&ptable.lock);
}
#else	//CS333_P1,P2
  void
yield(void)
{
  struct proc *curproc = myproc();

  acquire(&ptable.lock);  //DOC: yieldlock
  curproc->state = RUNNABLE;
  sched();
  release(&ptable.lock);
}
#endif

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
#ifdef CS333_P3
void
sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();

  if(p == 0)
    panic("sleep");

  // Must acquire ptable.lock in order to
  // change p->state and then call sched.
  // Once we hold ptable.lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup runs with ptable.lock locked),
  // so it's okay to release lk.
  if(lk != &ptable.lock){  //DOC: sleeplock0
    acquire(&ptable.lock);  //DOC: sleeplock1
    if (lk) release(lk);
  }
  // Go to sleep.
  p->chan = chan;
#ifdef CS333_P3
  stateListRemove(&ptable.list[RUNNING], p);
  assertState(p, RUNNING, __FUNCTION__, __LINE__);
#endif
#ifdef CS333_P4
  p->budget -= (ticks - p->cpu_ticks_in);
  if(p->budget <= 0)
  {
    if(p->priority > 0)
      --(p->priority);
    p->budget = DEFAULT_BUDGET;
  }
#endif
  p->state = SLEEPING;
#ifdef CS333_P3
  stateListAdd(&ptable.list[SLEEPING], p);
#endif

  sched();

  // Tidy up.
  p->chan = 0;

  // Reacquire original lock.
  if(lk != &ptable.lock){  //DOC: sleeplock2
    release(&ptable.lock);
    if (lk) acquire(lk);
  }
}
#else	//CS333_P1,P2
  void
sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();

  if(p == 0)
    panic("sleep");

  // Must acquire ptable.lock in order to
  // change p->state and then call sched.
  // Once we hold ptable.lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup runs with ptable.lock locked),
  // so it's okay to release lk.
  if(lk != &ptable.lock){  //DOC: sleeplock0
    acquire(&ptable.lock);  //DOC: sleeplock1
    if (lk) release(lk);
  }
  // Go to sleep.
  p->chan = chan;
  p->state = SLEEPING;

  sched();

  // Tidy up.
  p->chan = 0;

  // Reacquire original lock.
  if(lk != &ptable.lock){  //DOC: sleeplock2
    release(&ptable.lock);
    if (lk) acquire(lk);
  }
}
#endif

//PAGEBREAK!
// Wake up all processes sleeping on chan.
// The ptable lock must be held.
#ifdef CS333_P4
static void
wakeup1(void *chan)
{
  struct proc *p;
  struct proc *temp;
  p = ptable.list[SLEEPING].head;
  while(p){
    if(p->chan == chan) //make sure p is in the same channel
    {
      temp = p->next;  //to make sure to hang onto the next proc.
      stateListRemove(&ptable.list[SLEEPING], p);
      assertState(p, SLEEPING, __FUNCTION__, __LINE__);
      p->state = RUNNABLE;
      stateListAdd(&ptable.ready[p->priority], p);
      p = temp;
    }
    else
      p = p->next; //go to the next process
  }
}

#elif CS333_P3
static void
wakeup1(void *chan)
{
  struct proc *p;
  p = ptable.list[SLEEPING].head;
  while(p){
    if(p->chan == chan) //make sure p is in the same channel
    {
      struct proc *temp = p->next;  //to make sure to hang onto the next proc.
      stateListRemove(&ptable.list[SLEEPING], p);
      assertState(p, SLEEPING, __FUNCTION__, __LINE__);
      p->state = RUNNABLE;
      stateListAdd(&ptable.list[RUNNABLE], p);
      p = temp;
    }
    else
      p = p->next; //go to the next process
  }
}
#else	//CS333_P1,P2
static void
wakeup1(void *chan)
{
  struct proc *p;

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == SLEEPING && p->chan == chan)
      p->state = RUNNABLE;
}
#endif


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
#ifdef CS333_P4
int
kill(int pid){
  struct proc *p;
  struct proc *temp;

  acquire(&ptable.lock);
  for(int i = 1; i < NELEM(states); i++){
    p = ptable.list[i].head;
    while(p){
      if(p->pid == pid){
        p->killed = 1;
        // Wake process from sleep if necessary.
        if(p->state == SLEEPING){
          temp = p->next; //to hold the next SLEEPING;
          stateListRemove(&ptable.list[SLEEPING], p);
          assertState(p, SLEEPING, __FUNCTION__, __LINE__);
          p->state = RUNNABLE;
          stateListAdd(&ptable.ready[p->priority], p);
          p = temp; //return p to the next item in the SLEEPING list
        }
        release(&ptable.lock);
        return 0;
      }
      p = p->next;
    }
  }

  for(int i = MAXPRIO; i >= PRIO_MIN; i--)
  {
    p = ptable.ready[i].head;
    while(p){
      if(p->pid == pid){
        p->killed = 1;
        release(&ptable.lock);
        return 0;
      }
      p = p->next;
    }
  }
  release(&ptable.lock);
  return -1;
}
#elif CS333_P3
int
kill(int pid){
  struct proc *p;
  struct proc *temp;

  acquire(&ptable.lock);
  for(int i = 1; i < NELEM(states); i++){
    p = ptable.list[i].head;
    while(p){
      if(p->pid == pid){
        p->killed = 1;
        // Wake process from sleep if necessary.
        if(p->state == SLEEPING){
          temp = p->next; //to hold the next SLEEPING;
          stateListRemove(&ptable.list[SLEEPING], p);
          assertState(p, SLEEPING, __FUNCTION__, __LINE__);
          p->state = RUNNABLE;
          // think carefully here: could there be further SLEEPING processes
          //that we need to wake up?
          //If we remove p from the sleeping list, what does p->next become?
          stateListAdd(&ptable.list[RUNNABLE], p);
          p = temp; //return p to the next item in the SLEEPING list
        }
        release(&ptable.lock);
        return 0;
      }
      p = p->next;
    }
  }
  release(&ptable.lock);
  return -1;
}
#else	//CS333_P1,P2
int
kill(int pid)
{
  struct proc *p;

  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->pid == pid){
      p->killed = 1;
      // Wake process from sleep if necessary.
      if(p->state == SLEEPING)
        p->state = RUNNABLE;
      release(&ptable.lock);
      return 0;
    }
  }
  release(&ptable.lock);
  return -1;
}
#endif

//PAGEBREAK: 36
// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.

#ifdef CS333_P3
void
procdumpP2P3P4(struct proc *p, char *state_string)
{
  int i;
  static int MAXNAME = 12;
  uint ppid;
  uint elapsed = ticks - p->start_ticks;
  uint milliseconds = elapsed%1000;
  uint seconds = (elapsed/1000)%60;

  uint cpu_milliseconds = p->cpu_ticks_total%1000;
  uint cpu_seconds = p->cpu_ticks_total/1000;

  if(p->parent == NULL)
    ppid = p->pid;
  else
    ppid = p->parent->pid;
  int len = strlen(p->name);
  if(len>MAXNAME)
  {
    p->name[MAXNAME] = '\0';
    len = MAXNAME;
  }
  cprintf("%d\t%s", p->pid, p->name);
  for(i = len; i<=MAXNAME; i++)
    cprintf(" ");

  cprintf("%d\t\t%d\t%d\t%d\t%d", p->uid, p->gid, ppid, p->priority, seconds);
  if(milliseconds >= 100)
    cprintf(".%d\t", milliseconds);
  else if(milliseconds < 100 || milliseconds >= 10)
    cprintf(".%d0\t", milliseconds);
  else if(milliseconds < 10)
    cprintf(".%d00\t", milliseconds);

  cprintf("%d", cpu_seconds);
  if(cpu_milliseconds >= 100)
    cprintf(".%d\t", cpu_milliseconds);
  else if(cpu_milliseconds < 100 || cpu_milliseconds >= 10)
    cprintf(".0%d\t", cpu_milliseconds);
  else if(cpu_milliseconds < 10)
    cprintf(".00%d\t", cpu_milliseconds);

  cprintf("%s\t%d\t", states[p->state], p->sz);

  return;
}
#elif defined(CS333_P1)
void
procdumpP1(struct proc *p, char *state_string)
{
  uint elapsed = ticks - p->start_ticks;
  uint milliseconds = elapsed%1000;
  uint seconds = (elapsed/1000)%60;

  cprintf("%d\t%s\t\t%d.%d\t%s\t%d\t",
      p->pid , p->name , seconds , milliseconds , states[p->state], p->sz);
  return;
}
#endif

void
procdump(void)
{
  int i;
  struct proc *p;
  char *state;
  uint pc[10];

#if defined(CS333_P4)
#define HEADER "\nPID\tName         UID\tGID\tPPID\tPrio\tElapsed\tCPU\tState\tSize\t PCs\n"
#elif defined(CS333_P2)
#define HEADER "\nPID\tName         UID\tGID\tPPID\tElapsed\tCPU\tState\tSize\t PCs\n"
#elif defined(CS333_P1)
#define HEADER "\nPID\tName         Elapsed\tState\tSize\t PCs\n"
#else
#define HEADER "\n"
#endif

  cprintf(HEADER);  // not conditionally compiled as must work in all project states

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";

    // see TODOs above this function
    // P2 and P3 are identical and the P4 change is minor
#if defined(CS333_P2)
    procdumpP2P3P4(p, state);
#elif defined(CS333_P1)
    procdumpP1(p, state);
#else
    cprintf("%d\t%s\t%s\t", p->pid, p->name, state);
#endif

    if(p->state == SLEEPING){
      getcallerpcs((uint*)p->context->ebp+2, pc);
      for(i=0; i<10 && pc[i] != 0; i++)
        cprintf(" %p", pc[i]);
    }
    cprintf("\n");
  }
#ifdef CS333_P1
  cprintf("$ ");  // simulate shell prompt
#endif // CS333_P1
}

#if defined(CS333_P3)
// list management helper functions
static void
stateListAdd(struct ptrs* list, struct proc* p)
{
  if((*list).head == NULL){
    (*list).head = p;
    (*list).tail = p;
    p->next = NULL;
  } else{
    ((*list).tail)->next = p;
    (*list).tail = ((*list).tail)->next;
    ((*list).tail)->next = NULL;
  }
}
#endif

#if defined(CS333_P3)
static int
stateListRemove(struct ptrs* list, struct proc* p)
{
  if((*list).head == NULL || (*list).tail == NULL || p == NULL){
    return -1;
  }

  struct proc* current = (*list).head;
  struct proc* previous = 0;

  if(current == p){
    (*list).head = ((*list).head)->next;
    // prevent tail remaining assigned when we've removed the only item
    // on the list
    if((*list).tail == p){
      (*list).tail = NULL;
    }
    return 0;
  }

  while(current){
    if(current == p){
      break;
    }

    previous = current;
    current = current->next;
  }

  // Process not found. return error
  if(current == NULL){
    return -1;
  }

  // Process found.
  if(current == (*list).tail){
    (*list).tail = previous;
    ((*list).tail)->next = NULL;
  } else{
    previous->next = current->next;
  }

  // Make sure p->next doesn't point into the list.
  p->next = NULL;

  return 0;
}
#endif

#if defined(CS333_P3)
static void
initProcessLists()
{
  int i;

  for (i = UNUSED; i <= ZOMBIE; i++) {
    ptable.list[i].head = NULL;
    ptable.list[i].tail = NULL;
  }
#if defined(CS333_P4)
  for (i = 0; i <= MAXPRIO; i++) {
    ptable.ready[i].head = NULL;
    ptable.ready[i].tail = NULL;
  }
#endif
}
#endif

#if defined(CS333_P3)
  static void
initFreeList(void)
{
  struct proc* p;

  for(p = ptable.proc; p < ptable.proc + NPROC; ++p){
    p->state = UNUSED;
    stateListAdd(&ptable.list[UNUSED], p);
  }
}
#endif

#if defined(CS333_P3)
// example usage:
// assertState(p, UNUSED, __FUNCTION__, __LINE__);
// This code uses gcc preprocessor directives. For details, see
// https://gcc.gnu.org/onlinedocs/cpp/Standard-Predefined-Macros.html
  static void
assertState(struct proc *p, enum procstate state, const char * func, int line)
{
  if (p->state == state)
    return;
  cprintf("Error: proc state is %s and should be %s.\nCalled from %s line %d\n",
      states[p->state], states[state], func, line);
  panic("Error: Process state incorrect in assertState()");
}
#endif

#if defined(CS333_P3)
// Project 3/4 control sequence support
void
printList(int state)
{
  int count = 0;
  //const int PER_LINE = 15;  // per line max on print
  //const int PER_LINE_Z = (PER_LINE/2);  // zombie list has more chars per entry on print
  struct proc *p;
  static char *stateNames[] = {  // note: sparse array
    [RUNNABLE]  "Runnable",
    [SLEEPING]  "Sleep",
    [RUNNING]   "Running",
    [ZOMBIE]    "Zombie"
  };


  if (state < UNUSED || state > ZOMBIE) {
    cprintf("Invalid control sequence\n");
    cprintf("$ ");  // simulate shell prompt
    return;
  }

  acquire(&ptable.lock);
#ifdef DEBUG
  checkProcs(__FILE__, __FUNCTION__, __LINE__);
#endif
#ifdef CS333_P4
  if (state == RUNNABLE) {
    printReadyLists();
    release(&ptable.lock);
    cprintf("$ ");  // simulate shell prompt
    return;
  }
#endif
  cprintf("\n%s List Processes:\n", stateNames[state]);
  p = ptable.list[state].head;
  while (p != NULL) {
    if (p->state != state) {  // sanity check
      cprintf("Error: PID %d on %s list but should be on %s\n",
          p->pid, states[p->state], states[state]);
      panic("Corrupted list\n");
    }
    if (state == ZOMBIE)
      cprintf("(%d, %d)", p->pid,
          (p->parent) ? p->parent->pid : p->pid);
    else
      cprintf("%d", p->pid);
    p = p->next;
    cprintf("%s", p ? " -> " : "\n");
    if (p && (++count) %
        ((state == ZOMBIE) ? PER_LINE_Z : PER_LINE) == 0)
      cprintf("\n");
  }
  release(&ptable.lock);
  cprintf("$ ");  // simulate shell prompt
  return;
}

  void
printFreeList(void)
{
  int count = 0;
  struct proc *p;

  acquire(&ptable.lock);
  p = ptable.list[UNUSED].head;
  while (p != NULL) {
    count++;
    p = p->next;
  }
  release(&ptable.lock);
  cprintf("\nFree List Size: %d processes\n", count);
  cprintf("$ ");  // simulate shell prompt
  return;
}

  void
printListStats()
{
  int i, count, total = 0;
  struct proc *p;

  acquire(&ptable.lock);
  for (i=UNUSED; i<=ZOMBIE; i++) {
    count = 0;
    p = ptable.list[i].head;
    while (p != NULL) {
      count++;
      if(p->state != i) {
        cprintf("\nlist invariant failed: process %d has state %s but is on list %s\n",
            p->pid, states[p->state], states[i]);
      }
      p = p->next;
    }
    cprintf("\n%s list has ", states[i]);
    if (count < 10) cprintf(" ");  // line up columns. we know NPROC < 100
    cprintf("%d processes", count);
    total += count;
  }
  release(&ptable.lock);
  cprintf("\nTotal on lists is: %d. NPROC = %d. %s",
      total, NPROC, (total == NPROC) ? "Congratulations!" : "Bummer");
  cprintf("\n$ ");  // simulate shell prompt
  return;
}
#endif // CS333_P3

#ifdef CS333_P4
void
printReadyList(struct proc *p, int prio)
{
  if (p == NULL) {
    cprintf("(NULL)\n");
    return;
  }
  int count = 0;
  do {
    cprintf("pid: %d, Budget: %d", p->pid, p->budget);
    if(p->priority != prio) {
      cprintf("\nlist invariant failed: process %d has prio %d but is on runnable list %d\n",
          p->pid, p->priority, prio);
    }
    p = p->next;
    cprintf("%s", p ? " -> " : "\n");
    if (p && (++count) % PER_LINE == 0)
      cprintf("\n");
  } while (p != NULL);
}

void
printReadyLists()
{
  struct proc *p;

  cprintf("Ready List Processes:\n");
  // this look must be changed based on MAX/MIN prio
  for (int i=PRIO_MAX; i >= PRIO_MIN; i--) {
    p = ptable.ready[i].head;
    if(p == NULL)
      continue;
    cprintf("Prio %d: ", i);
    if(p->state != RUNNABLE) {
      cprintf("\nlist invariant failed: process %d has state %s but is on ready list\n",
          p->pid, states[p->state]);
    }
    printReadyList(p, i);
  }
}
#endif // CS333_P4



#ifdef CS333_P2
int
getstheprocs(uint max, struct uproc* table)
{
  uint procs_copied = 0;
  struct proc *p;
  acquire(&ptable.lock);
  if(max < 0)
    return -1;

  for(p = ptable.proc; p != &ptable.proc[NPROC] && procs_copied < max; ++p)
  {
    if(p->state != UNUSED && p->state != EMBRYO)
    {
      table[procs_copied].pid = p->pid;
      table[procs_copied].uid = p->uid;
      table[procs_copied].gid = p->gid;
      if(p->parent == NULL)
        table[procs_copied].ppid = p->pid;
      else
        table[procs_copied].ppid = p->parent->pid; 
      table[procs_copied].elapsed_ticks = ticks - p->start_ticks;
      table[procs_copied].CPU_total_ticks = p->cpu_ticks_total;
      safestrcpy(table[procs_copied].state, states[p->state], STRMAX);
      table[procs_copied].size = p->sz;
      table[procs_copied].priority = p->priority;
      safestrcpy(table[procs_copied].name, p->name, sizeof(p->name));

      ++procs_copied;
    }
  }
  release(&ptable.lock);
  return procs_copied; 
}
#endif  //CS333_P2

#ifdef CS333_P4
int
setpriority(int pid, int priority)
{
  if(priority < 0 || priority > MAXPRIO)
    return -1;
  if(pid <= 0)
    return -1;
  acquire(&ptable.lock);
  struct proc *curr;
  //To check the P3 lists:
  for(int i = SLEEPING; i <= RUNNING; ++i)
  {
    curr = ptable.list[i].head;
    while(curr)
    {
      if(curr->pid == pid)
      {
        curr->priority = priority;
        curr->budget = DEFAULT_BUDGET;
        release(&ptable.lock);
        return 0;
      }
      curr = curr->next;
    }
  }

  //now to check the ready lists:
  for(int i = PRIO_MAX; i >= PRIO_MIN; --i)
  {
    if(ptable.ready[i].head)
      curr = ptable.ready[i].head;
    while(curr)
    {
      if(curr->pid == pid)
      {
        if(stateListRemove(&ptable.ready[curr->priority], curr) == -1)
          panic("\nThe process was not removed from the priority list in setpriority\n");
        curr->priority = priority;
        curr->budget = DEFAULT_BUDGET;
        stateListAdd(&ptable.ready[curr->priority], curr);
        release(&ptable.lock);
        return 0;
      }
      curr = curr->next;
    }
  }
  release(&ptable.lock);
  return -1;
}

int getpriority(int pid)
{
  if(pid < 0)
    return -1;

  acquire(&ptable.lock);
  struct proc *curr;

  //To check the P3 lists:
  for(int i = 0; i < NELEM(states); i++)
  {
    curr = ptable.list[i].head;
    while(curr)
    {
      if(curr->pid == pid)
      {
        if(curr->state == UNUSED)
          return -1;
        release(&ptable.lock);
        return curr->priority;
      }
      curr = curr->next;
    }
  }
  //now to check the ready lists:
  for(int i = PRIO_MAX; i >= PRIO_MIN; --i)
  {
    curr = ptable.ready[i].head;
    while(curr)
    {
      if(curr->pid == pid)
      {
        if(curr->state == UNUSED)
          return -1;
        release(&ptable.lock);
        return curr->priority;
      }
      curr = curr->next;
    }
  }
  release(&ptable.lock);
  return -1;
}

void
promoteProcs()
{
  acquire(&ptable.lock);
  struct proc *curr;
  struct proc *temp;
  //go through ACTIVE PROCS
  for(int i = EMBRYO; i <= RUNNING; i++)
  {
    curr = ptable.list[i].head;
    while(curr)
    {
      if(curr->priority < MAXPRIO)
      {
        if(curr->state == RUNNABLE)
        {
          if(stateListRemove(&ptable.ready[curr->priority], curr) == -1)
            panic("\nFailed to remove from RUNNING list in promteProcs()\n");
          assertState(curr, RUNNABLE, __FUNCTION__, __LINE__);

          curr->priority++;
          curr->budget = DEFAULT_BUDGET;
          stateListAdd(&ptable.ready[curr->priority], curr);
        }
        else
        {
          curr->priority++;
          curr->budget = DEFAULT_BUDGET;
        }
      }
      curr = curr->next;
    }
  }
  for(int i = MAXPRIO; i >= PRIO_MIN; i--)
  {
    curr = ptable.ready[i].head;
    while(curr)
    {
      temp = curr->next;
      if(curr->priority < MAXPRIO)
      {
        if(stateListRemove(&ptable.ready[curr->priority], curr) == -1)
          panic("\nFailed to remove from RUNNING list in promteProcs()\n");
        assertState(curr, RUNNABLE, __FUNCTION__, __LINE__);
        curr->priority++;
        curr->budget = DEFAULT_BUDGET;
        stateListAdd(&ptable.ready[curr->priority], curr);
      }
      curr = temp;
    }
  }
  release(&ptable.lock);
}
#endif  //CS333_P4
