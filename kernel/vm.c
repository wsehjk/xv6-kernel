#include "param.h"
#include "types.h"
#include "memlayout.h"
#include "elf.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"

/*
 * the kernel's page table.
 */
pagetable_t kernel_pagetable;

extern char etext[];  // kernel.ld sets this to end of kernel code.

extern char trampoline[]; // trampoline.S

/*
 * create a direct-map page table for the kernel.
 */
void
kvminit()
{
  kernel_pagetable = (pagetable_t) kalloc();// 分配一页 存储根页目录 
  memset(kernel_pagetable, 0, PGSIZE);
  // 之后映射 clint 到 Trampoline 所有虚拟内存

  // CLINT
  kvmmap(CLINT, CLINT, 0x10000, PTE_R | PTE_W);

  // PLIC
  kvmmap(PLIC, PLIC, 0x400000, PTE_R | PTE_W);

  // uart registers
  kvmmap(UART0, UART0, PGSIZE, PTE_R | PTE_W);

  // virtio mmio disk interface
  kvmmap(VIRTIO0, VIRTIO0, PGSIZE, PTE_R | PTE_W);  
  // 将VIRTIO0开始的大小为PGSIZE的虚拟内存映射到VIRTIO0开始的连续物理内存

  // map kernel text executable and read-only.
  kvmmap(KERNBASE, KERNBASE, (uint64)etext-KERNBASE, PTE_R | PTE_X);

  // map kernel data and the physical RAM we'll make use of.
  kvmmap((uint64)etext, (uint64)etext, PHYSTOP-(uint64)etext, PTE_R | PTE_W);

  // map the trampoline for trap entry/exit to
  // the highest virtual address in the kernel.
  kvmmap(TRAMPOLINE, (uint64)trampoline, PGSIZE, PTE_R | PTE_X);

//  vmprint(kernel_pagetable);
}

// create a kernel pagetable for process 
pagetable_t proc_kvminit() {
  pagetable_t pagetable;
  if ((pagetable = uvmcreate()) == 0) {
    return 0;
  }

  //if(mappages(pagetable, CLINT, CLINT_SIZE, CLINT, PTE_W|PTE_R) < 0){
  //  uvmfree(pagetable, 0);
  //  return 0;
  //}

  // PLIC
  if(mappages(pagetable, PLIC, PLIC_SIZE, PLIC, PTE_R | PTE_W) < 0){
  //  uvmunmap(pagetable, CLINT, PGROUNDUP(CLINT_SIZE)/PGSIZE, 0); 
    uvmfree(pagetable, 0);
    return 0;
  }

  // uart registers
  if(mappages(pagetable, UART0, UART0_SIZE, UART0, PTE_R | PTE_W) < 0){
  //  uvmunmap(pagetable, CLINT, PGROUNDUP(CLINT_SIZE)/PGSIZE, 0); 
    uvmunmap(pagetable, PLIC, PGROUNDUP(PLIC_SIZE)/PGSIZE, 0); 
    uvmfree(pagetable, 0);
    return 0;
  }

  // virtio mmio disk interface
  if(mappages(pagetable, VIRTIO0, VIRTIO0_SIZE, VIRTIO0, PTE_R | PTE_W) < 0){
  //  uvmunmap(pagetable, CLINT, PGROUNDUP(CLINT_SIZE)/PGSIZE, 0); 
    uvmunmap(pagetable, PLIC, PGROUNDUP(PLIC_SIZE)/PGSIZE, 0); 
    uvmunmap(pagetable, UART0, PGROUNDUP(UART0_SIZE)/PGSIZE, 0); 
    uvmfree(pagetable, 0);
    return 0;
  } 
  // 将VIRTIO0开始的大小为PGSIZE的虚拟内存映射到VIRTIO0开始的连续物理内存

  // map kernel text executable and read-only.
  if(mappages(pagetable, KERNBASE, (uint64)etext-KERNBASE, KERNBASE, PTE_R | PTE_X) < 0) {
  //  uvmunmap(pagetable, CLINT, PGROUNDUP(CLINT_SIZE)/PGSIZE, 0); 
    uvmunmap(pagetable, PLIC, PGROUNDUP(PLIC_SIZE)/PGSIZE, 0); 
    uvmunmap(pagetable, UART0, PGROUNDUP(UART0_SIZE)/PGSIZE, 0); 
    uvmunmap(pagetable, VIRTIO0, PGROUNDUP(VIRTIO0_SIZE)/PGSIZE, 0);
    uvmfree(pagetable, 0);
    return 0;
  } 

  // map kernel data and the physical RAM we'll make use of.
  if(mappages(pagetable, (uint64)etext, PHYSTOP-(uint64)etext, (uint64)etext, PTE_R | PTE_W) < 0) {
  //  uvmunmap(pagetable, CLINT, PGROUNDUP(CLINT_SIZE)/PGSIZE, 0); 
    uvmunmap(pagetable, PLIC, PGROUNDUP(PLIC_SIZE)/PGSIZE, 0); 
    uvmunmap(pagetable, UART0, PGROUNDUP(UART0_SIZE)/PGSIZE, 0); 
    uvmunmap(pagetable, VIRTIO0, PGROUNDUP(VIRTIO0_SIZE)/PGSIZE, 0); 
    uvmunmap(pagetable, KERNBASE, PGROUNDUP((uint64)etext-KERNBASE)/PGSIZE, 0); 
    uvmfree(pagetable, 0);
    return 0;
  }

  // map the trampoline for trap entry/exit to
  // the highest virtual address in the kernel.
  if(mappages(pagetable, TRAMPOLINE, TRAMPOLINE_SIZE, (uint64)trampoline, PTE_R | PTE_X) < 0) {
  //  uvmunmap(pagetable, CLINT, PGROUNDUP(CLINT_SIZE)/PGSIZE, 0); 
    uvmunmap(pagetable, PLIC, PGROUNDUP(PLIC_SIZE)/PGSIZE, 0); 
    uvmunmap(pagetable, UART0, PGROUNDUP(UART0_SIZE)/PGSIZE, 0); 
    uvmunmap(pagetable, VIRTIO0, PGROUNDUP(VIRTIO0_SIZE)/PGSIZE, 0); 
    uvmunmap(pagetable, KERNBASE, PGROUNDUP((uint64)etext-KERNBASE)/PGSIZE, 0); 
    uvmunmap(pagetable, (uint64)etext, PGROUNDUP(PHYSTOP-(uint64)etext)/PGSIZE, 0); 
    uvmfree(pagetable, 0);
    return 0;
  } 
  return pagetable;
}

// Switch h/w page table register to the kernel's page table,
// and enable paging.
void
kvminithart()
{
  w_satp(MAKE_SATP(kernel_pagetable));
  sfence_vma();
}

// Return the address of the PTE in page table pagetable
// that corresponds to virtual address va.  If alloc!=0,
// create any required page-table pages.
//
// The risc-v Sv39 scheme has three levels of page-table
// pages. A page-table page contains 512 64-bit PTEs.
// A 64-bit virtual address is split into five fields:
//   39..63 -- must be zero.
//   30..38 -- 9 bits of level-2 index.
//   21..29 -- 9 bits of level-1 index.
//   12..20 -- 9 bits of level-0 index.
//    0..11 -- 12 bits of byte offset within the page.
pte_t *
walk(pagetable_t pagetable, uint64 va, int alloc)
{
  if(va >= MAXVA)
    panic("walk");
  // pagetable 指向每个level页目录的地址，初始为根目录(level 为2)
  for(int level = 2; level > 0; level--) {    
    pte_t *pte = &pagetable[PX(level, va)]; 
    if(*pte & PTE_V) {  // pte有效，找到下一级页目录地址
      pagetable = (pagetable_t)PTE2PA(*pte);
    } else {
      if(!alloc || (pagetable = (pde_t*)kalloc()) == 0)
        return 0; // alloc 为0 或者kalloc()返回0（分配失败）返回零
      memset(pagetable, 0, PGSIZE); // 将新分配的页表填写0
      *pte = PA2PTE(pagetable) | PTE_V;  // 设置页目录项， page_table指向下一层页目录
    }  // level2 和level1的标志位设为v即可
  }
  return &pagetable[PX(0, va)];  // 返回最低级的pte地址
}

// Look up a virtual address, return the physical address,
// or 0 if not mapped.
// Can only be used to look up user pages.
uint64
walkaddr(pagetable_t pagetable, uint64 va)
{
  pte_t *pte;
  uint64 pa;

  if(va >= MAXVA)
    return 0;

  pte = walk(pagetable, va, 0);
  if(pte == 0)
    return 0;
  if((*pte & PTE_V) == 0)
    return 0;
  if((*pte & PTE_U) == 0)  // user pages PTE_V flag set 
    return 0;
  pa = PTE2PA(*pte);
  return pa;
}

// add a mapping to the kernel page table.
// only used when booting.
// does not flush TLB or enable paging.
void
kvmmap(uint64 va, uint64 pa, uint64 sz, int perm)
{
  if(mappages(kernel_pagetable, va, sz, pa, perm) != 0)
    panic("kvmmap");
}

// translate a kernel virtual address to
// a physical address. only needed for
// addresses on the stack.
// assumes va is page aligned.
uint64
kvmpa(uint64 va)
{
  uint64 off = va % PGSIZE;
  pte_t *pte;
  uint64 pa;
  
  pte = walk(kernel_pagetable, va, 0);
  if(pte == 0)
    panic("kvmpa");
  if((*pte & PTE_V) == 0)
    panic("kvmpa");
  pa = PTE2PA(*pte);
  return pa+off;
}

// Create PTEs for virtual addresses starting at va that refer to
// physical addresses starting at pa. va and size might not
// be page-aligned. Returns 0 on success, -1 if walk() couldn't
// allocate a needed page-table page.
int
mappages(pagetable_t pagetable, uint64 va, uint64 size, uint64 pa, int perm)
{
  // 将va开始大小为size(字节)的虚拟内存映射到pa开始的物理内存
  uint64 a, last;
  pte_t *pte;

  a = PGROUNDDOWN(va);  // 获得开始页号
  last = PGROUNDDOWN(va + size - 1);   // 区域内最后一个页号
  for(;;){
    if((pte = walk(pagetable, a, 1)) == 0)//为虚拟地址寻找一个pte,如果没有，则创建一个
      return -1;  // 创建页表失败
    if(*pte & PTE_V) // pte已经有效
      panic("remap");
    *pte = PA2PTE(pa) | perm | PTE_V;   // 设置pte 物理页号以及标志位
    if(a == last)   // 已经映射好区域内最后一页
      break;
    a += PGSIZE;  // 映射下一页， 
    pa += PGSIZE;
  }
  return 0;
}

// Remove npages of mappings starting from va. va must be
// page-aligned. The mappings must exist.
// Optionally free the physical memory.
// 删除从va开始的npage页的pte,如果do_free则释放映射的物理内存
void
uvmunmap(pagetable_t pagetable, uint64 va, uint64 npages, int do_free)
{
  uint64 a;
  pte_t *pte;

  if((va % PGSIZE) != 0)
    panic("uvmunmap: not aligned");

  for(a = va; a < va + npages*PGSIZE; a += PGSIZE){
    if((pte = walk(pagetable, a, 0)) == 0) // 查找a对应pte,
      panic("uvmunmap: walk");  
    if((*pte & PTE_V) == 0) {  
       panic("uvmunmap: not mapped");
    } // level0 pte 为invalid
    if(PTE_FLAGS(*pte) == PTE_V)   // level2 和level1的标志只设为pte_v
      panic("uvmunmap: not a leaf");  // walk()+13
    if(do_free){   // 回收内存
      uint64 pa = PTE2PA(*pte);
      kfree((void*)pa);
    }
    *pte = 0;
  }
}

// create an empty user page table.
// returns 0 if out of memory.
pagetable_t
uvmcreate()
{
  pagetable_t pagetable;
  pagetable = (pagetable_t) kalloc();
  if(pagetable == 0)
    return 0;
  memset(pagetable, 0, PGSIZE);
  return pagetable;
}

// Load the user initcode into address 0 of pagetable,
// for the very first process.
// sz must be less than a page.
void
uvminit(pagetable_t pagetable, uchar *src, uint sz)
{
  char *mem;

  if(sz >= PGSIZE)
    panic("inituvm: more than a page");
  mem = kalloc();// 分配一页物理内存
  memset(mem, 0, PGSIZE); // 填入垃圾值
  mappages(pagetable, 0, PGSIZE, (uint64)mem, PTE_W|PTE_R|PTE_X|PTE_U);
  // 从0开始的一页虚拟内存映射到mem处
  memmove(mem, src, sz);// 内核使用物理地址
}

// Allocate PTEs and physical memory to grow process from oldsz to
// newsz, which need not be page aligned.  Returns new size or 0 on error.
uint64
uvmalloc(pagetable_t pagetable, uint64 oldsz, uint64 newsz)
{
  char *mem;
  uint64 a;

  if(newsz < oldsz)
    return oldsz;
  
  oldsz = PGROUNDUP(oldsz); // oldsz在某个page中间， up得到下一页起始地址
  for(a = oldsz; a < newsz; a += PGSIZE){  // 按页分配内存
    mem = kalloc();    // kmem.freelist 
    if(mem == 0){ // 分配失败
      uvmdealloc(pagetable, a, oldsz);  // 就把[oldsz, a)分配的内存释放掉
      return 0;
    }
    memset(mem, 0, PGSIZE);  // 填写垃圾值
    if(mappages(pagetable, a, PGSIZE, (uint64)mem, PTE_W|PTE_X|PTE_R|PTE_U) != 0){
      kfree(mem);// mappages中 walk返回pte为0（申请页表内存失败）mem内存还没有被映射，直接释放
      uvmdealloc(pagetable, a, oldsz);
      return 0;
    }
  }
  return newsz;
}

// Deallocate user pages to bring the process size from oldsz to
// newsz.  oldsz and newsz need not be page-aligned, nor does newsz
// need to be less than oldsz.  oldsz can be larger than the actual
// process size.  Returns the new process size.
uint64   // myproc()->pagetable
uvmdealloc(pagetable_t pagetable, uint64 oldsz, uint64 newsz)
{
  if(newsz >= oldsz)
    return oldsz;
  // newsz < oldsz 
  if(PGROUNDUP(newsz) < PGROUNDUP(oldsz)){
    int npages = (PGROUNDUP(oldsz) - PGROUNDUP(newsz)) / PGSIZE;
    uvmunmap(pagetable, PGROUNDUP(newsz), npages, 1);
  }

  return newsz;
}

// Recursively free page-table pages.
// All leaf mappings must already have been removed.
void
freewalk(pagetable_t pagetable)
{
  // there are 2^9 = 512 PTEs in a page table.
  for(int i = 0; i < 512; i++){
    pte_t pte = pagetable[i];
    if((pte & PTE_V) && (pte & (PTE_R|PTE_W|PTE_X)) == 0){//PTE_V有效，并且是level2和level1的表项
      // this PTE points to a lower-level page table.
      uint64 child = PTE2PA(pte);
      freewalk((pagetable_t)child); // 删除子页表
      pagetable[i] = 0;
    } else if(pte & PTE_V){
      panic("freewalk: leaf");
    }
  }
  kfree((void*)pagetable); // 释放页表占用的内存
}
// lab3: 
const char* indents[]= {".. .. ..", ".. ..", "..", 0};
void printwalk(pagetable_t pagetable, int level) {
    for (int i = 0; i < 512; ++i) {
      pte_t pte = pagetable[i];
      if ((pte & PTE_V)) {
        uint64 child = PTE2PA(pte); 
        printf("%s%d: pte %p pa %p\n", indents[level],i, pte, child);
        if (level)
          printwalk((pagetable_t)child, level-1);
      }
    }
}
void vmprint(pagetable_t pagetable) {
  printf("page table %p\n", pagetable);
  printwalk(pagetable, 2);
  return ;
}

// Free user memory pages,
// then free page-table pages.
// 通过页表找到物理地址，回收内存(sz>0)并且回收页表占用的内存
void
uvmfree(pagetable_t pagetable, uint64 sz)
{
  if(sz > 0)  // 从地址0开始删除所有的页表项，并且释放内存
    uvmunmap(pagetable, 0, PGROUNDUP(sz)/PGSIZE, 1);
  freewalk(pagetable); // 递归释放页表占用的内存
}

// Given a parent process's page table, copy
// its memory into a child's page table.
// Copies both the page table and the
// physical memory.
// returns 0 on success, -1 on failure.
// frees any allocated pages on failure.
int
uvmcopy(pagetable_t old, pagetable_t new, uint64 sz)
{
  pte_t *pte;
  uint64 pa, i;
  uint flags;
  char *mem;

  for(i = 0; i < sz; i += PGSIZE){
    if((pte = walk(old, i, 0)) == 0)
      panic("uvmcopy: pte should exist");
    if((*pte & PTE_V) == 0)
      panic("uvmcopy: page not present");
    pa = PTE2PA(*pte);
    flags = PTE_FLAGS(*pte);
    if((mem = kalloc()) == 0)
      goto err;
    memmove(mem, (char*)pa, PGSIZE);
    if(mappages(new, i, PGSIZE, (uint64)mem, flags) != 0){
      kfree(mem);
      goto err;
    }
  }
  return 0;

 err:
  uvmunmap(new, 0, i / PGSIZE, 1);
  return -1;
}

// mark a PTE invalid for user access.
// used by exec for the user stack guard page.
void
uvmclear(pagetable_t pagetable, uint64 va)
{
  pte_t *pte;
  
  pte = walk(pagetable, va, 0);
  if(pte == 0)
    panic("uvmclear");
  *pte &= ~PTE_U;
}

// Copy from kernel to user.
// Copy len bytes from src to virtual address dstva in a given page table.
// Return 0 on success, -1 on error.
int
copyout(pagetable_t pagetable, uint64 dstva, char *src, uint64 len)
{
  uint64 n, va0, pa0;

  while(len > 0){
    va0 = PGROUNDDOWN(dstva);
    pa0 = walkaddr(pagetable, va0);
    if(pa0 == 0)
      return -1;
    n = PGSIZE - (dstva - va0);
    if(n > len)
      n = len;
    memmove((void *)(pa0 + (dstva - va0)), src, n);

    len -= n;
    src += n;
    dstva = va0 + PGSIZE;
  }
  return 0;
}

// Copy from user to kernel.
// Copy len bytes to dst from virtual address srcva in a given page table.
// Return 0 on success, -1 on error.
int
copyin(pagetable_t pagetable, char *dst, uint64 srcva, uint64 len)
{
  uint64 n, va0, pa0;

  while(len > 0){
    va0 = PGROUNDDOWN(srcva);
    pa0 = walkaddr(pagetable, va0);
    if(pa0 == 0)
      return -1;
    n = PGSIZE - (srcva - va0);
    if(n > len)
      n = len;
    memmove(dst, (void *)(pa0 + (srcva - va0)), n);

    len -= n;
    dst += n;
    srcva = va0 + PGSIZE;
  }
  return 0;
  //return copyin_new(pagetable, dst, srcva, len);
}

// Copy a null-terminated string from user to kernel.
// Copy bytes to dst from virtual address srcva in a given page table,
// until a '\0', or max.
// Return 0 on success, -1 on error.
int
copyinstr(pagetable_t pagetable, char *dst, uint64 srcva, uint64 max)
{
  uint64 n, va0, pa0;
  int got_null = 0;

  while(got_null == 0 && max > 0){
    va0 = PGROUNDDOWN(srcva);
    pa0 = walkaddr(pagetable, va0);
    if(pa0 == 0)
      return -1;
    n = PGSIZE - (srcva - va0);
    if(n > max)
      n = max;

    char *p = (char *) (pa0 + (srcva - va0));
    while(n > 0){
      if(*p == '\0'){
        *dst = '\0';
        got_null = 1;
        break;
      } else {
        *dst = *p;
      }
      --n;
      --max;
      p++;
      dst++;
    }

    srcva = va0 + PGSIZE;
  }
  if(got_null){
    return 0;
  } else {
    return -1;
  }
  //return copyin_new(pagetable, dst, srcva, len);
}
