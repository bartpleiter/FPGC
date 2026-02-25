# FPGC

![FPGC Logo](images/logo_big_alpha.png)

## What is FPGC?

FPGC (FPGA Computer) is a personal hobby project where I build an entire computer around an FPGA from scratch. None of the main components (CPU, GPU, I/O, memory controller) use existing designs. The project goes from clock-by-clock hardware logic all the way up to an operating system with a shell, filesystem, including build tools like an Assembler and C compiler.

This is technically the successor of the FPGC6 project. Building abstraction layers only works if the foundation is solid, and it took a few restarts to get there.

## What does it cover?

- **FPGA logic design**: Custom CPU, GPU, Memory Unit, and I/O, all in Verilog
- **PCB design**: A fully custom board with FPGA, SDRAM, SRAM, SPI Flash, Ethernet, USB, and HDMI
- **Software**: Bootloaders, assembler, C compiler, filesystem, operating system, and networking protocol
- **Toolchain**: Development tools for programming and interacting with the system via UART and Ethernet
- **Physical design**: 3D-printed case and monitor for a standalone desktop computer

See [Specifications](System-overview/Specifications.md) for the full hardware/software spec sheet, or [Architecture](System-overview/Architecture.md) for how everything connects.

## Project goals

### Main goals

- [ ] Use the FPGC as a fully portable standalone PC that can write, compile, and run software without an external computer
- [ ] Run DOOM (or a similar game) at a playable framerate, which requires the CPU to average under 2 cycles per instruction
- [ ] Run a demo in a cluster setup of 5 FPGCs networked together, with one acting as a server and the other four rendering different parts of the screen in parallel
- [x] Properly document the entire project

### Sub goals

- [x] Custom RAM-optimized filesystem (BRFS)
- [x] Fully custom PCB without a development board
- [ ] Custom OS with system calls and a proper architecture for user programs
- [ ] 3D-printed case and monitor for a standalone physical computer
- [ ] Self-hosted assembler and C compiler running on the FPGC
- [x] Ethernet communication via custom layer 2 protocol
- [ ] SD card mass storage
