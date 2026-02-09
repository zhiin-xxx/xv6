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

struct kmem{
  struct spinlock lock;
  struct run *freelist;
} kmempool[NCPU];

void
kinit()
{
  for(int i = 0; i < NCPU; i++) {
    initlock(&kmempool[i].lock, "kmem");
    kmempool[i].freelist = 0;
  }
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
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
  push_off();
  int hart = cpuid();
  pop_off();

  acquire(&kmempool[hart].lock);
  r->next = kmempool[hart].freelist;
  kmempool[hart].freelist = r;
  release(&kmempool[hart].lock);

}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  //当自己有内存可以用的时候从自己这里走；否则从别人那里抢内存
  struct run *r;
  push_off();
  int hart = cpuid();
  acquire(&kmempool[hart].lock);
  r = kmempool[hart].freelist;
  if(r)
    kmempool[hart].freelist = r->next;
  else{
    //从其他cpu抢内存
    for(int i = 0; i < NCPU; i++) {
      if(i == hart)
        continue;
      acquire(&kmempool[i].lock);
      r = kmempool[i].freelist;
      if(r) {
        kmempool[i].freelist = r->next;
        release(&kmempool[i].lock);
        break;
      }
      release(&kmempool[i].lock);
    }
  }
  release(&kmempool[hart].lock);  
  pop_off();
  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}
