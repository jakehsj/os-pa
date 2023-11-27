#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

void
main(int argc, char *argv[])
{
  uint64 p = (uint64) 0x100000000ULL;

  mmap((void *)p, 100, PROT_WRITE, MAP_SHARED | MAP_HUGEPAGE);
  if (fork() == 0)
  {
    *(int *)p = 0x900dbeef;
    if (fork() == 0)
    {
      
      printf("pid %d: %x\n", getpid(), *(int *)p);
      exit(0);
    }
    wait(0);
    printf("pid %d: %x\n", getpid(), *(int *)p);
    exit(0);
  }
  wait(0);
  printf("pid %d: %x\n", getpid(), *(int *)p);
  if (fork() == 0)
  {
    if (fork() == 0)
    {
      *(int *)p = 0xdeadbeef;
      printf("pid %d: %x\n", getpid(), *(int *)p);
      exit(0);
    }
    wait(0);
    printf("pid %d: %x\n", getpid(), *(int *)p);
    exit(0);
  }
  wait(0);
  printf("pid %d: %x\n", getpid(), *(int *)p);
  munmap((void *)p);
  printf("pid %d: %x\n", getpid(), *(int *)p);
  // printf("hi\n");
  exit(0);
  return;
}
