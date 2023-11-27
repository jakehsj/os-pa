#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

void
main(int argc, char *argv[])
{
  uint64 p = (uint64) 0x100000000ULL;

  mmap((void *)p, 100, PROT_READ, MAP_SHARED | MAP_HUGEPAGE);
  munmap((void *)p);
  // *(int *)p = 0xdeadbeef;
  // printf("pid %d: %x\n", getpid(), *(int *)p);
  exit(0);
  return;
}
