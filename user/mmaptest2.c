#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

void
main(int argc, char *argv[])
{
  uint64 p = (uint64) 0x100000000ULL;
  uint64 q = (uint64) 0x110000000ULL;

  mmap((void *)p, 100, PROT_WRITE, MAP_PRIVATE | MAP_HUGEPAGE);
  *(int *)(p) = 0xdeadbeef;
  if (fork() == 0)
  {
    *(int *)(p) = 0x900dbeef;
    mmap((void *)q, 100, PROT_WRITE, MAP_PRIVATE | MAP_HUGEPAGE);
    // mmap((void *)p, 100, PROT_WRITE, MAP_PRIVATE | MAP_HUGEPAGE);
    printf("pid %d, p: %x\n", getpid(), *(int *)(p));
    if(fork() == 0){
      *(int *)(q) = 0x5e5ebeef;
      printf("inside 3rd fork\n");
      printf("pid %d, q: %x\n", getpid(), *(int *)(q));
      munmap((void *)p);
      exit(0);
    }
    munmap((void *)p);
    wait(0);
    *(int *)(q) = 0x1234beef;
    printf("pid %d, q: %x\n", getpid(), *(int *)(q));
    exit(0);
  }
  // printf("wating\n");
  wait(0);
  // printf("wait done\n");    
  printf("pid %d, p: %x\n", getpid(), *(int *)(p));
  munmap((void *)p);
  exit(0);
  return;
}
