# 4190.307 Operating Systems (Fall 2023)
# Project #2: System Calls

## Introduction

System calls are the interfaces that allow user applications to request various services from the operating system kernel. This project aims to understand how those system calls are implemented in `xv6`.

## Problem specification

Your task is to implement a new system call named `ntraps()` that returns the total number of system calls or interrupts received by the `xv6` kernel. Detailed project requirements are described below.

**SYNOPSIS**
```
#define N_SYSCALL     0
#define N_INTERRUPT   1
#define N_TIMER       2

int ntraps(int type);
```

**DESCRIPTION**

In `xv6`, the term _trap_ is used as a generic term encompassing both _exceptions_ and _interrupts_ (see Chap. 4 of the [xv6 book](http://csl.snu.ac.kr/courses/4190.307/2023-2/book-riscv-rev3.pdf)). The system call `ntraps()` is used to get the total number of system calls, interrupts, or timer interrupts received by the `xv6` kernel since the system is booted. The actual return value depends on the `type` argument, as shown below.

**RETURN VALUE**

`ntraps()` returns the following value. 

* `ntraps(N_SYSCALL)` or `ntraps(0)` returns the total number of system calls (including the current `ntraps()` system call) made for all cores since the system is booted.
* `ntraps(N_INTERRUPT)` or `ntraps(1)` returns the total number of device interrupts (including timer interrupts) received by all cores since the system is booted. Software interrupts issued by the RISC-V hart should not be counted.
* `ntraps(N_TIMER)` or `ntraps(2)` returns the total number of timer interrupts received by all cores since the system is booted.
* Otherwise, -1 is returned.

## Skeleton code
The `pa2` branch has a user-level utility program called `ntraps`, which simply calls the `ntraps()` system call with the given argument. The source code of the `ntraps` utility is available in the `./user/ntraps.c` file. Using this program, you can get the value returned by the `ntraps()` system call.

Currently, any kernel-level implementation for supporting the `ntraps()` system call is missing. So, if you run the `ntraps` utility in the shell, you will get an error as shown below.

```
qemu-system-riscv64 -machine virt -bios none -kernel kernel/kernel -m 128M -smp 3 -nographic -global virtio-mmio.force-legacy=false -drive file=fs.img,if=none,format=raw,id=x0 -device virtio-blk-device,drive=x0,bus=virtio-mmio-bus.0

xv6 kernel is booting

hart 2 starting
hart 1 starting
init: starting sh
$ ntraps
3 ntraps: unknown sys call 22
ntraps: kernel returns an error -1
$