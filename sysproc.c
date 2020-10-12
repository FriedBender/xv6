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
	//TODO: if the PPID is NULL, then PPID=PID
	if(myproc()->parent == NULL)
		return myproc()->pid;
	return myproc()->parent->pid;
}

// set UID
int
sys_setuid(void)
{
	//TODO: Make sure values are correct: 0<= x <= 32767
	int uid;
	argint(0, &uid);
	if(uid < 0 || uid > 32767)
		return -1;
	return myproc()->uid = uid;
}

// set GID
int
sys_setgid(void)
{
	//TODO: Make sure values are correct: 0<= x <= 32767
	int gid;
	argint(0, &gid);
	if(gid < 0 || gid > 32767)
		return -1;
	return myproc()->gid = gid;
}
#endif	//CS333_P2
