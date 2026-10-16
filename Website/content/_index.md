---
title: Introduction
cascade:
  type: docs
---

![FPGC Logo](/images/logo.png)

## What is FPGC?

FPGC (FPGA Computer) is a personal hobby project where I build an entire computer around an FPGA from scratch. None of the main components (CPU, GPU, Memory controllers, PCB, Assembler, OS, FS, Network protocol) use existing designs. Only the C compiler toolchain is modified from an existing codebase (QBE + cproc, libc). The project goes from clock-by-clock hardware logic all the way up to running machine learning algorithms as user programs on a fully custom software stack.

Why? Building it yourself helps you to understand how it works, it is very satisfying to have a physical device running your own designs, and it is fun to take "full-stack developer" to the extreme.

This project is the successor of the FPGC6 project. Building abstraction layers only works if the foundation is solid, and it took a few restarts to get here.
