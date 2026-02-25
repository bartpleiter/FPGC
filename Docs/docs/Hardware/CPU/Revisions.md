# CPU Revisions

The B32P CPU has gone through several iterations. Each revision improved performance or maintainability based on lessons learned from the previous design.

## B32P3 (Current)

The third iteration is mainly a pipeline redesign focused on simplicity and timing optimization, while maintaining ISA compatibility with B32P2 (drop-in replacement). The new classic MIPS-style 5-stage pipeline achieves 100 MHz on a Cyclone IV FPGA, completely outperforming the B32P2 despite slightly worse IPC from the simpler design.

- **5-stage pipeline**: IF, ID, EX, MEM, WB
- **Straightforward hazard handling**: Load-use detection with 2-level data forwarding
- **100 MHz timing**: Critical paths specifically broken up to reach this clock speed for the first time
- **Simpler to extend**: The cleaner design makes it easier to add features like step-through debugging

## B32P2

The second iteration focused on reducing SDRAM latency through dedicated L1 caches with 8-word burst reads, with most memory-related components running at 100 MHz (double the CPU clock). This was a major upgrade that introduced the multi-cycle ALU unit, replacing the previous memory-mapped division and fixed-point operations. Moving multiply to a dedicated unit also improved timing, since the DSP block inputs and outputs need to be registered.

- **6-stage pipeline**: FE1, FE2, REG, EXMEM1, EXMEM2, WB
- **L1I and L1D caches**: Direct-mapped caches backed by a 256-bit burst SDRAM interface
- **Complex hazard handling**: New hazard handling was added incrementally each time a bug was found, leading to tangled forwarding paths and a maximum of about 50 MHz
- **Multi-cycle ALU**: Dedicated hardware for multiply, divide, and fixed-point operations

The complexity of the hazard logic and the long forwarding paths were the main reasons for redesigning the pipeline in B32P3.

## B32P1

The first pipelined CPU design, heavily inspired by classic MIPS architecture. This version introduced pipelining and the SLT instruction (needed by the C compiler). The SDRAM access pattern was inefficient: the pipeline had to stall every instruction to access SDRAM. Attempts to add L1 cache didn't fit the pipeline design well, so a workaround L2 cache was placed behind the SDRAM controller. It helped, but the fundamental access pattern needed a complete redesign.

## B322

The second complete CPU design, introduced with the FPGC4. An evolution and refactor of the original B32, adding support for additional interrupts. All memory access still went through the Memory Unit (memory-mapped I/O style). This was the first design to run BDOS and simple C programs compiled by a Python-based C compiler (modified ShivyC).

## B32

The original CPU, introduced with the FPGC3. This was the first CPU designed from scratch for the project, built through simulation and paired with the first working SDRAM controller.
