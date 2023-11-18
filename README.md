# 4190.307 Operating Systems (Fall 2023)
# Project #3: BTS (Brain Teased Scheduler)
### Due: 11:59 PM, October 31 (Tuesday)

## Introduction

Currently, the CPU scheduler of `xv6` uses a simple round-robin policy. The goal of this project is to understand the scheduling subsystem of `xv6` by implementing the scheduler named BTS (Brain Teased Scheduler).

## Background: BFS

The design of the Brain Teased Scheduler (BTS) is inspired by the Brain F\*\*k Scheduler (BFS).
The BFS is a process scheduler designed for the Linux kernel in 2009 by a kernel programmer Con Kolivas, as an alternative to the Completely Fair Scheduler (CFS) and the O(1) scheduler. The goal of BFS is to provide a scheduler with a simple algorithm that does not require adjustment of heuristics or tuning parameters to tailor performance to a specific type of computational workload. 

BFS is best described as a single runqueue, O(n) lookup, earliest effective virtual deadline first design, loosely based on EEVDF (earliest eligible virtual deadline first) and Con Kolivas' Staircase Deadline scheduler. The key to achieving low latency, scheduling fairness, and *nice level* distribution in BFS is entirely in the virtual deadline mechanism. When a task is put into the runqueue, it is given a virtual deadline. The virtual deadline is in proportion to the nice value. In other words, the task with a lower nice value (i.e., higher priority) has a shorter virtual deadline, while the task with a higher nice value (i.e., lower priority) has a longer virtual deadline. The scheduler simply picks the task with the earliest virtual deadline to run next on the CPU. The deadline is "virtual" because there is no guarantee that a task is actually scheduled by that time, but it is used to compare which task should go next. For more information on BFS, please refer to [here](https://en.wikipedia.org/wiki/Brain_Fuck_Scheduler).


## Problem specification

In order to implement and test BTS, we introduce two new system calls, `nice()` and `getticks()`. Also, we have added two new fields, `p->nice`, and `p->ticks` in the `proc` structure (@`./kernel/proc.h`) of `xv6` as shown below.

```
// Per-process state
struct proc {
  struct spinlock lock;

  // p->lock must be held when using these:
  enum procstate state;        // Process state
  void *chan;                  // If non-zero, sleeping on chan
  int killed;                  // If non-zero, have been killed
  int xstate;                  // Exit status to be returned to parent's wait
  int pid;                     // Process ID

  // The following fields are added for PA3
  int nice;                    // Nice value (-3 <= nice <= 3)
  int ticks;                   // Total number of ticks used
  ...
}
```

### 1. Implement the `nice()` system call
First, you need to implement the `nice()` system call that is used to set the `nice` value of a process. The system call number for `nice()` is already assigned to 23 in the `./kernel/syscall.h` file.

__SYNOPSIS__
```
    int nice(int pid, int value);
```

__DESCRIPTION__

The `nice()` system call sets the nice value of the process with the specified `pid` to `value`. The nice value represents the relative priority of a process. While traditional UNIX systems employ nice values ranging from -20 to 19, we use a more limited range, spanning from -3 to 3. A higher nice value means a lower priority, with the default value set to zero. When a process is created, its nice value is inherited from the parent by default. The nice value of the `init` process is set to zero. 

* If `pid` is positive, then the nice value of the process with the specified `pid` is changed. 
* If `pid` is zero, then the nice value of the calling process is changed.

__RETURN VALUE__

* On success, zero is returned. 
* On error, -1 is returned. The possible error conditions are as follows.
  - `pid` is negative.
  - There is no valid process that has `pid`.
  - The nice value exceeds the range of [-3, 3].

### 2. Implement the BTS algorithm

Our BTS (Brain Teased Scheduler) algorithm works as follows:

1. The timeslice is just one timer tick in BTS. Hence, the kernel scheduler is invoked on every timer tick.
   
2. The kernel has a predefined `prio_ratio[]` table that is used to compute the virtual deadline for each nice level as shown below. The `prio_ratio[]` table's initial entry corresponds to the nice level -3, followed by the entry for nice level -2, and so on.

   ```
   /* proc.c */
   #define NICE_TO_PRIO(n)    ((n) + 3)

   int prio_ratio[] = {1, 2, 3, 5, 7, 9, 11};
   ```

3. When a process is put into the runqueue (except for the unblocked processes, see below), its virtual deadline is computed as follows.

   Process *P*'s virtual deadline = current tick + prio_ratio[NICE_TO_PRIO(*P*'s nice value)]

4. For the next process to run, the scheduler picks the process that has the minimum virtual deadline among the runnable processes. If the virtual deadline of the selected process is the same as that of the last process, the last process is selected again for cache efficiency. When there are more than two processes that have the same virtual deadline (which is smaller than that of the previous process), the process with the lower nice value is selected. Otherwise, if the nice values are equal, the process with the lower `pid` value is chosen.

5. Once scheduled, the running process is __*NOT*__ preempted until the end of its timeslice even if a process with a lower virtual deadline is created or wakes up. 

6. When the current process is blocked, its virtual deadline is not changed. Furthermore, when the process is awakened and resumes execution, the virtual deadline remains unadjusted.

7. When the nice value of a process is changed via the `nice()` system call, its virtual deadline is adjusted immediately according to the new nice value as long as the process is waiting in the runqueue (except for the current process that calls `nice()`).  
   
8. If there are no runnable processes, the behavior of the scheduler is the same as that of the current round-robin scheduler.


**Example 1**: There are two processes P0 and P1 with the same nice value -2. This example illustrates that when there are processes with the same nice value, the CPU time is equally distributed among them.

```
Tick    Virtual deadline     Scheduled
           P0      P1         Process
  0         2       2           P0
  1         3       2           P1
  2         3       4           P0
  3         5       4           P1
  4         5       6           P0
  5         7       6           P1
...
```

**Example 2**: Assume that there are three processes P0, P1, and P2, that have the nice values of -3, -2, and 0, respectively. You can see that the process with a lower nice value receives more CPU time under the BTS.

```
Tick    Virtual deadline     Scheduled
        P0     P1     P2      Process
  0      1      2      5         P0 
  1      2      2      5         P0   
  2      3      2      5         P1
  3      3      5      5         P0
  4      5      5      5         P0
  5      6      5      5         P1
  6      6      8      5         P2
  7      6      8     12         P0
  8      9      8     12         P1
  9      9     11     12         P0
 10     11     11     12         P0
 11     12     11     12         P1  
 12     12     14     12         P0
 13     14     14     12         P2
 14     14     14     19         P0
 15     16     14     19         P1
...
```

### 3. Design document 

You need to prepare and submit the design document (in a single PDF file) for your implementation. Your design document should include the following:

* Brief summary of modifications you have made
* The details about your implementation of the `nice()` system call and the BTS algorithm
* The result of running `schedtest3` (`xv6.log` and `graph.png` files, see below for details) with the analysis of your result

## Skeleton code

The skeleton code for this project assignment (PA3) is available as a branch named `pa3`. Therefore, you should work on the `pa3` branch as follows:

```
$ git clone https://github.com/snu-csl/xv6-riscv-snu
$ cd xv6-riscv-snu
$ git checkout pa3
```

After downloading, you have to set your `STUDENTID` in the `Makefile` again.

Please note that the skeleton code already includes the implementation of the following `getticks()` system call.

__SYNOPSIS__
```
    int getticks(int pid);
```

__DESCRIPTION__

The `getticks()` system call returns the number of ticks used by the process `pid`. The kernel maintains per-process value, `p->ticks`, for each process. The `p->ticks` value is initialized to zero when the corresponding process is created. 
Afterward, the `p->ticks` value of the currently running process at the moment of a timer interrupt is incremented by one.

* If `pid` is positive, then return the number of ticks used by the process with the specified `pid`.
* If `pid` is zero, then return the number of ticks used by the calling process.

__RETURN VALUE__

* On success, the number of ticks is returned.
* On error (e.g., no process found), -1 is returned.

In addition, the `pa3` branch includes three user-level programs called `schedtest1`, `schedtest2`, and `schedtest3`, whose source code is available in `./user/schedtest1.c`, `./user/schedtest2.c`, and `./user/schedtest3.c`, respectively. `schedtest1` is a subset of the `xv6`'s `usertests` program in the `./user` directory. It tests system calls related to process management. `schedtest2` performs a stress test on the scheduler; it first creates 30 processes and then each process repeats some computation and sleep. At the same time, one of the existing processes is killed and then created again every 3 ticks. The last program, `schedtest3`, creates three CPU-intensive child processes and then measures the number of ticks used by those processes, while changing their nice values every 300 ticks.

The following shows a sample output of the `schedtest3` program when it is run under the default round-robin scheduler. The first column shows the elapsed time in seconds, and the rest of the columns represent the number of ticks used by processes P1, P2, and P3 during the 6-second period. You can see that each process uses exactly the same number of ticks.

```
xv6 kernel is booting

init: starting sh
$ schedtest3
0, 1, 1, 1
6, 20, 20, 20
12, 20, 20, 20
18, 20, 20, 20
24, 20, 20, 20
30, 20, 20, 20
36, 20, 20, 20
42, 20, 20, 20
48, 20, 20, 20
54, 20, 20, 20
60, 20, 20, 20
66, 20, 20, 20
72, 20, 20, 20
78, 20, 20, 20
84, 20, 20, 20
90, 20, 20, 20
96, 20, 20, 20
102, 20, 20, 20
108, 20, 20, 20
114, 20, 20, 20
120, 20, 20, 20
126, 20, 20, 20
132, 20, 20, 20
138, 20, 20, 20
144, 20, 20, 20
150, 20, 20, 20
$
```

Once you implement the BTS successfully, you should be able to get a result similar to the following: 

```
xv6 kernel is booting

init: starting sh
$ schedtest3
0, 0, 0, 0
6, 20, 20, 20
12, 20, 20, 20
18, 20, 20, 20
24, 20, 20, 20
30, 20, 20, 20
36, 46, 10, 4
42, 45, 10, 5
48, 45, 10, 5
54, 45, 10, 5
60, 45, 10, 5
66, 43, 11, 6
72, 42, 12, 6
78, 42, 12, 6
84, 42, 12, 6
90, 42, 12, 6
96, 37, 14, 9
102, 37, 14, 9
108, 38, 13, 9
114, 36, 14, 10
120, 37, 14, 9
126, 20, 20, 20
132, 20, 20, 20
138, 20, 20, 20
144, 20, 20, 20
150, 20, 20, 20
$
```

In the above example, you can see that the number of ticks used by each process varies depending on its nice value which is changed every 30 seconds. We provide you with a Python script called `graph.py` in the `./xv6-riscv-snu` directory. You can use the Python script to convert the above `xv6` output into a graph image. Note that the `graph.py` script requires the Python numpy, pandas, and matplotlib packages. Please install them using the following command:

```
$ sudo apt install python3-numpy python3-pandas python3-matplotlib
```

In order to generate a graph, you should run `xv6` using the `make qemu-log` command that saves all the output into the file named `xv6.log`. And then run the `make png` command to generate the `graph.png` file using the Python script, `graph.py` as shown below.

```
$ make qemu-log
qemu-system-riscv64 -machine virt -bios none -kernel kernel/kernel -m 128M -smp 1 -nographic -global virtio-mmio.force-legacy=false -drive file=fs.img,if=none,format=raw,id=x0 -device virtio-blk-device,drive=x0,bus=virtio-mmio-bus.0 | tee xv6.log

xv6 kernel is booting

init: starting sh
$ schedtest3                <---- run the schedtest3 program
0, 1, 1, 1
6, 20, 20, 20
12, 20, 20, 20
18, 20, 20, 20
24, 20, 20, 20
...
$ QEMU: Terminated          <---- quit qemu using ^a-x. xv6 output is available in the xv6.log file

$ make png                  <---- Generate the graph. (this should be done on Ubuntu, not on xv6)
./graph.py xv6.log graph.png
$
```

If everything goes fine, you will get the following graph: 

![PNG image here](https://github.com/snu-csl/os-pa3/blob/master/graph.png)

