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
#define NUM 13
struct {
  struct buf buf[NBUF];

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  struct spinlock lock[NUM];
  struct buf head[NUM];
} bcache;

void
binit(void)
{
  struct buf *b;
  char name[16];
  for(int i=0;i<NUM;i++){
    snprintf(name, sizeof(name), "bcache%d", i);
    initlock(&bcache.lock[i], name);
    bcache.head[i].prev = &bcache.head[i];
    bcache.head[i].next = &bcache.head[i];
  }
  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    b->next = bcache.head[0].next;
    b->prev = &bcache.head[0];
    initsleeplock(&b->lock, "buffer");
    bcache.head[0].next->prev = b;
    bcache.head[0].next = b;
    b->time=0;
  }
}
extern uint ticks;
// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;
  int idx=blockno%NUM;
  acquire(&bcache.lock[idx]);

  // Is the block already cached?
  for(b = bcache.head[idx].next; b != &bcache.head[idx]; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      b->time=ticks;
      release(&bcache.lock[idx]);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  uint lasttime=0xffffffff;
  int found=0;
  struct buf* target=0;
  for(b = bcache.head[idx].prev; b != &bcache.head[idx]; b = b->prev){
    if(b->refcnt == 0 && b->time<lasttime) {
      lasttime=b->time;
      found=1;
      target=b;
    }
  }
  //b就是最早的
  if(found) {
    b=target;
    b->dev = dev;
    b->blockno = blockno;
    b->valid = 0;
    b->refcnt = 1;
    b->time=ticks;
    release(&bcache.lock[idx]);
    acquiresleep(&b->lock);
    return b;
  }

  //没找到
  for(int i=0;i<NUM;i++){
    if(i==idx) continue;
    acquire(&bcache.lock[i]);
    lasttime=0xffffffff;
    found=0;
    target=0;
    for(b = bcache.head[i].prev; b != &bcache.head[i]; b = b->prev){
      if(b->refcnt == 0 && b->time<lasttime) {
        lasttime=b->time;
        found=1;
        target=b;
      }
    }
    //b就是最早的
    if(found) {
      b=target;
      b->dev = dev;
      b->blockno = blockno;
      b->valid = 0;
      b->refcnt = 1;
      b->time=ticks;
      b->next->prev = b->prev;
      b->prev->next = b->next;
      //插入idx链表头部
      b->next = bcache.head[idx].next;
      b->prev = &bcache.head[idx];
      bcache.head[idx].next->prev = b;
      bcache.head[idx].next = b;
      release(&bcache.lock[i]);
      release(&bcache.lock[idx]);
      acquiresleep(&b->lock);
      return b;
    }
    release(&bcache.lock[i]);
  }
  release(&bcache.lock[idx]);

  panic("bget: no buffers");
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
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

  b->refcnt--;
}

void
bpin(struct buf *b) {
  int idx=b->blockno%NUM;
  acquire(&bcache.lock[idx]);
  b->refcnt++;
  release(&bcache.lock[idx]);
}

void
bunpin(struct buf *b) {
  int idx=b->blockno%NUM;
  acquire(&bcache.lock[idx]);
  b->refcnt--;
  release(&bcache.lock[idx]);
}


