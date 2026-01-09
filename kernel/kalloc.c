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
} kmem;

//物理页面引用计数
#define REFpages ((PHYSTOP-KERNBASE)/PGSIZE)
struct mem_ref
{
  struct spinlock lock;
  int cnt;
};
struct mem_ref mem_ref[REFpages];

void
kinit()
{
  for(int i=0;i<REFpages;i++){
    initlock(&mem_ref[i].lock,"mem_ref");
    mem_ref[i].cnt=0;
  }
  initlock(&kmem.lock, "kmem");
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

  uint page_index = ((uint64)pa - KERNBASE) / PGSIZE;
  acquire(&mem_ref[page_index].lock);
  if(mem_ref[page_index].cnt > 0){
    mem_ref[page_index].cnt--;
  }
  int cnt = mem_ref[page_index].cnt;
  release(&mem_ref[page_index].lock);
  if(cnt > 0){
    return;
  }

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;
  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
  release(&kmem.lock);

}

void
kaddref(void *pa)
{

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kaddref");
  uint page_index = ((uint64)pa - KERNBASE) / PGSIZE;
  acquire(&mem_ref[page_index].lock);
  mem_ref[page_index].cnt++;
  release(&mem_ref[page_index].lock);
}
// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r){
    kmem.freelist = r->next;
  }
  release(&kmem.lock);

  if(r){
    memset((char*)r, 5, PGSIZE); // fill with junk
    kaddref((void*)r);
  }
  return (void*)r;
}
