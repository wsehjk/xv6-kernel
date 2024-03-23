# boot and first process

## boot


### first process
0号cpu在设置中断，内存分配，开启页表之后就会调用`userinit`函数设置第一个用户进程。
`userinit`首先调用`allocproc`获得一个分配好`pid`，设置好页表的进程结构。
之后将`initcode`复制到分配的物理内存并且做好页表映射 (`uvminit`)。
设置返回到用户空间需要用到的`pc`，以及`sp`。这个进程是硬编码产生的，不是通过`fork`得到的，返回用户空间需要用的信息只有`pc`和`sp`。之后将进程状态设置为可调度，等待cpu的调度
```c
// a user program that calls exec("/init")
// od -t xC initcode
uchar initcode[] = {
  0x17, 0x05, 0x00, 0x00, 0x13, 0x05, 0x45, 0x02,
  0x97, 0x05, 0x00, 0x00, 0x93, 0x85, 0x35, 0x02,
  0x93, 0x08, 0x70, 0x00, 0x73, 0x00, 0x00, 0x00,
  0x93, 0x08, 0x20, 0x00, 0x73, 0x00, 0x00, 0x00,
  0xef, 0xf0, 0x9f, 0xff, 0x2f, 0x69, 0x6e, 0x69,
  0x74, 0x00, 0x00, 0x24, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00
};

// Set up first user process.
void
userinit(void)
{
  struct proc *p;

  p = allocproc();  // 检查 0
  if (p == 0) {
    return ;
  }
  initproc = p;
  
  // allocate one user page and copy init's instructions
  // and data into it.
  uvminit(p->pagetable, initcode, sizeof(initcode));
  p->sz = PGSIZE;

  // prepare for the very first "return" from kernel to user.
  p->trapframe->epc = 0;      // user program counter
  p->trapframe->sp = PGSIZE;  // user stack pointer

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  p->state = RUNNABLE;

  release(&p->lock);
}
```
`initcode`是一段只执行`exec`系统调用的代码，
```asm
globl start
start:
        la a0, init
        la a1, argv
        li a7, SYS_exec
        ecall
# for(;;) exit();
exit:
        li a7, SYS_exit
        ecall
        jal exit
# char init[] = "/init\0";
init:
  .string "/init\0"
# char *argv[] = { init, 0 };
.p2align 2
argv:
  .long init
  .long 0
```