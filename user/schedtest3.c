//----------------------------------------------------------------
//
//  4190.307 Operating Systems (Fall 2023)
//
//  Project #3: BTS (Brain Teased Scheduler)
//
//  October 19, 2023
//
//  Jin-Soo Kim (jinsoo.kim@snu.ac.kr)
//  Systems Software & Architecture Laboratory
//  Dept. of Computer Science and Engineering
//  Seoul National University
//
//----------------------------------------------------------------

#include "kernel/param.h"
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"
#include "kernel/fcntl.h"
#include "kernel/syscall.h"
#include "kernel/memlayout.h"
#include "kernel/riscv.h"

#define P   6       // period: 6 sec = 60 ticks
#define R   5       // counts: 5 times
            
int pid1, pid2, pid3;
int sec = 0;
int p1, p2, p3;     // previous ticks
int t1, t2, t3;     // current ticks

void loginit()
{
  p1 = getticks(pid1);
  p2 = getticks(pid2);
  p3 = getticks(pid3);
  printf("%d, %d, %d, %d\n", sec, p1, p2, p3);
}

void log()
{
  int i;

  for (i = 0; i < R; i++)
  {
    sleep(P * 10);
    sec += P;
    t1 = getticks(pid1);
    t2 = getticks(pid2);
    t3 = getticks(pid3);
    printf("%d, %d, %d, %d\n", sec, t1 - p1, t2 - p2, t3 - p3);
    p1 = t1;
    p2 = t2;
    p3 = t3;
  }
}

void
main(int argc, char *argv[])
{

  if ((pid1 = fork()) == 0)
    while (1);

  if ((pid2 = fork()) == 0)
    while (1);

  if ((pid3 = fork()) == 0)
    while (1);

  loginit();

  // Phase 1: 0/0/0
  log();

  // Phase 2: -3/0/3
  nice(pid1, -3);
  nice(pid2, 0);
  nice(pid3, 3);
  log();

  // Phase 3: -2/0/2
  nice(pid1, -2);
  nice(pid2, 0);
  nice(pid3, 2);
  log();

  // Phase 4: -1/0/1
  nice(pid1, -1);
  nice(pid2, 0);
  nice(pid3, 1);
  log();

  // Phase 5: 2/2/2
  nice(pid1, 2);
  nice(pid2, 2);
  nice(pid3, 2);
  log();

  // Terminate all
  sleep(10);
  kill(pid1);
  kill(pid2);
  kill(pid3);

  wait(0);
  wait(0);
  wait(0);
  exit(0);
}
