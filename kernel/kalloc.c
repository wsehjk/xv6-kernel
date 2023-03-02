// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem[NCPU];

void
kinit()
{
  for(int i = 0; i < NCPU; ++i) {
    initlock(&kmem[i].lock, "kmem");
    snprintf((kmem[i].lock).name, 1024, "kem_%d", i);
  }
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)  // 将全部的空闲页分配给最先运行kinit的cpu
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;
  push_off();   // cpuid()的使用要关中断
  int id = cpuid();
  acquire(&kmem[id].lock);  // 将回收的page放在cpu自己的free_list中
  r->next = kmem[id].freelist;   // 写在内存里
  kmem[id].freelist = r;
  release(&kmem[id].lock);
  pop_off();
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  push_off();   // 先关闭中断
  int i = cpuid();
  int j = 0;
  while (j < NCPU) {  // j == NCPU，说明所有cpu的free_list都为空
    acquire(&kmem[i].lock);
    r = kmem[i].freelist;
    if(r)
      kmem[i].freelist = r->next;
    release(&kmem[i].lock);
    if (r) break;  // 在i free_list中找到空闲页, 退出循环
    j ++; // 记录已经寻找了多少cpu的free_list
    i = (i+1)%NCPU; // 下一个free_list
  }
  pop_off();

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}
