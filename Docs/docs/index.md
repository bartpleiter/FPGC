# FPGC

![FPGC Logo](images/logo_big_alpha.png)

## What is FPGC?

FPGC (FPGA Computer) is a personal hobby project where I build an entire computer around an FPGA from scratch. None of the main components (CPU, GPU, Memory controllers, PCB, Assembler, OS, FS, Network protocol) use existing designs. Only the C compiler toolchain is modified from an existing codebase (QBE + cproc, libc). The project goes from clock-by-clock hardware logic all the way up to running machine learning algorithms as user programs on a fully custom software stack.

Why? It is the only way to truely understand how computers work, and it is fun to take "full-stack developer" to the extreme.

This project is the successor of the FPGC6 project. Building abstraction layers only works if the foundation is solid, and it took a few restarts to get here.

## What does it cover?

- **FPGA logic design**: Custom CPU, GPU, Memory Unit, and I/O, all in Verilog
- **PCB design**: A fully custom board with FPGA, SDRAM, SRAM (as VRAM), SPI Flash, Ethernet, USB, HDMI, and more
- **Software**: Bootloaders, Assembler, C compiler toolchain, Filesystem, Operating System, Networking Protocol, Set of user programs
- **Toolchain**: Development tools for programming and interacting with the system via UART and Ethernet, lots of testing frameworks as well
- **Physical design**: 3D-printed case and monitor for a standalone desktop computer

See [Specifications](System-overview/Specifications.md) for the full hardware/software spec sheet, or [Architecture](System-overview/Architecture.md) for how everything connects.

## Project goals

### Main goals

- [x] Use the FPGC as a fully portable standalone PC that can write, compile, and run software without an external computer
- [x] Run DOOM at a playable framerate, which requires the CPU to be efficient and fast enough, and a solid software stack as it is a relatively complex program [Achieved with 20-30 FPS in low-detail mode without border]
- [x] Run a demo in a cluster setup of 5 FPGCs networked together, with one acting as a server and the other four rendering different parts of the screen in parallel [Achieved with a distributed mandelbrot renderer, and distributed Tetris genetic algorithm]
- [x] Properly document the entire project [Achieved by this docs site, which I try to keep up to date]

### Sub goals

- [x] Custom RAM-optimized filesystem (BRFS)
- [x] Fully custom PCB without a development board
- [x] Custom OS with system calls and a proper architecture for user programs (position independent code with a relocation table, separate user memory slots, etc)
- [x] Mini server rack setup with 5 FPGCs networked together cleanly
- [ ] 3D-printed case and monitor for a single standalone physical computer
- [x] Self-hosted assembler and C compiler running on the FPGC
- [x] Ethernet communication via custom layer 2 protocol
- [x] SD card mass storage
- [x] Modern C compiler setup with linker support in toolchain, and a proper standard library (libc) to make porting existing C code easier
- [x] Modify the CPU design and software stack to become byte-addressable instead of the word-addressable design I have been using since the start of FPGC
- [x] Efficient CPU design to allow 100 MHz execution
- [x] DMA support for faster data transfers
