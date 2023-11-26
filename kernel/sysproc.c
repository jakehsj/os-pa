#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"
uint64
sys_exit(void)
{
  int n;
  argint(0, &n);
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
  argaddr(0, &p);
  return wait(p);
}

uint64
sys_sbrk(void)
{
  uint64 addr;
  int n;

  argint(0, &n);
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

  argint(0, &n);
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(killed(myproc())){
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

  argint(0, &pid);
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

// #ifdef SNU
uint64
sys_kcall(void)
{
  int n;

  argint(0, &n);
  switch (n)
  {
    case KC_FREEMEM:    return freemem;
    case KC_USED4K:     return used4k;
    case KC_USED2M:     return used2m;
    case KC_PF:         return pagefaults;
    default:            return -1;
  }
}

uint64
sys_mmap(void)
{
  // PA4: FILL HERE
  void *addr;
  int length, prot, flags;
  argaddr(0, (uint64*)&addr); 
  argint(1, &length); 
  argint(2, &prot); 
  argint(3, &flags);

  if (addr < (void*)PHYSTOP || addr > (void*)(MAXVA-0x10000000) || length < 1){
    // printf("addr: %p\n",addr);
    // printf("length: %d\n",length);
    return 0;
  }
  if (prot != PROT_READ && prot != PROT_WRITE) return 0;
  if (flags != MAP_PRIVATE && flags != MAP_SHARED && flags != (MAP_HUGEPAGE | MAP_PRIVATE) && flags != (MAP_HUGEPAGE | MAP_SHARED)) {
    // printf("mmap bad\n");
    return 0;
  }

  // check if addr alligned
  if ((uint64)addr & (PGSIZE-1)) return 0;
  if ((flags & MAP_HUGEPAGE) && ((uint64)addr & (HUGE_PAGE_SIZE - 1))) return 0;


  struct proc *p = myproc();
  acquire(&p->lock);

  struct mmapped_region *vm = &p->vm[0];

  if(flags & MAP_HUGEPAGE){
    length = PGHGROUNDUP(length);
  } else{
    length = PGROUNDUP(length);
  }

  for(int i=0;i<4;i++){
    if(p->vm[i].valid == 0){
      vm =  &p->vm[i];
    } else{
      if(p->vm[i].addr <= addr && (uint64)(p->vm[i].addr) + length > (uint64)addr){
        // printf("mmap bad\n");
        release(&p->lock);
        return 0;
      }
    }
  }
  // printf("mmap good\n");
  vm->valid = 1;
  vm->addr = addr;
  vm->length = length;
  if(prot == PROT_READ) vm->prot = PTE_R;
  else  vm->prot = PTE_R | PTE_W;
  vm->flags = flags;

  release(&p->lock);
  return (uint64)addr;  

}

uint64
sys_munmap(void)
{
  // PA4: FILL HERE
  // printf("munmap\n");

  void *addr;
  argaddr(0, (uint64 *)&addr);
  if (addr < (void*)PHYSTOP || addr > (void*)(MAXVA-0x10000000)){
    printf("addr: %p\n",addr);
    return -1;
  }
  // check if addr alligned
  if ((uint64)addr & (PGSIZE-1)) return 0;

  struct proc *p = myproc();
  struct mmapped_region *vm = NULL;
  int idx = -1;
  acquire(&p->lock);

  for(int i=0;i<4;i++){
    if(p->vm[i].valid) {
      if(p->vm[i].addr == addr){
        vm = &p->vm[i];
        idx = i;
        break;
      }
    }
  }

  if(vm == NULL) {
    release(&p->lock);
    return -1;
  }
  if ((vm->flags & MAP_HUGEPAGE) && ((uint64)addr & (HUGE_PAGE_SIZE - 1))) {
    release(&p->lock);
    return -1;
  }

  // printf("before unmap_vm\n");
  unmap_vm(idx,p);
  // printf("after unmap_vm\n");
  release(&p->lock);
  return 0;  // 성공 시 0 반환

}
// #endif