# FPGC

![FPGC Logo](images/logo_big_alpha.png)

## Introduction

FPGC (FPGA Computer) is my gigantic personal FPGA hobby project where I build an entire computer (CPU, GPU, I/O, etc) around an FPGA from scratch, meaning I do not use any existing design for the main components. This project starts at the lowest level, where you define what happens at every clock cycle of the system, up to high level software where you interact with the operating system without having to think about these lower levels anymore. In the end it becomes building abstraction layer upon abstraction layer, which only works if you have a solid foundation, otherwise you will have to start over (which has happened multiple times already, technically this is version 7 of the project).

Furthermore, this project also covers other parts needed to create a fully functional physical computer, such as PCB design and programming tools. As the goal is to learn about all the different parts of a (personal) computer, even things as the filesystem are designed and built from scratch.

## Key features of the project

- FPGA Logic Design: A custom CPU, GPU, Memory Unit and I/O, all written in Verilog.
- Hardware: Complete PCB design with FPGA, Memory and I/O, and a 3D-printed case plus monitor for a complete, although small, physical computer.
- Software: Bootloaders, assembler, C compiler (by modifying an existing compiler instead of building one from scratch), custom filesystem and operating system designed specifically for FPGC, with the compiler and assembler being able to run on the FPGC itself.
- Programming Tools: A full toolchain with development tools to program and interact with the system, via UART or network.

See the System Overview subpages for more details on specs and achitecture.

## End goal of the project

This project is currently considered finished when the FPGC meets the following requirements.

### Main goals

The FPGC:

- [ ] Can be used as a fully portable standalone PC that can be used to write, compile and run software without the need for an external PC or programmer.
- [ ] Can run DOOM, either the original or a simplified but similar version.
    - Preferably with a playable framerate, and if not then the CPU of the FPGC should run the code on average in <2 cycles per instruction, which should only be possible with a proper CPU pipeline design and caching.
- [ ] Is properly documented.

## Sub goals

The FPGC:

- [ ] Uses a fully custom RAM-optimized filesystem.
- [ ] The FPGC uses a fully custom desgined PCB without the use of an FPGA development board/module.
- [ ] The FPGC uses a custom designed OS with system calls and a proper architecture to run user programs.
- [ ] The FPGC has a 3d printed case and monitor to be a fully standalone physical computer.
- [ ] The FPGC can run an assembler and C compiler.
- [ ] The FPGC can communicate via Ethernet.
- [ ] The FPGC can load and store data to mass storage (SD card).
