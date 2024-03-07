记录代码片段的阅读随笔，便于之后整理

`allocproc` 寻找一个状态为UNUSED的进程
设置`pid`，并且分配物理内存作为页表，在页表中配置`(trapframe, trapline)`的映射
设置`context`中的`epc`为`forkret`，sp为内核栈栈顶，即新创建进程被调度之后的执行环境
`allocproc` 只在两处使用 1. `userinit` 系统初始化第一个进程 2. `fork` 系统初始化子进程
```c
static struct proc*
allocproc(void)
{
  struct proc *p;

  for(p = proc; p < &proc[NPROC]; p++) {
    acquire(&p->lock);
    if(p->state == UNUSED) {
      goto found;
    } else {
      release(&p->lock);
    }
  }
  return 0;

found:
  p->pid = allocpid();

  // Allocate a trapframe page.
  if((p->trapframe = (struct trapframe *)kalloc()) == 0){
    release(&p->lock);
    return 0;
  }

  // An empty user page table.
  p->pagetable = proc_pagetable(p);
  if(p->pagetable == 0){
    freeproc(p);
    release(&p->lock);
    return 0;
  }

  // Set up new context to start executing at forkret,
  // which returns to user space.
  memset(&p->context, 0, sizeof(p->context));
  p->context.ra = (uint64)forkret;
  p->context.sp = p->kstack + PGSIZE; 
  return p;
}
```