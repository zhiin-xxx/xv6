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
#define NBUKCET 13
struct {
  struct spinlock lock;
  struct buf buf[NBUF];
  struct buf bucket[NBUKCET]; // 哈希桶头节点数组
  struct spinlock block[NBUKCET]; // 每个哈希桶的锁
} bcache;

void
binit(void)
{
  struct buf *b;
  // char name[32];
  initlock(&bcache.lock, "bcache");
  for(int i=0;i<NBUKCET;i++) {
    // snprintf(name, sizeof(name), "bcache_bucket_lock_%d", i); // 锁命名
    initlock(&bcache.block[i], "bcache_bucket");
    // Create linked list of buffers
    bcache.bucket[i].prev = &bcache.bucket[i];
    bcache.bucket[i].next = &bcache.bucket[i];
    bcache.buf[i].stamp=0;
  }
  //刚开始所有的buf都分配给bucket0
  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
      b->next = bcache.bucket[0].next;
      b->prev = &bcache.bucket[0];
      initsleeplock(&b->lock, "buffer");
      bcache.bucket[0].next->prev = b;
      bcache.bucket[0].next = b;
    }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;
  int idx = blockno % NBUKCET;
  acquire(&bcache.block[idx]);

  // Is the block already cached?
  for(b = bcache.bucket[idx].next; b != &bcache.bucket[idx]; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.block[idx]);
      acquiresleep(&b->lock);
      // printf("bget(has been cached): blockno=%d idx=%d------&b:%p\n", blockno, idx, b);
      return b;
    }
  }

  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  //1.还有空间
  for(b = bcache.bucket[idx].prev; b != &bcache.bucket[idx]; b = b->prev){
    if(b->refcnt == 0) {
      b->dev = dev;
      b->blockno = blockno;
      b->valid = 0;
      b->refcnt = 1;
      release(&bcache.block[idx]);
      acquiresleep(&b->lock);
      // printf("bget(not cached): blockno=%d idx=%d------&b:%p\n", blockno, idx, b);
      return b;
    }
  }
  //2.没有空间，直接从其他桶里淘汰
  for(int i=0;i<NBUKCET;i++) {
    if(i == idx) continue;//避免重复访问自身
    acquire(&bcache.block[i]);
    for(b = bcache.bucket[i].prev; b != &bcache.bucket[i]; b = b->prev){
      if(b->refcnt == 0) {
        //从桶i中删除b
        b->prev->next = b->next;
        b->next->prev = b->prev;
        //加入到桶idx中
        b->next = bcache.bucket[idx].next;
        b->prev = &bcache.bucket[idx];
        bcache.bucket[idx].next->prev = b;
        bcache.bucket[idx].next = b;

        b->dev = dev;
        b->blockno = blockno;
        b->valid = 0;
        b->refcnt = 1;
        release(&bcache.block[i]);
        release(&bcache.block[idx]);
        acquiresleep(&b->lock);
        // printf("bget(evict from %d to %d): blockno=%d idx=%d------&b:%p\n", i, idx, blockno, idx, b);
        return b;
      }
    }
    release(&bcache.block[i]);
  }
  panic("bget: no buffers");
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  b->stamp=ticks; // 更新使用时间戳
  if(!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
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
  bunpin(b);
}

void
bpin(struct buf *b) {
  int idx = b->blockno % NBUKCET;

  acquire(&bcache.block[idx]);
  b->refcnt++;
  release(&bcache.block[idx]);
}

void
bunpin(struct buf *b) {
  int idx = b->blockno % NBUKCET;
  acquire(&bcache.block[idx]);
  b->refcnt--;
  release(&bcache.block[idx]);
}


