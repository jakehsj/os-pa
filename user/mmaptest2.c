#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

void
main(int argc, char *argv[])
{
  uint64 p = (uint64) 0x100000000ULL;

  mmap((void *)p, 100, PROT_WRITE, MAP_SHARED | MAP_HUGEPAGE);
  *(int *)p = 0xdeadbeef;
  if (fork() == 0)
  {
    *(int *)p = 0x900dbeef;
    printf("pid %d: %x\n", getpid(), *(int *)p);
    exit(0);
  }
  wait(0);
  printf("pid %d: %x\n", getpid(), *(int *)p);
  munmap((void *)p);
  exit(0);
  return;
}
