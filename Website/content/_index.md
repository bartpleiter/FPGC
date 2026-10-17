---
title: Introduction
cascade:
  type: docs
---

![FPGC Logo](/images/logo.png)

## What is FPGC?

FPGC (FPGA Computer) is a personal hobby project where I build an entire computer around an FPGA from scratch. None of the main components (CPU, GPU, Memory controllers, PCB, Assembler, OS, FS, Network protocol, etc.) use existing designs. Only the C compiler toolchain is modified from an existing codebase (QBE + cproc, libc) since creating that from scratch would be another multi-year project that will realistically never be as performant. The project goes from clock-by-clock hardware logic all the way up to running machine learning algorithms as user programs on a fully custom software stack.

**Why?** Building things yourself helps you to understand how it works, it is very satisfying to create your own physical device running your own logic and software, and it is fun to take "full-stack developer" to the extreme.

I have been working on this project on and off since 2018, going from zero knowledge on FPGAs to where this project is right now. This means I had to restart a lot of times.
