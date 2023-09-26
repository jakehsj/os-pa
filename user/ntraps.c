//----------------------------------------------------------
//
// 4190.307: Operating Systems (Fall 2023)
//
// Project #2: System calls
//
// September 26, 2023
//
// Jin-Soo Kim (jinsoo.kim@snu.ac.kr)
// Inje Kang (abcinje@snu.ac.kr)
// Heejae Kim (adpp00@snu.ac.kr)
// Systems and Architecture Laboratory
// Dept. of Computer Science and Engineering
// Seoul National University
//
//----------------------------------------------------------

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define N_SYSCALL   0
#define N_INTERRUPT 1
#define N_TIMER     2

int
main(int argc, char *argv[])
{
  int res;
  int type;
  char *names[] = {"syscalls", "interrupts", "timer interrupts"};

  if (argc == 1 || *argv[1] == 's')
    type = N_SYSCALL;
  else if (*argv[1] == 'i')
    type = N_INTERRUPT;
  else if (*argv[1] == 't')
    type = N_TIMER;
  else {
    printf("%s: invalid argument\n", argv[0]);
    printf("Usage: %s ['s'|'i'|'t']\n", argv[0]);
    exit(-1);
  }

  if ((res = ntraps(type)) < 0) {
      printf("%s: kernel returns an error %d\n", argv[0], res);
      exit(1);
  }

  printf("%s: # %s = %d\n", argv[0], names[type], res);
  exit(0);
}
