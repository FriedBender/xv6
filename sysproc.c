#include "types.h"
#include "x86.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#ifdef PDX_XV6
#include "pdx-kernel.h"
#endif // PDX_XV6
#ifdef CS333_P2
#include "uproc.h"
#endif  //CS333_P2

int
sys_fork(void)
{
  return fork();
}

int
sys_exit(void)
{
  exit();
  return 0;  // not reached
}

int
sys_wait(void)
{
  return wait();
}

int
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

int
sys_getpid(void)
{
  return myproc()->pid;
}

int
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

int
sys_sleep(void)
{
  int n;
  uint ticks0;

  if(argint(0, &n) < 0)
    return -1;
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(myproc()->killed){
      return -1;
    }
    sleep(&ticks, (struct spinlock *)0);
  }
  return 0;
}

// return how many clock tick interrupts have occurred
// since start.
int
sys_uptime(void)
{
  uint xticks;

  xticks = ticks;
  return xticks;
}

#ifdef PDX_XV6
// shutdown QEMU
int
sys_halt(void)
{
  do_shutdown();  // never returns
  return 0;
}
#endif // PDX_XV6

#ifdef CS333_P1
//get the current date
int
sys_date(void)
{
    struct rtcdate *d;

    if(argptr(0 , (void*)&d , sizeof(struct rtcdate)) <0)
        return -1;

    cmostime(d);
    
    return 0;
}
#endif // CS333_P1

#ifdef CS333_P2
// UID of the current process
uint
sys_getuid(void)
{
  return myproc()->uid;
}

// GID of the current process
uint
sys_getgid(void)
{
  return myproc()->gid;
}

// process ID of the parent process
uint
sys_getppid(void)
{
  if(myproc()->parent == NULL)
    return myproc()->pid;
  return myproc()->parent->pid;
}

// set UID
int
sys_setuid(void)
{
  int uid;
  if(argint(0, &uid)<0)
    return -1;
  if(uid < 0 || uid > 32767)
    return -1;
  myproc()->uid = uid;
  return 0;
}

// set GID
int
sys_setgid(void)
{
 int gid;
 if(argint(0, &gid)<0)
    return -1;
 if(gid < 0 || gid > 32767)
    return -1;
  myproc()->gid = gid;
  return 0;
}

//For PS system call
int
sys_getprocs(void)
{
  int max = 0;
  struct uproc* table;
  if(argint(0 , &max) < 0)
    return -1;
  if((argptr(1 , (void*)&table , sizeof(struct uproc)*max)) <0)
    return -1;
  return getstheprocs( max , table);
}
#endif	//CS333_P2

#ifdef CS333_P4
int
sys_setpriority(void)
{
  int pid;
  int priority;
  if(argint(0, &pid) == -1)
    return -1;
  if(argint(1, &priority) == -1)
    return -1;
  return setpriority(pid, priority);  //goes into kernel mode to find the pid,
                                  // and set priority for that pid.
}

int
sys_getpriority(void)
{
  int pid;
  if(argint(0, &pid) == -1)
    return -1;
  return getpriority(pid);
}
#endif  //CS333_P4
