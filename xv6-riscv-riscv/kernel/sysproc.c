#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"
#include "vm.h"
#include "schedinfo.h"

extern struct proc proc[NPROC];
uint64
sys_exit(void)
{
  int n;
  argint(0, &n);
  kexit(n);
  return 0; // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

//Q1
uint64
sys_getcpu(void)
{
  push_off();
  int id= cpuid();
  pop_off();

  return id;
}

uint64
sys_schedinfo(void)
{
    int pid;
    uint64 addr;

    argint(0, &pid);
    argaddr(1, &addr);

    struct proc *p;
    struct proc *cur = myproc();
    struct schedinfo si;

    memset(&si, 0, sizeof(si));

    for (p = proc; p < &proc[NPROC]; p++) {
        acquire(&p->lock);

        if (p->pid == pid && p->state != UNUSED) {
            si.pid = p->pid;
            si.state = p->state;
            si.nsched = p->nsched;
            si.rticks = p->rticks;
            si.lastcpu = p->lastcpu;
            si.slice = p->slice;
            si.ctime = p->ctime;
            si.ftime = p->ftime;
            si.firstcpu = p->firstcpu;

            release(&p->lock);

            if (copyout(cur->pagetable, cur->sz, addr,
            (char *)&si, sizeof(si)) < 0)
    return -1;

            return 0;
        }

        release(&p->lock);
    }

    return -1;
}

uint64
sys_setslice(void)
{
  int pid, n;
  struct proc *p;

  argint(0, &pid);
  argint(1, &n);

  // Validate time slice
  if (n < 1 || n > 100)
    return -1;

  // PID 0 means the calling process
  if (pid == 0) {
    p = myproc();
    acquire(&p->lock);
    p->slice = n;
    p->used = 0;
    release(&p->lock);
    return 0;
  }

  // Otherwise, find the process with the given PID
  for (p = proc; p < &proc[NPROC]; p++) {
    acquire(&p->lock);

    if (p->pid == pid && p->state != UNUSED) {
      p->slice = n;
      p->used = 0;
      release(&p->lock);
      return 0;
    }

    release(&p->lock);
  }

  return -1;
} 

uint64
sys_fork(void)
{
  return kfork();
}

uint64
sys_wait(void)
{
  uint64 p;
  argaddr(0, &p);
  return kwait(p);
}

uint64
sys_sbrk(void)
{
  uint64 addr;
  int t;
  int n;

  argint(0, &n);
  argint(1, &t);
  addr = myproc()->sz;

  if (t == SBRK_EAGER || n < 0) {
    if (growproc(n) < 0) {
      return -1;
    }
  } else {
    // Lazily allocate memory for this process: increase its memory
    // size but don't allocate memory. If the processes uses the
    // memory, vmfault() will allocate it.
    if (addr + n < addr)
      return -1;
    if (addr + n > TRAPFRAME)
      return -1;
    myproc()->sz += n;
  }
  return addr;
}

uint64
sys_pause(void)
{
  int n;
  uint ticks0;

  argint(0, &n);
  if (n < 0)
    n = 0;
  acquire(&tickslock);
  ticks0 = ticks;
  while (ticks - ticks0 < n) {
    if (killed(myproc())) {
      release(&tickslock);
      return -1;
    }
    sleep_prepare(&ticks);
    release(&tickslock);
    sleep();
    acquire(&tickslock);
  }
  release(&tickslock);
  return 0;
}

uint64
sys_kill(void)
{
  int pid;

  argint(0, &pid);
  return kkill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

//Q3
extern int forkwatch_enabled;

uint64
sys_forkwatch(void)
{
  int on;
  int old = forkwatch_enabled;

  argint(0, &on);
  forkwatch_enabled = (on != 0);

  return old;
}