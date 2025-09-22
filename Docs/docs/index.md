# FPGC

![FPGC Logo](images/logo_big_alpha.png)

## Introduction

> To fully understand the detailed mechanics of these magical black-box devices we call computers, you must build your own from scratch.

FPGC (FPGA Computer) is a gigantic personal FPGA hobby project where I do exactly this: explore the design and implementation of a fully custom computer built around an FPGA. This project starts at the lowest level, where you define what happens at every clock cycle of the system, up to high level software where you interact with the operating system without having to think about these lower levels anymore. In the end it becomes building abstraction layer upon abstraction layer, which has it's own (interesting) challenges causing you to "jump between abstraction layers" until there is a solid foundation.

Furthermore, this project also covers other parts needed to create a fully functional physical computer, such as PCB design and programming tools. As the goal is to learn about all the different parts of a computer, even things as the filesystem are designed and built from scratch.

## Key features of the project

- FPGA Logic Design: A custom CPU, GPU, Memory Unit and I/O, all written in Verilog.
- Hardware: PCB design with I/O and memory, and a 3D-printed case plus monitor for a complete, although small, physical computer.
- Software: Bootloaders, assembler, C compiler (by modifying an existing compiler instead of building one from scratch), custom filesystem and operating system designed specifically for FPGC, with the compiler and assembler being able to run on the FPGC itself.
- Programming Tools: A full toolchain with development tools to program and interact with the system, via UART or network.

See the System Overview subpages for more details on specs and achitecture.

## End goal of the project

This project is considered finished when:

- [ ] The FPGC can be used as a fully portable standalone PC that can be used to write, compile and run software without the need for an external PC or programmer.
- [ ] The FPGC can run its own web server.
- [ ] The FPGC can run DOOM, either the original or a simplified but similar version, compiled and assembled into machine code for my own instruction set architecture, to use system calls of a self designed operating system, running from a self designed file system. 
    - Preferably with a playable framerate, and if not then the CPU of the FPGC should run the code on average in <2 cycles per instruction, which should only be possible with a proper CPU pipeline design and caching.
- [ ] The project is properly documented

!!! note
    As of writing the FPGC is going through a huge redesign (again) to work with a more efficient CPU pipeline and even a different FPGA (from Xilinx this time). While predecessor FPGC6 did tick off the top two bullets, the FPGC current is not in a state yet to run code. Therefore, no end goals are reached at this point in time.
