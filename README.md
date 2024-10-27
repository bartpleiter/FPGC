# FPGC

> To fully understand the detailed mechanics of these magical black-box devices we call computers, you must build your own from scratch.

FPGC (FPGA Computer) is a gigantic personal FPGA hobby project where I do exactly this: explore the design and implementation of a fully custom computer built around an FPGA. This project starts at the lowest level, where you define what happens at every clock cycle of the system, up to high level software where you interact with the operating system without having to think about the lower levels anymore. Furthermore, this project also covers other parts needed to create a fully functional physical computer, such as PCB design and programming tools. As the goal is to learn about all the different parts of a computer, even things as the filesystem are designed and built from scratch.

## Key features of the project

- FPGA Logic Design: A custom CPU, GPU, Memory Unit, written in Verilog.
- Hardware: PCB design with I/O, and a 3D-printed case and monitor for a complete, physical computer.
- Software: Bootloaders, assembler, C compiler (only thing not built from scratch), custom filesystem and operating system designed specifically for FPGC, with the compilers being able to run on the FPGC itself.
- Programming Tools: A full toolchain with development tools to program and interact with the system, via UART or network.

## End goal of the project

This project is considered finished when:
- The FPGC can be used as a fully portable standalone PC that can be used to write, compile and run software without the need for an external PC or programmer.
- The FPGC can run its own web server.
- The FPGC can run DOOM, either the original or a simplified but similar version, compiled and assembled into machine code for my own instruction set architecture, to use system calls of a self designed operating system, running from a self designed file system. 
    - Preferably with a playable framerate, and if not then the CPU of the FPGC should run the code in <2 cycles per instruction, which should only be possible with a proper CPU pipeline design and caching.
- The project is properly documented
