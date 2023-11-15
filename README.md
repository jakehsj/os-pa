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
