# sleep 系统调用的实现
`sleep n` 调用作用是进程在内核沉睡`n`个时钟周期。处理句柄是`sys_sleep`
```c
uint64 sys_sleep(void){
  int n;
  uint ticks0;
  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
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
// ~/xv6-2020/kernel/sysproc.c
```

内核获取传入的参数，然后对`tickslock`加锁，获取系统当前的`ticks`，`tickslock`和`ticks`都是全局变量。
```cpp
struct spinlock tickslock;
uint ticks;
```

`while`循环先检测睡眠条件是否满足，以及沉睡中的进程状态，之后调用`sleep`函数。`sleep`函数作用是将进程放在某个集合中，并且修改进程状态，之后调用`sched()`函数，内核进行进程切换。

*将进程放在某个集合中*是通过将`p->chan = chan`指向某个内存地址实现的，这里就是`ticks`变量的内存地址。那么进程的状态`chan = &ticks`且状态为`SLEEPING`的进程就在一个集合里。

```c
// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void
sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();
  
  // Must acquire p->lock in order to
  // change p->state and then call sched.
  // Once we hold p->lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup locks p->lock),
  // so it's okay to release lk.
  if(lk != &p->lock){  //DOC: sleeplock0
    acquire(&p->lock);  //DOC: sleeplock1
    release(lk);
  }
  // Go to sleep.
  p->chan = chan;
  p->state = SLEEPING;

  sched(); // giveup cpu
  // Tidy up.
  p->chan = 0;
  // Reacquire original lock.
  if(lk != &p->lock){
    release(&p->lock);
    acquire(lk);
  }
}
```
系统的时钟中断`clockintr()`对`ticks`计数，并且调用`wake(void* chan)`唤醒`p->chan = chan`的进程，这里就是`&ticks`。
```c
void
clockintr()
{
  acquire(&tickslock);
  ticks++;
  wakeup(&ticks);
  release(&tickslock);
}


// Wake up all processes sleeping on chan.
// Must be called without any p->lock.
void
wakeup(void *chan)
{
  struct proc *p;

  for(p = proc; p < &proc[NPROC]; p++) {
    acquire(&p->lock);
    if(p->state == SLEEPING && p->chan == chan) {
      p->state = RUNNABLE;
    }
    release(&p->lock);
  }
}
```
`wakeup`也只是将进程状态设置为可执行，并不是立刻执行进程。状态改变的进程等待调度，从`sched`中返回，到`sleep`，再返回到`sys_sleep()`. `sys_sleep`检查睡眠条件`ticks - tick0 < n`，如果满足继续上述流程。
可以看到系统并不知道有进程沉睡`n`个时钟周期，需要在特定的时间唤醒某个进程。只是在特定的时钟中断时唤醒进程，检查睡眠条件。`xv6`中有很多`p->chan = &(内核变量)`的代码，这些代码都利用地址的唯一性，将进程放在特定的集合中，类似与并查集。具体的`sched`，系统调用过程之后分析。
## 问题
1. &ticks 这个地址是虚拟地址还是物理地址？
2. 注意到 `sched`函数调用之前`process->lock`已经加锁，而`wakeup`需要对`process->lock`加锁，为什么没有问题。