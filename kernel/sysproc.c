#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

uint64
sys_exit(void)
{
  int n;
  if(argint(0, &n) < 0)
    return -1;
  exit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  if(argaddr(0, &p) < 0)
    return -1;
  return wait(p);
}

uint64
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;

  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(myproc()->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

uint64
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}
// void *mmap(void *addr, size_t length, int prot, int flags,
//                   int fd, off_t offset);
//        int munmap(void *addr, size_t length);
#include "fcntl.h"
#include "sleeplock.h"
#include "fs.h"
#include "file.h"
uint64
sys_mmap(void)
{
  uint64 addr;
  int length, prot, flags, fd;
  uint64 offset;
  if(argaddr(0, &addr) < 0 || argint(1, &length) < 0 || argint(2, &prot) < 0 
    || argint(3, &flags) < 0 || argint(4, &fd) < 0 || argaddr(5, &offset) < 0)
    return -1;
  if((prot & PROT_WRITE)&& (flags & MAP_SHARED)) {
    struct file* f = myproc()->ofile[fd];
    if(f->writable == 0){
      printf("sys_mmap: fd %d not writable but MAP_SHARED requested\n", fd);
      return -1;
    }
    
  }
  for(int i = 0; i < 16; i++) {
    struct vma *vma = &myproc()->vmas[i];
    if(!vma->used) {
      vma->start = MMAPBEGIN-myproc()->sz_mapped - length;
      vma->length = length;
      vma->prot = prot;
      vma->flags = flags;
      vma->file = myproc()->ofile[fd];
      filedup(myproc()->ofile[fd]);
      vma->offset = offset;
      vma->used = 1;
      myproc()->sz_mapped += length;
      printf("sys_mmap: mapped file %d to vma %d, start=%p, length=%d---pid:%d\n", fd, i, vma->start, length, myproc()->pid);
      return vma->start;
    }
  }
  panic("sys_mmap: no free vma");
  return -1;
}
extern pte_t *
walk(pagetable_t pagetable, uint64 va, int alloc);
uint64
sys_munmap(void)
{
  uint64 addr;
  int length;
  if(argaddr(0, &addr) < 0 || argint(1, &length) < 0)
    return -1;
  uint64 a = PGROUNDDOWN(addr);

  struct vma *vma=0;
   for(int i = 0; i < 16; i++) {
    vma = &myproc()->vmas[i];
    if(vma->used && vma->start <= addr && addr < vma->start + vma->length) {
      if((vma->flags & MAP_SHARED) &&(vma->prot & PROT_WRITE)) {
        filewrite(vma->file, addr, length);
      }
      vma->mapcnt -= length/PGSIZE;
      if(vma->mapcnt==0){
          vma->used = 0;
          vma->mapcnt =0;
          fileclose(vma->file);
          myproc()->sz_mapped -= vma->length;
      }
      uvmunmap(myproc()->pagetable, a, length/PGSIZE, 1);
      vma->validaddr=vma->start + length;
      printf("sys_munmap: unmapped vma %d, start=%p, length=%d\n", i, vma->start, length);
      break;
    }
  }
  
  return 0;
}