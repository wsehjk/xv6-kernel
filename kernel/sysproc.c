#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

uint64
sys_exit(void)
{
  int n;
  if(argint(0, &n) < 0)
    return -1;
  exit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  if(argaddr(0, &p) < 0)
    return -1;
  return wait(p);
}

uint64
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

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;

  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  backtrace();
  while(ticks - ticks0 < n){
    if(myproc()->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

uint64
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
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

uint64
sys_sigalarm(void) 
{
  int n;
  uint64 handler;
  struct proc* p;
  if (argint(0, &n) < 0) {
    return -1;
  }
  if (argaddr(1, &handler) < 0) {
    return -1;
  }
  p = myproc();
  if(n == 0) {
    p->elapsed_time = 0;
    p->handler = 0;
    p->interval = 0;
    return 0;
  }
  pte_t* pte = walk(p->pagetable, handler, 0);
  if(pte == 0)  // level2 和level1对应的页表项不存在
    return -1;
  if((*pte & PTE_V) == 0)  // level0 的页表项无效
    return -1;
  if((*pte & PTE_U) == 0)  // level0的页表项用户不可见
    return -1;
  if((*pte & PTE_X) == 0) // 没有执行权限
    return -1;
  p->handler = handler;
  p->interval = n;
  p->elapsed_time = 0;
  return 0;
}


uint64 sys_sigreturn(void) {
  struct proc* p = myproc();
  //恢复中断发生时，寄存器信息，回到被中断的代码
  p->trapframe->ra  =  p->handler_saved.ra;    
  p->trapframe->sp  =  p->handler_saved.sp;
  p->trapframe->gp  =  p->handler_saved.gp;
  p->trapframe->tp  =  p->handler_saved.tp;
  p->trapframe->t0  =  p->handler_saved.t0;
  p->trapframe->t1  =  p->handler_saved.t1;
  p->trapframe->t2  =  p->handler_saved.t2;
  p->trapframe->s0  =  p->handler_saved.s0;
  p->trapframe->s1  =  p->handler_saved.s1;
  p->trapframe->a0  =  p->handler_saved.a0;
  p->trapframe->a1  =  p->handler_saved.a1;
  p->trapframe->a2  =  p->handler_saved.a2;
  p->trapframe->a3  =  p->handler_saved.a3;
  p->trapframe->a4  =  p->handler_saved.a4;
  p->trapframe->a5  =  p->handler_saved.a5;
  p->trapframe->a6  =  p->handler_saved.a6;
  p->trapframe->a7  =  p->handler_saved.a7;
  p->trapframe->s2  =  p->handler_saved.s2;
  p->trapframe->s3  =  p->handler_saved.s3;
  p->trapframe->s4  =  p->handler_saved.s4;
  p->trapframe->s5  =  p->handler_saved.s5;
  p->trapframe->s6  =  p->handler_saved.s6;
  p->trapframe->s7  =  p->handler_saved.s7;
  p->trapframe->s8  =  p->handler_saved.s8;
  p->trapframe->s9  =  p->handler_saved.s9;
  p->trapframe->s10 =  p->handler_saved.s10;
  p->trapframe->s11 =  p->handler_saved.s11;
  p->trapframe->t3  =  p->handler_saved.t3;
  p->trapframe->t4  =  p->handler_saved.t4;
  p->trapframe->t5  =  p->handler_saved.t5;
  p->trapframe->t6  =  p->handler_saved.t6;
  p->trapframe->epc =  p->handler_saved.epc;
  p->handler_called =  0;
  return 0;
}