//
// File-system system calls.
// Mostly argument checking, since we don't trust
// user code, and calls into file.c and fs.c.
//

#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "stat.h"
#include "spinlock.h"
#include "proc.h"
#include "fs.h"
#include "sleeplock.h"
#include "file.h"
#include "fcntl.h"

// Fetch the nth word-sized system call argument as a file descriptor
// and return both the descriptor and the corresponding struct file.
static int
argfd(int n, int *pfd, struct file **pf)
{
  int fd;
  struct file *f;

  if(argint(n, &fd) < 0)
    return -1;
  if(fd < 0 || fd >= NOFILE || (f=myproc()->ofile[fd]) == 0)
    return -1;
  if(pfd)
    *pfd = fd;
  if(pf)
    *pf = f;
  return 0;
}

// Allocate a file descriptor for the given file.
// Takes over file reference from caller on success.
static int
fdalloc(struct file *f)
{
  int fd;
  struct proc *p = myproc();

  for(fd = 0; fd < NOFILE; fd++){
    if(p->ofile[fd] == 0){
      p->ofile[fd] = f;
      return fd;
    }
  }
  return -1;
}

uint64
sys_dup(void)
{
  struct file *f;
  int fd;

  if(argfd(0, 0, &f) < 0)
    return -1;
  if((fd=fdalloc(f)) < 0)
    return -1;
  filedup(f);
  return fd;
}

uint64
sys_read(void)
{
  struct file *f;
  int n;
  uint64 p;

  if(argfd(0, 0, &f) < 0 || argint(2, &n) < 0 || argaddr(1, &p) < 0)
    return -1;
  return fileread(f, p, n);
}

uint64
sys_write(void)
{
  struct file *f;
  int n;
  uint64 p;

  if(argfd(0, 0, &f) < 0 || argint(2, &n) < 0 || argaddr(1, &p) < 0)
    return -1;

  return filewrite(f, p, n);
}

uint64
sys_close(void)
{
  int fd;
  struct file *f;

  if(argfd(0, &fd, &f) < 0)
    return -1;
  myproc()->ofile[fd] = 0;
  fileclose(f);
  return 0;
}

uint64
sys_fstat(void)
{
  struct file *f;
  uint64 st; // user pointer to struct stat

  if(argfd(0, 0, &f) < 0 || argaddr(1, &st) < 0)
    return -1;
  return filestat(f, st);
}

// Create the path new as a link to the same inode as old.
uint64
sys_link(void)
{
  char name[DIRSIZ], new[MAXPATH], old[MAXPATH];
  struct inode *dp, *ip;

  if(argstr(0, old, MAXPATH) < 0 || argstr(1, new, MAXPATH) < 0)
    return -1;

  begin_op();
  if((ip = namei(old)) == 0){
    end_op();
    return -1;
  }

  ilock(ip);
  if(ip->type == T_DIR){
    iunlockput(ip);
    end_op();
    return -1;
  }

  ip->nlink++;
  iupdate(ip);
  iunlock(ip);

  if((dp = nameiparent(new, name)) == 0)
    goto bad;
  ilock(dp);
  if(dp->dev != ip->dev || dirlink(dp, name, ip->inum) < 0){
    iunlockput(dp);
    goto bad;
  }
  iunlockput(dp);
  iput(ip);

  end_op();

  return 0;

bad:
  ilock(ip);
  ip->nlink--;
  iupdate(ip);
  iunlockput(ip);
  end_op();
  return -1;
}

// Is the directory dp empty except for "." and ".." ?
static int
isdirempty(struct inode *dp)
{
  int off;
  struct dirent de;

  for(off=2*sizeof(de); off<dp->size; off+=sizeof(de)){
    if(readi(dp, 0, (uint64)&de, off, sizeof(de)) != sizeof(de))
      panic("isdirempty: readi");
    if(de.inum != 0)
      return 0;
  }
  return 1;
}

uint64
sys_unlink(void)
{
  struct inode *ip, *dp;
  struct dirent de;
  char name[DIRSIZ], path[MAXPATH];
  uint off;

  if(argstr(0, path, MAXPATH) < 0)
    return -1;

  begin_op();
  if((dp = nameiparent(path, name)) == 0){
    end_op();
    return -1;
  }

  ilock(dp);

  // Cannot unlink "." or "..".
  if(namecmp(name, ".") == 0 || namecmp(name, "..") == 0)
    goto bad;

  if((ip = dirlookup(dp, name, &off)) == 0)
    goto bad;
  ilock(ip);

  if(ip->nlink < 1)
    panic("unlink: nlink < 1");
  if(ip->type == T_DIR && !isdirempty(ip)){
    iunlockput(ip);
    goto bad;
  }

  memset(&de, 0, sizeof(de));
  if(writei(dp, 0, (uint64)&de, off, sizeof(de)) != sizeof(de))
    panic("unlink: writei");
  if(ip->type == T_DIR){
    dp->nlink--;
    iupdate(dp);
  }
  iunlockput(dp);

  ip->nlink--;
  iupdate(ip);
  iunlockput(ip);

  end_op();

  return 0;

bad:
  iunlockput(dp);
  end_op();
  return -1;
}

static struct inode*
create(char *path, short type, short major, short minor)
{
  struct inode *ip, *dp;
  char name[DIRSIZ];

  if((dp = nameiparent(path, name)) == 0)
    return 0;

  ilock(dp);

  if((ip = dirlookup(dp, name, 0)) != 0){
    iunlockput(dp);
    ilock(ip);
    if(type == T_FILE && (ip->type == T_FILE || ip->type == T_DEVICE))
      return ip;
    iunlockput(ip);
    return 0;
  }

  if((ip = ialloc(dp->dev, type)) == 0)
    panic("create: ialloc");

  ilock(ip);
  ip->major = major;
  ip->minor = minor;
  ip->nlink = 1;
  iupdate(ip);

  if(type == T_DIR){  // Create . and .. entries.
    dp->nlink++;  // for ".."
    iupdate(dp);
    // No ip->nlink++ for ".": avoid cyclic ref count.
    if(dirlink(ip, ".", ip->inum) < 0 || dirlink(ip, "..", dp->inum) < 0)
      panic("create dots");
  }

  if(dirlink(dp, name, ip->inum) < 0)
    panic("create: dirlink");

  iunlockput(dp);

  return ip;
}

uint64
sys_open(void)
{
  char path[MAXPATH];
  int fd, omode;
  struct file *f;
  struct inode *ip;
  int n;

  if((n = argstr(0, path, MAXPATH)) < 0 || argint(1, &omode) < 0)
    return -1;

  begin_op();

  if(omode & O_CREATE){
    ip = create(path, T_FILE, 0, 0);
    if(ip == 0){
      end_op();
      return -1;
    }
  } else {
    if((ip = namei(path)) == 0){
      end_op();
      return -1;
    }
    ilock(ip);
    if(ip->type == T_DIR && omode != O_RDONLY){
      iunlockput(ip);
      end_op();
      return -1;
    }
  }

  if(ip->type == T_DEVICE && (ip->major < 0 || ip->major >= NDEV)){
    iunlockput(ip);
    end_op();
    return -1;
  }

  if((f = filealloc()) == 0 || (fd = fdalloc(f)) < 0){
    if(f)
      fileclose(f);
    iunlockput(ip);
    end_op();
    return -1;
  }

  if(ip->type == T_DEVICE){
    f->type = FD_DEVICE;
    f->major = ip->major;
  } else {
    f->type = FD_INODE;
    f->off = 0;
  }
  f->ip = ip;
  f->readable = !(omode & O_WRONLY);
  f->writable = (omode & O_WRONLY) || (omode & O_RDWR);

  if((omode & O_TRUNC) && ip->type == T_FILE){
    itrunc(ip);
  }

  iunlock(ip);
  end_op();

  return fd;
}

uint64
sys_mkdir(void)
{
  char path[MAXPATH];
  struct inode *ip;

  begin_op();
  if(argstr(0, path, MAXPATH) < 0 || (ip = create(path, T_DIR, 0, 0)) == 0){
    end_op();
    return -1;
  }
  iunlockput(ip);
  end_op();
  return 0;
}

uint64
sys_mknod(void)
{
  struct inode *ip;
  char path[MAXPATH];
  int major, minor;

  begin_op();
  if((argstr(0, path, MAXPATH)) < 0 ||
     argint(1, &major) < 0 ||
     argint(2, &minor) < 0 ||
     (ip = create(path, T_DEVICE, major, minor)) == 0){
    end_op();
    return -1;
  }
  iunlockput(ip);
  end_op();
  return 0;
}

uint64
sys_chdir(void)
{
  char path[MAXPATH];
  struct inode *ip;
  struct proc *p = myproc();
  
  begin_op();
  if(argstr(0, path, MAXPATH) < 0 || (ip = namei(path)) == 0){
    end_op();
    return -1;
  }
  ilock(ip);
  if(ip->type != T_DIR){
    iunlockput(ip);
    end_op();
    return -1;
  }
  iunlock(ip);
  iput(p->cwd);
  end_op();
  p->cwd = ip;
  return 0;
}

uint64
sys_exec(void)
{
  char path[MAXPATH], *argv[MAXARG];
  int i;
  uint64 uargv, uarg;

  if(argstr(0, path, MAXPATH) < 0 || argaddr(1, &uargv) < 0){
    return -1;
  }
  memset(argv, 0, sizeof(argv));
  for(i=0;; i++){
    if(i >= NELEM(argv)){
      goto bad;
    }
    if(fetchaddr(uargv+sizeof(uint64)*i, (uint64*)&uarg) < 0){
      goto bad;
    }
    if(uarg == 0){
      argv[i] = 0;
      break;
    }
    argv[i] = kalloc();
    if(argv[i] == 0)
      goto bad;
    if(fetchstr(uarg, argv[i], PGSIZE) < 0)
      goto bad;
  }

  int ret = exec(path, argv);

  for(i = 0; i < NELEM(argv) && argv[i] != 0; i++)
    kfree(argv[i]);

  return ret;

 bad:
  for(i = 0; i < NELEM(argv) && argv[i] != 0; i++)
    kfree(argv[i]);
  return -1;
}

uint64
sys_pipe(void)
{
  uint64 fdarray; // user pointer to array of two integers
  struct file *rf, *wf;
  int fd0, fd1;
  struct proc *p = myproc();

  if(argaddr(0, &fdarray) < 0)
    return -1;
  if(pipealloc(&rf, &wf) < 0)
    return -1;
  fd0 = -1;
  if((fd0 = fdalloc(rf)) < 0 || (fd1 = fdalloc(wf)) < 0){
    if(fd0 >= 0)
      p->ofile[fd0] = 0;
    fileclose(rf);
    fileclose(wf);
    return -1;
  }
  if(copyout(p->pagetable, fdarray, (char*)&fd0, sizeof(fd0)) < 0 ||
     copyout(p->pagetable, fdarray+sizeof(fd0), (char *)&fd1, sizeof(fd1)) < 0){
    p->ofile[fd0] = 0;
    p->ofile[fd1] = 0;
    fileclose(rf);
    fileclose(wf);
    return -1;
  }
  return 0;
}
struct VMA* vma_alloc()
{
  struct proc* proc = myproc();
  struct VMA* vma;
  for(vma = proc->vma; vma != proc->vma + NVMA; vma ++) {
    if (vma->addr == 0) 
      return vma;
  }
  return 0;
};

// void *mmap(void *addr, uint64 length, uint64 prot, uint64 flags, int fd, uint64 offset);
uint64 sys_mmap(void) {
  uint64 addr;
  uint64 length;
  int prot, flags, fd;
  uint64 offset;
  struct file* file;
  
  argaddr(0, &addr);
  if (addr != 0)   // assume addr is 0 
    return -1;

  argaddr(1, &length);
  if (length%PGSIZE)   // 假设划分的长度page aligned
    return -1;

  argint(2, &prot);
  argint(3, &flags);
  argfd(4, &fd, &file);  
  argaddr(5, &offset);

  // if file opend read_only, 
  if (file->writable == 0 && prot&PROT_WRITE && flags == MAP_SHARED) {
    return -1;
  }
  struct proc* proc = myproc();
  struct VMA* vma = vma_alloc();
  if (vma == 0)  {
    return -1;
  }

  vma->file = filedup(file);  // 记录信息
  vma->flags = flags;
  vma->offset = offset;
  vma->prot = prot;
  vma->addr = proc->sz;
  vma->length = length;
  proc->sz += length;
  return vma->addr;
}

struct VMA* findvma(uint64 va) {
  struct proc *p = myproc();
  struct VMA* vma;
  for(vma = p->vma; vma < p->vma + NVMA; ++vma) {
    if (vma->addr == 0) 
      continue;
    if (vma->addr <= va && va < vma->addr+vma->length) 
      return vma;
  }
  return 0;
}

// int munmap(char*, uint64 length);
uint64 sys_munmap(void) {  // 注意改变vma->addr, vma->length, vma->offset

  uint64 unmap_addr;
  uint64 unmap_length;
  argaddr(0, &unmap_addr);
  argaddr(1, &unmap_length);
  uint64 unmap_end = unmap_length + unmap_addr;
  struct VMA* vma = findvma(unmap_addr);
  if (vma == 0)
    return -1;
  
  uint64 start_va = vma->addr;
  uint64 tot_length = vma->length;
  uint64 region_end = start_va + tot_length;
  struct proc* p = myproc();

  // unmap的区域要么在开始的连续区域，要么在结束的连续区域
  if (start_va == unmap_addr) {
    if (unmap_length > tot_length) 
      return -1;
  } else {
    if (unmap_end != region_end)
     return -1;
  }
  uint64 addr = unmap_addr;
  if (vma->flags == MAP_SHARED) {
    if (start_va == unmap_addr) {
      while (unmap_addr != unmap_end) {
        uint64 next;
        uint n;
        if (unmap_addr%PGSIZE) {  // 当前地址 不是page 对齐
          next = PGROUNDUP(unmap_addr);
        } else {
          next = unmap_addr+PGSIZE;
        }
        if (next > unmap_end)
          next = unmap_end;
        n = next - unmap_addr;
        uint64 pa = walkaddr(p->pagetable, unmap_addr);
        if (pa) {
          filewrite(vma->file, unmap_addr, n);
        }
        unmap_addr += n;
      }
    } else {
      while (unmap_end != unmap_addr) {
        uint64 next;
        uint n;
        if (unmap_end%PGSIZE) {
          next = PGROUNDUP(unmap_end);
        } else {
          next = unmap_end - PGSIZE;
        }
        if (next < unmap_addr)
          next = unmap_addr;
        n = unmap_end - next;
        uint64 pa = walkaddr(p->pagetable, next);
        if (pa) {
          filewrite(vma->file, next, n);
        }
        unmap_end -= n;
      }  
    }
  }

  unmap_addr = addr;
  uvmunmap(p->pagetable, unmap_addr, unmap_length/PGSIZE, 1);

  if (unmap_addr == start_va) {
    start_va = unmap_addr + unmap_length;
    vma->offset += ((unmap_length)/PGSIZE)*PGSIZE;
  } else {
    region_end = unmap_addr;
  }
  vma->addr = start_va;
  vma->length = region_end - start_va;
  if (vma->length == 0) {  // 整个区域都被unmap了
    fileclose(vma->file);
    vma->addr = 0;
    memset(vma, 0, sizeof(*vma));
  }
  return 0;
}

/*
#define PROT_READ       0x1
#define PROT_WRITE      0x2
#define PROT_EXEC       0x4
*/
int page_fault_handler(struct proc* p) {
  uint64 ustack = PGROUNDDOWN(p->trapframe->sp);
  uint64 fault_va = r_stval();
  if (fault_va < ustack) {
    printf("page_fault_handler: fault_va < ustatck\n");
    return -1;
  }  // stack overflow 
  // maybe lazily growed heap, or lazily mmaped region
  struct VMA* vma = findvma(fault_va); 
  if (vma == 0) {  // lazily growed heap
    return -1;
  } 
  uint64 mem = (uint64)kalloc();  // lazily mmaped region
  if (mem == 0) {
    return -1;
  } 

  uint npages = (fault_va - vma->addr)/PGSIZE;
  uint64 start_offset = vma->offset + npages*PGSIZE; // file read start point
  int perm = PTE_U;
  if (vma->prot & PROT_READ) 
    perm |= PTE_R;
  if (vma->prot & PROT_WRITE)
    perm |= PTE_W;
  if (vma->prot & PROT_EXEC)
    perm |= PTE_X;  
  
  if (mappages(p->pagetable, fault_va, PGSIZE, mem, perm) < 0) {  // 映射内存
    kfree((void*)mem);
    return -1;
  }

  struct inode* ip = vma->file->ip;
  ilock(ip);
  if (readi(ip, 0, mem, start_offset, PGSIZE) < 0) {
    iunlock(ip);
    uvmunmap(p->pagetable, fault_va, 1, 1);
    return -1;
  }  // 读一个page
  iunlock(ip);
  return 0;
}