# Modification of xv6 RISC-V Operating System

This repository contains my work for the Operating System course at SNU, focusing on modifying and enhancing the xv6 RISC-V operating system. The main modifications include the implementation of a new system call and the redesign of the operating system's scheduler.

## Branches

### PA2 - ntraps System Call
- **Branch:** `pa2`
- **Description:** This branch includes the implementation of the `ntraps()` system call. This new system call returns the total number of system calls or interrupts received by the xv6 kernel. It serves as an essential tool for monitoring and debugging the operating system's performance.
- **Link:** [PA2 - ntraps System Call](https://github.com/jakehsj/os-pa/tree/pa2)

### PA3 - Brain Teased Scheduler
- **Branch:** `pa3`
- **Description:** In this branch, I have redesigned the default Round Robin scheduler of the xv6 RISC-V operating system to a custom 'Brain Teased Scheduler'. This modification enhances task scheduling efficiency, thereby improving the overall performance of the operating system.
- **Link:** [PA3 - Brain Teased Scheduler](https://github.com/jakehsj/os-pa/tree/pa3)

### PA4 - mmap with Huge Pages
- **Branch:** `pa4`
- **Description:** This branch features the implementation of advanced memory management functionalities in the xv6 kernel. It includes the development of kalloc_huge() and kfree_huge() functions for efficient 2MiB huge page allocation and deallocation, ensuring alignment and compatibility with existing 4KiB page sizes. The branch also introduces modified mmap() and munmap() system calls to support both standard and huge page mappings, alongside intricate handling of memory-mapped regions with fork(). The implementation is rigorously designed to prevent memory leaks, maintaining system integrity and performance.
- **Link:** Not yet uploaded due to course policy
