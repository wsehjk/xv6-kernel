# 系统调用的过程

`xv6`提供了`open`， `fork`，`wait`类似的系统调用函数。系统调用函数被封装的像过程调用函数一样，也需要用`call`指令，并且传递参数，记录返回地址。不过，系统调用函数的函数体是硬编码的汇编指令，并且只做两件事: 1. 将系统调用号放入寄存器，之后内核借此调用具体的`syscall handler`。2. `ecall`
```asm
#include "kernel/syscall.h"
.global fork
fork:
 li a7, SYS_fork
 ecall
 ret
```
`ecall`会执行一系列操作，以便进入内核
1. 关闭中断(disable interrupt)
2. 将`pc`的值赋给`sepc`寄存器
3. cpu变为`kernel mode`
4. `scause` 记录陷入内核的原因，这里是syscall，还有可能是page fault, device interrupt
5. 将`stvec`的值赋给`pc`
这里`stvec`指向`trampoline.s/uservec`代码，这段代码负责保存进程重要的寄存器，比如`sp`，`ra`，`tp`，到进程私有的`trapframe`内存中。`trapframe`是在进程创建时隐射到申请的物理页中，虚拟地址对于每个进程是一样的。并且从`trapframe`中获得在内核中运行需要的`kernel stack pointer`, `kernel pagetable`, `hartid`，以及需要跳转的`usertrap`函数地址。

之后跳转到`usertrap`
```c
//
// handle an interrupt, exception, or system call from user space.
// called from trampoline.S
//
void
usertrap(void)
{
  int which_dev = 0;

  if((r_sstatus() & SSTATUS_SPP) != 0)
    panic("usertrap: not from user mode");
  // send interrupts and exceptions to kerneltrap(),
  // since we're now in the kernel.
  w_stvec((uint64)kernelvec);
  struct proc *p = myproc();
  // save user program counter.
  p->trapframe->epc = r_sepc(); // 保存 user pc
  if(r_scause() == 8){
    // system call
    if(p->killed)
      exit(-1);
    // sepc points to the ecall instruction,
    // but we want to return to the next instruction.
    p->trapframe->epc += 4;
    // an interrupt will change sstatus &c registers,
    // so don't enable until done with those registers.
    intr_on();
    syscall();
  } 
  // else 

  if(p->killed)
    exit(-1);

  // give up the CPU if this is a timer interrupt.
  if(which_dev == 2)
    yield();
  usertrapret();
}
```
`usertrap`会先设置在内核中中断需要跳转的地址(stvec)。保存用户陷入内核时的`pc`(read sepc), 之后通过`scause`寄存器检查陷入内核原因，这里是`syscall`，之后就会开中断，并且调用`syscall()`，`syscall` 通过`trapframe`中`a7`寄存器来决定`syscall handler`。

之后`usertrap`调用`usertrapret`返回到用户空间。`usertrapret`做两件事 记录陷入内核需要用到的信息，为下次陷入内核做准备，并且恢复保存在`trapframe`中的寄存器信息。具体来看

```c
void
usertrapret(void)
{
  struct proc *p = myproc();

  // we're about to switch the destination of traps from
  // kerneltrap() to usertrap(), so turn off interrupts until
  // we're back in user space, where usertrap() is correct.
  intr_off();

  // send syscalls, interrupts, and exceptions to trampoline.S
  w_stvec(TRAMPOLINE + (uservec - trampoline));

  // set up trapframe values that uservec will need when
  // the process next re-enters the kernel.
  p->trapframe->kernel_satp = r_satp();         // kernel page table
  p->trapframe->kernel_sp = p->kstack + PGSIZE; // process's kernel stack
  p->trapframe->kernel_trap = (uint64)usertrap;
  p->trapframe->kernel_hartid = r_tp();         // hartid for cpuid()

  // set up the registers that trampoline.S's sret will use
  // to get to user space.
  
  // set S Previous Privilege mode to User.
  unsigned long x = r_sstatus();
  x &= ~SSTATUS_SPP; // clear SPP to 0 for user mode
  x |= SSTATUS_SPIE; // enable interrupts in user mode
  w_sstatus(x);

  // set S Exception Program Counter to the saved user pc.
  w_sepc(p->trapframe->epc);

  // tell trampoline.S the user page table to switch to.
  uint64 satp = MAKE_SATP(p->pagetable);

  // jump to trampoline.S at the top of memory, which 
  // switches to the user page table, restores user registers,
  // and switches to user mode with sret.
  uint64 fn = TRAMPOLINE + (userret - trampoline);
  ((void (*)(uint64,uint64))fn)(TRAPFRAME, satp);  // 函数指针
}
```

首先关闭中断，将`ecall`指令需要用到的跳转地址写入`stvec`寄存器，将`kernel pagetable`，`kernel stack pointer`，`usertrap()`地址，`cpu id`记录在`p->trapframe`中，进入内核之前需要设置这些信息，这些信息只在内核中可见，所有在退出内核之前将信息保存在`trapframe`中。此外，还把更新过的`p->trapframe->epc`写入`sepc`中，返回用户空间时，赋给`pc`。之后通过函数指针调用`trampoline.s/userret`，`userret`和`uservec`正好相反，设置用户页表， 恢复`p->trapframe`中的寄存器信息，`sret`指令返回用户空间。`sret`指令和`ecall`指令正好相反。

1. 开启中断(enable interrupt)
2. 将`sepc`的值赋给`pc`寄存器
3. cpu变为`user mode`

`p->trapframe->epc`表示返回用户空间，用户应该执行的第一条指令地址，一般来说就是`ecall`时保存的`pc`值，但是有几个特殊
1. 如果系统调用，`ecall`是保存的`pc`就是`ecall`执行地址，显然返回地址应该是`ecall`下一条指令，所以`p->trapframe->epc += 4`
2. `exec`系统调用不会返回，之前保存的`epc`没有用。将`epc`设置成`elf`文件中的`elf.entry`
