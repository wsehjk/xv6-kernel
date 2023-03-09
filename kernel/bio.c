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

#define BUCKET_SIZE 31 
struct {
  struct spinlock bucket_lock;  // 保护bucket 信息

  uint blockno;
  uint dev;
  struct buf* buf;  // 记录bucket map的buf
} bucket[BUCKET_SIZE];

struct {
  struct spinlock lock;
  struct buf buf[NBUF];

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  struct buf head;
} bcache;

void
binit(void)
{
  //struct buf *b;

  initlock(&bcache.lock, "bcache");

  for(int i = 0; i < BUCKET_SIZE; ++i) {
    bucket[i].blockno = 0;
    bucket[i].dev = 0;
    bucket[i].buf = 0;
    initlock(&bucket[i].bucket_lock, "bcache");
    snprintf(bucket[i].bucket_lock.name, 1024, "bcache_%d", i);
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  // printf("bget called\n");
  struct buf *b;
  int index = blockno%BUCKET_SIZE;

  for (int j = 0; j < BUCKET_SIZE; ++j) {
    acquire(&(bucket[index].bucket_lock));
    if(bucket[index].blockno == blockno && bucket[index].dev == dev) { // 查看bucket信息是否满足
      bucket[index].buf->refcnt ++;
      b = bucket[index].buf;
      release(&(bucket[index].bucket_lock));  // 释放bucket锁
      acquiresleep(&b->lock); // 获取buf锁
      return b;
    }
    release(&(bucket[index].bucket_lock)); // 释放bucket锁
    index = (index+1)%BUCKET_SIZE; // 查看下一个信息
  }

  // 当前buffer中没有 需要的块，得替换出去一个
  // 记录new block的hashbucket
  // 同时改变old_block的对应的hashbucket

  uint cur_tick = 0;
  acquire(&tickslock);
  cur_tick = ticks;
  release(&tickslock);
  cur_tick ++;
  b = 0;

  uint evicted_blockno = 0; // 记录可驱逐的块号
  uint evicted_dev = 0;

  acquire(&bcache.lock);
  // 遍历所有buf 选择refcnt 为0，并且 atime最小的buffer
  for(int i = 0; i < NBUF; ++i) {
    if (bcache.buf[i].refcnt == 0 && bcache.buf[i].atime < cur_tick) {
      cur_tick = bcache.buf[i].atime;
      b = &bcache.buf[i];
      evicted_blockno = bcache.buf[i].blockno;
      evicted_dev = bcache.buf[i].dev;

      index = evicted_blockno%BUCKET_SIZE;
      for (int j = 0; j < BUCKET_SIZE; ++j) { // 清除old block对应的bucket
        acquire(&(bucket[index].bucket_lock));
        if(bucket[index].blockno == evicted_blockno && bucket[index].dev == evicted_dev) { 
          bucket[index].blockno = 0;
          bucket[index].buf = 0;
          bucket[index].dev = 0;
          release(&(bucket[index].bucket_lock));
          break;
        }
        release(&(bucket[index].bucket_lock));
        index = (index+1)%BUCKET_SIZE; // 查看下一个信息
      }
    }
  }
  release(&bcache.lock);

  if (b) {  // 找到了一个可替换的块
    b->dev = dev;
    b->blockno = blockno;
    b->valid = 0;
    b->refcnt = 1;  // 设置buf信息


    // 设置new block对应的bucket
    index = blockno%BUCKET_SIZE;
    for (int j = 0; j < BUCKET_SIZE; ++j) {
      acquire(&(bucket[index].bucket_lock));
      if (bucket[index].blockno == 0 && bucket[index].dev == 0) {
        bucket[index].blockno = blockno;
        bucket[index].dev = dev;
        bucket[index].buf = b;
        release(&(bucket[index].bucket_lock));
        break;
      }
      release(&(bucket[index].bucket_lock));
      index = (index+1)%BUCKET_SIZE; // 查看下一个信息
    }
    acquiresleep(&b->lock);
    return b;
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
  // printf("brelse called\n");
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  uint tick;
  //acquire(&bcache.lock);  // release&acquire 可以交换顺序吗, 应该可以 
  // 假如先acquire(&bcache.lock), 再release(&b->lock)，持有b->lock并且申请bcache.lock
  // 另一个进程可能已经获取bcache.lock,先释放bcache.lock，申请b->lock，（即实行bget第一个循环）

  b->refcnt--;
  if (b->refcnt == 0) {
    acquire(&tickslock);
    tick = ticks;
    release(&tickslock);
    b->atime = tick;
  }

  //if (b->refcnt == 0) {
  // // no one is waiting for it.
  // b->next->prev = b->prev;
  // b->prev->next = b->next;  // 链接前后节点

  //  b->next = bcache.head.next;
  //  b->prev = &bcache.head;
  //  bcache.head.next->prev = b;
  //  bcache.head.next = b;   // 将buffer放在头部，和init一样
  //}
  
  //release(&bcache.lock);
}

void
bpin(struct buf *b) {
  acquire(&bcache.lock);
  b->refcnt++;
  release(&bcache.lock);
}

void
bunpin(struct buf *b) {
  acquire(&bcache.lock);
  b->refcnt--;
  release(&bcache.lock);
}


