// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.


#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

#define NBUCKET 13 
#define hash(id)  id%NBUCKET
struct hashbucket{
  struct spinlock lock;  // 保护bucket 中的buf信息
  struct buf head;  // 每个桶的头结点
} ;

struct {
  struct buf buf[NBUF]; // 30
  struct hashbucket bucket[NBUCKET];
} bcache;

void
binit(void)
{
  struct buf *b;
  char lockname[30];
  for(int i = 0; i < NBUCKET; ++i) {
    snprintf(lockname, 30, "bcache.bucket%d", i);
    initlock(&bcache.bucket[i].lock, lockname);  // 初始化每个锁
    bcache.bucket[i].head.next = &bcache.bucket[i].head;
    bcache.bucket[i].head.prev = &bcache.bucket[i].head;
  }

  for(int i = 0; i < NBUF; ++i) {
    b = &bcache.buf[i];
    b->next = bcache.bucket[0].head.next;
    b->prev = &bcache.bucket[0].head; 
    initsleeplock(&bcache.buf[i].lock, "buflock");
    bcache.bucket[0].head.next->prev = b;
    bcache.bucket[0].head.next = b;
  }

}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf* b;
  int id = hash(blockno);

  b = 0;
  acquire(&tickslock);
  uint cur_tick = ticks;
  release(&tickslock);
  struct buf* t = 0;

  uint cmp_tick = cur_tick;
  acquire(&bcache.bucket[id].lock); // 关了中断

  for(b = bcache.bucket[id].head.next; b != &bcache.bucket[id].head; b = b->next) {
    if (b->dev == dev && b->blockno == blockno) {  // 找到buffer中block
      b->refcnt ++;
      release(&bcache.bucket[id].lock); // 释放锁
      acquiresleep(&b->lock);  // 获取buf的锁
      return b;
    }
    if (b->refcnt == 0 && b->atime < cmp_tick) {  // 同时查找可能的替换block
      cmp_tick = b->atime;
      t = b;
    }
  }

  // not cached
  // 查找到第一个存在refcnt为0的散列表
  // 持有bache.bucket[id].lock
  if (t) { // 在相同的bucket中找到了 可替换bucket
    t->blockno = blockno;
    t->dev = dev;
    t->refcnt = 1;
    t->valid = 0;
    release(&bcache.bucket[id].lock);
    acquiresleep(&t->lock);
    return t;
  }
  // 持有bache.bucket[id].lock
  for (int targetid = hash(id+1); targetid != id; targetid = hash(targetid+1)) {
    b = 0;
    cmp_tick = cur_tick;
    acquire(&bcache.bucket[targetid].lock);
    for (t = bcache.bucket[targetid].head.next; t != &bcache.bucket[targetid].head; t = t->next) {
      if (t->refcnt == 0 && t->atime < cmp_tick) {
        cmp_tick = t->atime;
        b = t;
      }
    } 

    if (b) { // 在另一个bucket中找到了 可替换bucket
      b->blockno = blockno;
      b->dev = dev;
      b->refcnt = 1;
      b->valid = 0;

      b->next->prev = b->prev;
      b->prev->next = b->next; // 前后节点连接

      b->next = bcache.bucket[id].head.next;
      b->prev = &bcache.bucket[id].head;
      bcache.bucket[id].head.next->prev = b;
      bcache.bucket[id].head.next = b;   // 将b加入bucket[id]中

      release(&bcache.bucket[targetid].lock);
      release(&bcache.bucket[id].lock);
      acquiresleep(&b->lock);
      return b;
    }
    release(&bcache.bucket[targetid].lock); // 当前bucket没有，释放锁
  }
  panic("bget: no buffers");
}

// Return a locked buf with the contents of the indicated block.
// caller 可以唯一地持有锁
struct buf*
bread(uint dev, uint blockno)
{
  //printf("bread called\n");
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  // printf("bread return\n");
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{

  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  //acquire(&bcache.lock);  // release&acquire 可以交换顺序吗, 应该可以 
  // 假如先acquire(&bcache.lock), 再release(&b->lock)，持有b->lock并且申请bcache.lock
  // 另一个进程可能已经获取bcache.lock,先释放bcache.lock，申请b->lock，（即实行bget第一个循环）
  int id = hash(b->blockno);
  acquire(&bcache.bucket[id].lock);
  b->refcnt--;
  if (b->refcnt == 0) {
    acquire(&tickslock);
    b->atime = ticks;
    release(&tickslock);
  }
  release(&bcache.bucket[id].lock);
}

void
bpin(struct buf *b) {
  int id = hash(b->blockno);
  acquire(&bcache.bucket[id].lock);
  b->refcnt++;
  release(&bcache.bucket[id].lock);
}

void
bunpin(struct buf *b) {
  int id = hash(b->blockno);
  acquire(&bcache.bucket[id].lock);
  b->refcnt--;
  release(&bcache.bucket[id].lock);
}

