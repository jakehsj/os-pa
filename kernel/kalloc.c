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
int is_allocated[64]; // huge page alloc from Kernbase ~ phystop 

int bprefcount[(PHYSTOP-KERNBASE) >> PGSHIFT];
int hgrefcount[(PHYSTOP-KERNBASE) >> 21];

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

// #ifdef SNU
int freemem, used4k, used2m;
// #endif

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE){
    bprefcount[((uint64)p - KERNBASE) >> PGSHIFT] = 0;
    kfree(p);
  }
  // printf("hi~~\n");
  freemem = ((uint64)pa_end - (uint64)pa_start) / PGSIZE;
  used4k = 32*1024 - freemem;
}

// Free the page of physical memory pointed at by pa,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r, *prev;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  if(is_allocated[(PGHGROUNDDOWN((uint64)pa) / HUGE_PAGE_SIZE) - 1024]){
    // printf("kfree pa: %p", pa);
    // printf("idx: %d",(PGHGROUNDDOWN((uint64)pa)  / HUGE_PAGE_SIZE)-1024);
    panic("kfree");
  }

  // Fill with junk to catch dangling refs.
  
  acquire(&kmem.lock);
  
  if(bprefcount[((uint64)pa - KERNBASE) >> PGSHIFT] > 1){
    // printf("%d\n",bprefcount[((uint64)pa - KERNBASE) >> PGSHIFT]);
    bprefcount[((uint64)pa - KERNBASE) >> PGSHIFT]-=1;
    release(&kmem.lock);
    return;
  }
  bprefcount[((uint64)pa - KERNBASE) >> PGSHIFT]-=1;

  memset(pa, 1, PGSIZE);
  for (prev = NULL, r = kmem.freelist; 
       r != NULL && (void *)r > pa; 
       prev = r, r = r->next);

  if(r == pa){
    panic("kfree");
  }

  if (prev == NULL) {  // Insert at the beginning
    ((struct run*)pa)->next = kmem.freelist;
    kmem.freelist = (struct run*)pa;
  } else {  // Insert in the middle or end
    ((struct run*)pa)->next = prev->next;
    prev->next = (struct run*)pa;
  }
  used4k -= 1;
  freemem += 1;

  release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r, *prev, *next;
  int count;

  acquire(&kmem.lock);
  for (prev = NULL, r = kmem.freelist; r != NULL; prev = r, r = next) {
    // Check if r is 2MiB aligned
    if (((uint64)r + PGSIZE) % HUGE_PAGE_SIZE == 0) {
      // printf("ah ");
      // Check if the next 511 pages are contiguous for a potential huge page
      count = 1;
      next = r->next;
      while (count < 512 && next && (char*)next + PGSIZE == (char*)r) {
        r = next;
        next = r->next;
        count++;
      }

      if (count == 512) {
        continue;  // Continue to check the rest of the list
      } else{
        // printf("actually here!\n");
        if(prev){
          r = prev->next;
          prev->next = r->next;
        } else{
          r = kmem.freelist;
          kmem.freelist = kmem.freelist->next;
        }
        used4k += 1;
        freemem -= 1;
        release(&kmem.lock);

        memset((char*)r, 5, PGSIZE); // Fill with junk
        return (void*)r;
      }
    } else {
      // printf("found one!\n");
      if(prev){
        prev->next = r->next;
      } else{
        kmem.freelist = r->next;
      }

      used4k += 1;
      freemem -=1;
      release(&kmem.lock);

      if (r) {
        memset((char*)r, 5, PGSIZE); // Fill with junk
      }
      return (void*)r;
    }
  }
  // printf("hello\n");
  // Allocate this page
  r = kmem.freelist;
  if(r){
    kmem.freelist = r->next;
    used4k += 1;
    freemem -=1;
  }
  release(&kmem.lock);

  if (r) {
    memset((char*)r, 5, PGSIZE); // Fill with junk
  }
  return (void*)r;
}


// #ifdef SNU

void *kalloc_huge() {
  struct run *r, *prev, *start = NULL;
  int count;

  acquire(&kmem.lock);
    
  for (r = kmem.freelist, prev = NULL, count = 0; r != NULL; prev = r, r = r->next) {
    if (count == 0) {
      // Check alignment for the start of a huge page
      if ((((uint64)r + PGSIZE) & (HUGE_PAGE_SIZE - 1)) == 0) {
        // printf("start!\n");
        start = prev;
        count++;
      }
    } else {
        // Check consecutive pages
      if ((char*)r == (char*)prev - PGSIZE) {
        // printf("continuing ");
        count++;
        if (count == NUM_SMALL_PAGES_IN_HUGE_PAGE) {
            // Found a huge page
          if (start == NULL) {
            kmem.freelist = r->next;
          } else {
              start->next = r->next;
          }
          used2m += 1;
          freemem -=512;
          // printf("huge alloc idx : %d\n",((uint64)r / HUGE_PAGE_SIZE) - 1024);
          // printf("return pa : %p\n",r);
          is_allocated[((uint64)r / HUGE_PAGE_SIZE) - 1024] = 1;
          memset((char*)r, 5, HUGE_PAGE_SIZE);  // Fill with junk
          release(&kmem.lock);
          return (void*)r;
        }
      } else {
        // printf("too bad\n");
        count = 0;  // Reset count if pages are not consecutive
      }
    }
  }

  release(&kmem.lock);
  return NULL;  // No available huge page frame
}


void kfree_huge(void *pa) {
  struct run *r, *prev;
  int i;

  if (((uint64)pa % HUGE_PAGE_SIZE) != 0 || !is_allocated[((uint64)pa / HUGE_PAGE_SIZE)-1024])
    panic("kfree_huge");

  // printf("%p\n", pa);
  
  for (prev = NULL, r = kmem.freelist; 
       r != NULL && (void *)r > pa; 
       prev = r, r = r->next);

  // pa 와 pa + HUGE_PAGE_SIZE 사이에 kfree 된 페이지가 있는경우, 그리고 pa 뒤에도 free 된 페이지가 있는경우
  if((uint64)prev < (uint64)pa + HUGE_PAGE_SIZE && prev != NULL){
    // printf("prev: %p\n", prev);
    // printf("freelist: %p\n", r);
    panic("kfree_huge");
  }

  if(pa == r) panic("kfree_huge");


  acquire(&kmem.lock);
  if(hgrefcount[((uint64)pa - KERNBASE) >> 21] > 1){
    hgrefcount[((uint64)pa - KERNBASE) >> 21] -= 1;
    release(&kmem.lock);
    return;
  }
  hgrefcount[((uint64)pa - KERNBASE) >> 21] -= 1;

  memset(pa,1,HUGE_PAGE_SIZE);
  is_allocated[((uint64)pa / HUGE_PAGE_SIZE)-1024] = 0;
  // printf("i disallocated it\n");
  // printf("pa : %p\n",pa);
  // printf("idx: %d\n",((uint64)pa / HUGE_PAGE_SIZE)-1024);


  for (i = 0; i < NUM_SMALL_PAGES_IN_HUGE_PAGE; i++) {
    if (prev == NULL) {  // Insert at the beginning
      ((struct run*)pa)->next = kmem.freelist;
      kmem.freelist = (struct run*)pa;
    } else {  // Insert in the middle or end
      ((struct run*)pa)->next = prev->next;
      prev->next = (struct run*)pa;
    }
    pa = (char*)pa + PGSIZE;
  }
  used2m -= 1;
  freemem += 512;
  release(&kmem.lock);
}

// #endif
