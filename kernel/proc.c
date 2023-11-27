#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"

struct cpu cpus[NCPU];

struct proc proc[NPROC];

struct proc *initproc;

int nextpid = 1;
struct spinlock pid_lock;

// #ifdef SNU
int pagefaults;
extern int bprefcount[(PHYSTOP-KERNBASE) >> PGSHIFT];
extern int hgrefcount[(PHYSTOP-KERNBASE) >> 21];

pte_t *
walk_huge(pagetable_t pagetable, uint64 va, int alloc)
{
  if(va >= MAXVA)
    panic("walk");

  for(int level = 2; level > 1; level--) {
    pte_t *pte = &pagetable[PX(level, va)];
    if(*pte & PTE_V) {
      pagetable = (pagetable_t)PTE2PA(*pte);
    } else {
      if(!alloc || (pagetable = (pde_t*)kalloc()) == 0)
        return 0;
      memset(pagetable, 0, PGSIZE);
      *pte = PA2PTE(pagetable) | PTE_V;
    }
  }
  return &pagetable[PX(1, va)];
}

int 
maphugepages(pagetable_t pagetable, uint64 va, uint64 size, uint64 pa, int perm)
{
  uint64 a, last;
  pte_t *pte;
  if(size == 0)
    panic("mappages: size");
  // printf("va: %p\n",va);
  a = PGHGROUNDDOWN(va);
  last = PGHGROUNDDOWN(va + size - 1);
  // printf("maphugepages pa: %p\n", pa);
  for(;;){
    // printf("a: %x\n",a);
    // printf("last: %x\n",last);
    if((pte = walk_huge(pagetable, a, 1)) == 0)
      return -1;
    // printf("walking done\n");
    if(*pte & PTE_V)
      panic("maphugepages: remap");
    *pte = PA2PTE(pa) | perm | PTE_V;
    if(a == last)
      break;
    a += HUGE_PAGE_SIZE;
    pa += HUGE_PAGE_SIZE;
  }
  return 0;
}


void unmap_vm(int idx, struct proc *p){

  struct mmapped_region *vm = &p->vm[idx];
  if(!vm->valid) return;
  void *addr = vm->addr;

  if(vm->flags & MAP_HUGEPAGE){
    pte_t *pte;
    for(uint64 i = (uint64)addr; i < (uint64)addr + vm->length; i+=HUGE_PAGE_SIZE){
      if((pte = walk_huge(p->pagetable, i, 0))) {
        if(*pte & PTE_V){
          uint64 pa = PTE2PA(*pte);
          *pte = 0;
          kfree_huge((void*)pa);
        }
      }
    }
    // p->vm[idx].valid = 0;
  }

  else{
    pte_t *pte;
    for(uint64 i = (uint64)addr; i < (uint64)addr + vm->length; i+=PGSIZE){
      if((pte = walk(p->pagetable, i, 0))) {
        // printf("freeing \n");
        if(*pte & PTE_V){
          uint64 pa = PTE2PA(*pte);
          *pte = 0;
          kfree((void*)pa);
        }
      }
    }
    // p->vm[idx].valid = 0;
  }
}

// #endif

extern void forkret(void);
static void freeproc(struct proc *p);

extern char trampoline[]; // trampoline.S

// helps ensure that wakeups of wait()ing
// parents are not lost. helps obey the
// memory model when using p->parent.
// must be acquired before any p->lock.
struct spinlock wait_lock;

// Allocate a page for each process's kernel stack.
// Map it high in memory, followed by an invalid
// guard page.
void
proc_mapstacks(pagetable_t kpgtbl)
{
  struct proc *p;
  
  for(p = proc; p < &proc[NPROC]; p++) {
    char *pa = kalloc();
    if(pa == 0)
      panic("kalloc");
    uint64 va = KSTACK((int) (p - proc));
    kvmmap(kpgtbl, va, (uint64)pa, PGSIZE, PTE_R | PTE_W);
  }
}

// initialize the proc table.
void
procinit(void)
{
  struct proc *p;
  
  initlock(&pid_lock, "nextpid");
  initlock(&wait_lock, "wait_lock");
  for(p = proc; p < &proc[NPROC]; p++) {
      initlock(&p->lock, "proc");
      p->state = UNUSED;
      p->kstack = KSTACK((int) (p - proc));
  }
}

// Must be called with interrupts disabled,
// to prevent race with process being moved
// to a different CPU.
int
cpuid()
{
  int id = r_tp();
  return id;
}

// Return this CPU's cpu struct.
// Interrupts must be disabled.
struct cpu*
mycpu(void)
{
  int id = cpuid();
  struct cpu *c = &cpus[id];
  return c;
}

// Return the current struct proc *, or zero if none.
struct proc*
myproc(void)
{
  push_off();
  struct cpu *c = mycpu();
  struct proc *p = c->proc;
  pop_off();
  return p;
}

int
allocpid()
{
  int pid;
  
  acquire(&pid_lock);
  pid = nextpid;
  nextpid = nextpid + 1;
  release(&pid_lock);

  return pid;
}

// Look in the process table for an UNUSED proc.
// If found, initialize state required to run in the kernel,
// and return with p->lock held.
// If there are no free procs, or a memory allocation fails, return 0.
static struct proc*
allocproc(void)
{
  struct proc *p;

  for(p = proc; p < &proc[NPROC]; p++) {
    acquire(&p->lock);
    if(p->state == UNUSED) {
      goto found;
    } else {
      release(&p->lock);
    }
  }
  return 0;

found:
  p->pid = allocpid();
  p->state = USED;

  // Allocate a trapframe page.
  if((p->trapframe = (struct trapframe *)kalloc()) == 0){
    freeproc(p);
    release(&p->lock);
    return 0;
  }

  // An empty user page table.
  p->pagetable = proc_pagetable(p);
  if(p->pagetable == 0){
    freeproc(p);
    release(&p->lock);
    return 0;
  }

  // Set up new context to start executing at forkret,
  // which returns to user space.
  memset(&p->context, 0, sizeof(p->context));
  p->context.ra = (uint64)forkret;
  p->context.sp = p->kstack + PGSIZE;

  return p;
}

// free a proc structure and the data hanging from it,
// including user pages.
// p->lock must be held.
static void
freeproc(struct proc *p)
{
  if(p->trapframe)
    kfree((void*)p->trapframe);
  p->trapframe = 0;
  if(p->pagetable)
    proc_freepagetable(p->pagetable, p->sz);
  p->pagetable = 0;
  p->sz = 0;
  p->pid = 0;
  p->parent = 0;
  p->name[0] = 0;
  p->chan = 0;
  p->killed = 0;
  p->xstate = 0;
  p->state = UNUSED;
}

// Create a user page table for a given process, with no user memory,
// but with trampoline and trapframe pages.
pagetable_t
proc_pagetable(struct proc *p)
{
  pagetable_t pagetable;

  // An empty page table.
  pagetable = uvmcreate();
  if(pagetable == 0)
    return 0;

  // map the trampoline code (for system call return)
  // at the highest user virtual address.
  // only the supervisor uses it, on the way
  // to/from user space, so not PTE_U.
  if(mappages(pagetable, TRAMPOLINE, PGSIZE,
              (uint64)trampoline, PTE_R | PTE_X) < 0){
    uvmfree(pagetable, 0);
    return 0;
  }

  // map the trapframe page just below the trampoline page, for
  // trampoline.S.
  if(mappages(pagetable, TRAPFRAME, PGSIZE,
              (uint64)(p->trapframe), PTE_R | PTE_W) < 0){
    uvmunmap(pagetable, TRAMPOLINE, 1, 0);
    uvmfree(pagetable, 0);
    return 0;
  }

  return pagetable;
}

// Free a process's page table, and free the
// physical memory it refers to.
void
proc_freepagetable(pagetable_t pagetable, uint64 sz)
{
  uvmunmap(pagetable, TRAMPOLINE, 1, 0);
  uvmunmap(pagetable, TRAPFRAME, 1, 0);
  // printf("uvmfree\n");
  uvmfree(pagetable, sz);
}

// a user program that calls exec("/init")
// assembled from ../user/initcode.S
// od -t xC ../user/initcode
uchar initcode[] = {
  0x17, 0x05, 0x00, 0x00, 0x13, 0x05, 0x45, 0x02,
  0x97, 0x05, 0x00, 0x00, 0x93, 0x85, 0x35, 0x02,
  0x93, 0x08, 0x70, 0x00, 0x73, 0x00, 0x00, 0x00,
  0x93, 0x08, 0x20, 0x00, 0x73, 0x00, 0x00, 0x00,
  0xef, 0xf0, 0x9f, 0xff, 0x2f, 0x69, 0x6e, 0x69,
  0x74, 0x00, 0x00, 0x24, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00
};

// Set up first user process.
void
userinit(void)
{
  struct proc *p;

  p = allocproc();
  initproc = p;
  
  // allocate one user page and copy initcode's instructions
  // and data into it.
  uvmfirst(p->pagetable, initcode, sizeof(initcode));
  p->sz = PGSIZE;

  // prepare for the very first "return" from kernel to user.
  p->trapframe->epc = 0;      // user program counter
  p->trapframe->sp = PGSIZE;  // user stack pointer

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  p->state = RUNNABLE;

  release(&p->lock);
}

// Grow or shrink user memory by n bytes.
// Return 0 on success, -1 on failure.
int
growproc(int n)
{
  uint64 sz;
  struct proc *p = myproc();

  sz = p->sz;
  if(n > 0){
    if((sz = uvmalloc(p->pagetable, sz, sz + n, PTE_W)) == 0) {
      return -1;
    }
  } else if(n < 0){
    sz = uvmdealloc(p->pagetable, sz, sz + n);
  }
  p->sz = sz;
  return 0;
}

// Create a new process, copying the parent.
// Sets up child kernel stack to return as if from fork() system call.
int
fork(void)
{
  // printf("fork\n");
  int i, pid;
  struct proc *np;
  struct proc *p = myproc();
  // printf("pid: %d\n",p->pid);

  // Allocate process.
  if((np = allocproc()) == 0){
    return -1;
  }

  // Copy user memory from parent to child.
  if(uvmcopy(p->pagetable, np->pagetable, p->sz) < 0){
    freeproc(np);
    release(&np->lock);
    return -1;
  }
  np->sz = p->sz;

  // printf("copy done\n");
  // 여기에 shared mapping / private mapping 
  for(int i=0;i<4;i++){
    // printf("inside loop i : %d\n",i);
    np->vm[i] = p->vm[i];
    if(np->vm[i].valid){
      // printf("valid loop i : %d\n",i);
      if(np->vm[i].flags & MAP_HUGEPAGE){
        for(uint64 j=0;j<p->vm[i].length;j+=HUGE_PAGE_SIZE){
          // printf("inside loop j : %d\n",j);
          uint64 addr = (uint64)p->vm[i].addr + j;
          pte_t *pte = walk_huge(p->pagetable, addr, 0);
          uint64 perm = np->vm[i].prot | PTE_U;
          if(pte && *pte){
            // printf("valid loop j : %d\n",j);
            uint64 pa = PTE2PA(*pte);
            hgrefcount[(pa-KERNBASE)>>21]++;
            if(p->vm[i].flags & MAP_PRIVATE){
              *pte = (*pte) & ~(PTE_W);
              perm = perm & ~(PTE_W);
            }
            maphugepages(np->pagetable, addr, HUGE_PAGE_SIZE, pa, perm);
          }
        }
      } else {
        for(uint64 j=0;j<p->vm[i].length;j+=PGSIZE){
          // printf("inside loop j : %d\n",j);
          uint64 addr = (uint64)p->vm[i].addr + j;
          pte_t *pte = walk(p->pagetable, addr, 0);
          uint64 perm = np->vm[i].prot | PTE_U;
          if(pte && *pte){
            // printf("valid loop j : %d\n",j);
            uint64 pa = PTE2PA(*pte);
            bprefcount[(pa-KERNBASE)>>PGSHIFT]++;
            if(p->vm[i].flags & MAP_PRIVATE){
              *pte = (*pte) & ~(PTE_W);
              perm = perm & ~(PTE_W);
            }
            mappages(np->pagetable, addr, PGSIZE, pa, perm);
          }
        }
      }
    }
  }
  // printf("loop done\n");
  // for(int i=0;i<64;i++){
  //   printf("hgrefcount[%d]: %d\n",i,hgrefcount[i]);
  // }


  // printf("copy2 done\n");

  // copy saved user registers.
  *(np->trapframe) = *(p->trapframe);

  // Cause fork to return 0 in the child.
  np->trapframe->a0 = 0;

  // increment reference counts on open file descriptors.
  for(i = 0; i < NOFILE; i++)
    if(p->ofile[i])
      np->ofile[i] = filedup(p->ofile[i]);
  np->cwd = idup(p->cwd);

  safestrcpy(np->name, p->name, sizeof(p->name));

  pid = np->pid;

  release(&np->lock);

  acquire(&wait_lock);
  np->parent = p;
  release(&wait_lock);

  acquire(&np->lock);
  np->state = RUNNABLE;
  release(&np->lock);
  return pid;
}

// Pass p's abandoned children to init.
// Caller must hold wait_lock.
void
reparent(struct proc *p)
{
  struct proc *pp;

  for(pp = proc; pp < &proc[NPROC]; pp++){
    if(pp->parent == p){
      pp->parent = initproc;
      wakeup(initproc);
    }
  }
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait().
void
exit(int status)
{
  struct proc *p = myproc();

  acquire(&p->lock);
  for(int i=0;i<4;i++){
    unmap_vm(i,p);
    p->vm[i].valid = 0;
    p->vm[i].addr = NULL;
    p->vm[i].length = 0;
    p->vm[i].prot = 0;
    p->vm[i].flags = 0;
  }
  // printf("unmap from exit done\n");
  release(&p->lock);


  if(p == initproc)
    panic("init exiting");

  // Close all open files.
  for(int fd = 0; fd < NOFILE; fd++){
    if(p->ofile[fd]){
      struct file *f = p->ofile[fd];
      fileclose(f);
      p->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(p->cwd);
  end_op();
  p->cwd = 0;

  acquire(&wait_lock);

  // Give any children to init.
  reparent(p);

  // Parent might be sleeping in wait().
  wakeup(p->parent);
  
  acquire(&p->lock);

  p->xstate = status;
  p->state = ZOMBIE;

  release(&wait_lock);
  // printf("before sched\n");
  // Jump into the scheduler, never to return.
  sched();
  panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int
wait(uint64 addr)
{
  struct proc *pp;
  int havekids, pid;
  struct proc *p = myproc();

  acquire(&wait_lock);

  for(;;){
    // Scan through table looking for exited children.
    havekids = 0;
    for(pp = proc; pp < &proc[NPROC]; pp++){
      if(pp->parent == p){
        // make sure the child isn't still in exit() or swtch().
        acquire(&pp->lock);

        havekids = 1;
        if(pp->state == ZOMBIE){
          // Found one.
          pid = pp->pid;
          if(addr != 0 && copyout(p->pagetable, addr, (char *)&pp->xstate,
                                  sizeof(pp->xstate)) < 0) {
            release(&pp->lock);
            release(&wait_lock);
            return -1;
          }
          freeproc(pp);
          release(&pp->lock);
          release(&wait_lock);
          return pid;
        }
        release(&pp->lock);
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || killed(p)){
      release(&wait_lock);
      return -1;
    }
    
    // Wait for a child to exit.
    sleep(p, &wait_lock);  //DOC: wait-sleep
  }
}

// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run.
//  - swtch to start running that process.
//  - eventually that process transfers control
//    via swtch back to the scheduler.
void
scheduler(void)
{
  struct proc *p;
  struct cpu *c = mycpu();
  
  c->proc = 0;
  for(;;){
    // Avoid deadlock by ensuring that devices can interrupt.
    intr_on();

    for(p = proc; p < &proc[NPROC]; p++) {
      acquire(&p->lock);
      if(p->state == RUNNABLE) {
        // Switch to chosen process.  It is the process's job
        // to release its lock and then reacquire it
        // before jumping back to us.
        p->state = RUNNING;
        c->proc = p;
        swtch(&c->context, &p->context);

        // Process is done running for now.
        // It should have changed its p->state before coming back.
        c->proc = 0;
      }
      release(&p->lock);
    }
  }
}

// Switch to scheduler.  Must hold only p->lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->noff, but that would
// break in the few places where a lock is held but
// there's no process.
void
sched(void)
{
  int intena;
  struct proc *p = myproc();

  if(!holding(&p->lock))
    panic("sched p->lock");
  if(mycpu()->noff != 1)
    panic("sched locks");
  if(p->state == RUNNING)
    panic("sched running");
  if(intr_get())
    panic("sched interruptible");

  intena = mycpu()->intena;
  swtch(&p->context, &mycpu()->context);
  mycpu()->intena = intena;
}

// Give up the CPU for one scheduling round.
void
yield(void)
{
  struct proc *p = myproc();
  acquire(&p->lock);
  p->state = RUNNABLE;
  sched();
  release(&p->lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch to forkret.
void
forkret(void)
{
  static int first = 1;

  // Still holding p->lock from scheduler.
  release(&myproc()->lock);

  if (first) {
    // File system initialization must be run in the context of a
    // regular process (e.g., because it calls sleep), and thus cannot
    // be run from main().
    first = 0;
    fsinit(ROOTDEV);
  }

  usertrapret();
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void
sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();
  
  // Must acquire p->lock in order to
  // change p->state and then call sched.
  // Once we hold p->lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup locks p->lock),
  // so it's okay to release lk.

  acquire(&p->lock);  //DOC: sleeplock1
  release(lk);

  // Go to sleep.
  p->chan = chan;
  p->state = SLEEPING;

  sched();

  // Tidy up.
  p->chan = 0;

  // Reacquire original lock.
  release(&p->lock);
  acquire(lk);
}

// Wake up all processes sleeping on chan.
// Must be called without any p->lock.
void
wakeup(void *chan)
{
  struct proc *p;

  for(p = proc; p < &proc[NPROC]; p++) {
    if(p != myproc()){
      acquire(&p->lock);
      if(p->state == SLEEPING && p->chan == chan) {
        p->state = RUNNABLE;
      }
      release(&p->lock);
    }
  }
}

// Kill the process with the given pid.
// The victim won't exit until it tries to return
// to user space (see usertrap() in trap.c).
int
kill(int pid)
{
  struct proc *p;

  for(p = proc; p < &proc[NPROC]; p++){
    acquire(&p->lock);
    if(p->pid == pid){
      p->killed = 1;
      if(p->state == SLEEPING){
        // Wake process from sleep().
        p->state = RUNNABLE;
      }
      release(&p->lock);
      return 0;
    }
    release(&p->lock);
  }
  return -1;
}

void
setkilled(struct proc *p)
{
  acquire(&p->lock);
  p->killed = 1;
  release(&p->lock);
}

int
killed(struct proc *p)
{
  int k;
  
  acquire(&p->lock);
  k = p->killed;
  release(&p->lock);
  return k;
}

// Copy to either a user address, or kernel address,
// depending on usr_dst.
// Returns 0 on success, -1 on error.
int
either_copyout(int user_dst, uint64 dst, void *src, uint64 len)
{
  struct proc *p = myproc();
  if(user_dst){
    return copyout(p->pagetable, dst, src, len);
  } else {
    memmove((char *)dst, src, len);
    return 0;
  }
}

// Copy from either a user address, or kernel address,
// depending on usr_src.
// Returns 0 on success, -1 on error.
int
either_copyin(void *dst, int user_src, uint64 src, uint64 len)
{
  struct proc *p = myproc();
  if(user_src){
    return copyin(p->pagetable, dst, src, len);
  } else {
    memmove(dst, (char*)src, len);
    return 0;
  }
}

// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void
procdump(void)
{
  static char *states[] = {
  [UNUSED]    "unused",
  [USED]      "used",
  [SLEEPING]  "sleep ",
  [RUNNABLE]  "runble",
  [RUNNING]   "run   ",
  [ZOMBIE]    "zombie"
  };
  struct proc *p;
  char *state;

  printf("\n");
  for(p = proc; p < &proc[NPROC]; p++){
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    printf("%d %s %s", p->pid, state, p->name);
    printf("\n");
  }
}

// #ifdef SNU
void
pagefault(uint64 scause, uint64 stval)
{
  pagefaults++;
  // printf("pagefault\n");
  // printf("stval: %p\n",stval);
  // PA4: FILL HERE

  struct mmapped_region *vm = NULL;
  struct proc *p = myproc();
  int idx;
  acquire(&p->lock);

  // this needs change vm 을 처음에 어떻게 설정해야할까?
  for(int i=0;i<4;i++){
    if(p->vm[i].valid) {
      // printf("vm addr: %p\n", p->vm[i].addr);
      if((uint64)p->vm[i].addr <= stval && (uint64)p->vm[i].addr + p->vm[i].length > stval){
        vm = &p->vm[i];
        idx = i;
        break;
      }
    }
  }
  // printf("vm selected\n");
  if (!vm) {
    printf("usertrap(): unexpected scause %p pid=%d\n", scause, p->pid);
    printf("            sepc=%p stval=%p\n", r_sepc(), stval);
    p->killed = 1;
    release(&p->lock);
    return;
  }



  pte_t *pte;
  if(scause == 15){
    // read only
    // printf("%p",vm->prot);
    if(!(vm->prot & PTE_W)){
      // printf("read only\n");
      p->killed = 1;
      release(&p->lock);
      return;
    }

    if(!(vm->flags & MAP_SHARED)){
    // private mapping cow but only one reference left
      if(vm->flags & MAP_HUGEPAGE){
        if((pte = walk_huge(p->pagetable, (uint64)vm->addr, 0))){
          if(*pte & PTE_V){
            if( hgrefcount[((uint64)PTE2PA(*pte) - KERNBASE) >> 21] == 1){
              *pte = (*pte) | PTE_W;
              release(&p->lock);
              return;
            }
            unmap_vm(idx,p);
          }
        }
      }else {
        if((pte = walk(p->pagetable, (uint64)vm->addr, 0))){
          if(*pte & PTE_V){
            if(bprefcount[((uint64)PTE2PA(*pte) - KERNBASE) >> PGSHIFT] == 1){
              // printf("ref cnt 1\n");
              *pte = (*pte) | PTE_W;
              release(&p->lock);
              return;
            }
            // printf("unmap\n");
            unmap_vm(idx,p);
          }
        }
      }
    }
  }

  char *pa;
  if(vm->flags & MAP_HUGEPAGE) pa = kalloc_huge();
  else pa = kalloc();
  if(pa == 0)
    panic("kalloc");
  // printf("addr: %p\n",vm->addr);
  // printf("length: %d\n",vm->length);
  // printf("flags: %d\n",vm->flags);

  if(vm->flags & MAP_HUGEPAGE){
    // printf("huge page alloc\n");
    memset(pa, 0, HUGE_PAGE_SIZE);
    uint64 fault_addr_head = PGHGROUNDDOWN(stval);
    if (maphugepages(p->pagetable, fault_addr_head, HUGE_PAGE_SIZE, (uint64)pa, vm->prot | PTE_U) != 0) {
      // printf("something wrong?\n");
      kfree_huge(pa);
      p->killed = 1;
    } else{
      hgrefcount[(uint64)(pa-KERNBASE)>>21] = 1;
      if(vm->flags&MAP_SHARED){
        // printf("ok good\n");
        struct proc *pp;
        int cnt = 0;
        for(pp = proc; pp < &proc[NPROC]; pp++) {
          if(pp == p) continue;
          acquire(&pp->lock);
          for(int i=0;i<4;i++){
            if(pp->vm[i].addr == vm->addr){
              maphugepages(pp->pagetable, fault_addr_head, HUGE_PAGE_SIZE, (uint64)pa, vm->prot | PTE_U);
              cnt++;
              break;
            }
          }
          release(&pp->lock);
        }
        hgrefcount[(uint64)(pa-KERNBASE)>>21] += cnt;
      }
    }
  } else{
    // printf("base page alloc\n");
    memset(pa, 0, PGSIZE);
    uint64 fault_addr_head = PGROUNDDOWN(stval);
    if (mappages(p->pagetable, fault_addr_head, PGSIZE, (uint64)pa, vm->prot | PTE_U) != 0) {
      kfree(pa);
      p->killed = 1;
    } else{
      bprefcount[(uint64)(pa-KERNBASE)>>PGSHIFT] = 1;
      if(vm->flags&MAP_SHARED){
        // printf("ok good\n");
        struct proc *pp;
        int cnt = 0;
        for(pp = proc; pp < &proc[NPROC]; pp++) {
          if(pp == p) continue;
          acquire(&pp->lock);
          for(int i=0;i<4;i++){
            if(pp->vm[i].addr == vm->addr){
              mappages(pp->pagetable, fault_addr_head, PGSIZE, (uint64)pa, vm->prot | PTE_U);
              cnt++;
              break;
            }
          }
          release(&pp->lock);
        }
        bprefcount[(uint64)(pa-KERNBASE)>>PGSHIFT] += cnt;
      }
    }
  }
  release(&p->lock);
  // printf("page fault done\n");
}
// #endif



// pte_t *
// walk(pagetable_t pagetable, uint64 va, int alloc)
// {
//   if(va >= MAXVA)
//     panic("walk");

//   for(int level = 2; level > 0; level--) {
//     pte_t *pte = &pagetable[PX(level, va)];
//     if(*pte & PTE_V) {
//       pagetable = (pagetable_t)PTE2PA(*pte);
//     } else {
//       if(!alloc || (pagetable = (pde_t*)kalloc()) == 0)
//         return 0;
//       memset(pagetable, 0, PGSIZE);
//       *pte = PA2PTE(pagetable) | PTE_V;
//     }
//   }
//   return &pagetable[PX(0, va)];
// }

// int
// mappages(pagetable_t pagetable, uint64 va, uint64 size, uint64 pa, int perm)
// {
//   uint64 a, last;
//   pte_t *pte;

//   if(size == 0)
//     panic("mappages: size");
  
//   a = PGROUNDDOWN(va);
//   last = PGROUNDDOWN(va + size - 1);
//   for(;;){
//     if((pte = walk(pagetable, a, 1)) == 0)
//       return -1;
//     if(*pte & PTE_V)
//       panic("mappages: remap");
//     *pte = PA2PTE(pa) | perm | PTE_V;
//     if(a == last)
//       break;
//     a += PGSIZE;
//     pa += PGSIZE;
//   }
//   return 0;
// }