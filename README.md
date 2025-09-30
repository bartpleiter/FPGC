# FPGC

FPGC (FPGA Computer) is my gigantic personal FPGA hobby project where I build an entire computer (CPU, GPU, I/O, etc) around an FPGA from scratch, meaning I do not use any existing design for the main components. This project starts at the lowest level, where you define what happens at every clock cycle of the system, up to high level software where you interact with the operating system without having to think about the lower levels anymore. Every layer is designed from scratch: Hardware design, Assembly languange, Bootloader, File System, OS, Network Protocol, and more. Furthermore, this project also covers other parts needed to create a fully functional physical computer, such as PCB design and programming tools. As the goal is to learn about all the different parts of a computer, even things as the filesystem are designed and built from scratch. The FPGC repo is the final iteration of the project, the successor of FPGC6.

## Key features of the project

- FPGA Logic Design: A custom CPU, GPU, Memory Unit and I/O, all written in Verilog, including simulation with video frame capture.
- Hardware: PCB design with I/O and memory, and a 3D-printed case and small monitor for a complete, physical computer.
- Software: Bootloaders, assembler, C compiler (only part that is not built from scratch but modified instead), custom filesystem, network protocol and operating system designed specifically for FPGC, with the compilers being able to run on the FPGC itself.
- Programming Tools: A full toolchain with development tools to program and interact with the system, via UART or network.

## End goal of the project

This project is considered finished when:

- The FPGC can be used as a fully portable standalone PC that can be used to write, compile and run software without the need for an external PC or programmer.
- The FPGC can run its own web server.
- The FPGC can run DOOM, either the original or a simplified but similar version, compiled and assembled into machine code for my own instruction set architecture, to use system calls of a self designed operating system, running from a self designed file system.
    - Preferably with a playable framerate, and if not then the CPU of the FPGC should run the code in <2 cycles per instruction, which should only be possible with a proper CPU pipeline design and caching.
- The project is properly documented

## Third-Party Components

### B32CC

Located in `BuildTools/B32CC/`, this is a modified version of SmallerC by Alexey Frunze to compile C code to B32P2 assembly.
