# CPU Revisions

This page documents the evolution of the B32P CPU architecture through its various revisions.

## B32P3 (Current)

B32P3 is the third iteration of the B32P architecture, which is mainly a pipeline redesign focused on simplicity and timing optimization, while maintaining compatibility with the existing ISA (allow for drop-in replacement of B32P2). All other components of the CPU have been kept mostly the same, with only some minor timing or refactor changes. The new pipeline design allows for 100Mhz operation on a Cyclone IV or 10 FPGA, which completely outperforms the B32P2 even though it has a slightly worse IPC due to the simplified design.

**Features:**

- **Classic MIPS-style pipeline**: 5-stage design (IF, ID, EX, MEM, WB)
- **Simplified hazard handling**: Straightforward load-use detection with data forwarding
- **Timing-focused optimizations**: Critical paths broken up to achieve 100MHz on a Cyclone IV or 10 FPGA (first time reaching this speed on the CPU)
- **Easier design**: Simpler design is easier to extend (for example with a debugger -> step through support)

---

## B32P2

B32P2 was the second iteration of the B32P architecture, with a focus on single-cycle execution of all pipeline stages. This CPU redesign included major changes like the introduction of a Multi-cycle ALU unit (replacing memory mapped division or fixed point operations), and a memory map that is more tightly integrated with the CPU to allow for higher performance (at a cost of flexibility). By moving the multiply instruction to the Multi-cycle ALU, timings could be improved as the DSP block inputs/outputs need to be registered for proper timings (this was the timing bottleneck of the B32P1).

**Features:**

- **Focus on reducing SDRAM latency**: Dedicated l1 caches with bursts of 8 words, with most memory related components running at double the clock frequency (100MHz) to greatly improve performance compared to B32P1
- **L1 Instruction and Data caches**: Simple direct-mapped caches to reduce SDRAM access latency, and allow for easy integration with any (SD)RAM controller that just needs to write or read 256 bit words.
- **Deeper pipeline**: 6-stage design to make it easy for instruction cache hits to only take a single cycle
- **Complex hazard handling**: Added hazard handling each time a new hazard was discovered (which eventually led to a hard to maintain design, and long forwarding paths limiting the design to 50MHz at best)

**Pipeline Stages:**

1. **FE1 (Fetch 1)**: Instruction cache fetch and ROM access
2. **FE2 (Fetch 2)**: Instruction cache miss handling
3. **REG**: Register file read and instruction decode
4. **EXMEM1**: ALU execution and data cache access
5. **EXMEM2**: Multi-cycle ALU and data cache miss handling
6. **WB**: Register file writeback

---

## B32P1

The B32P1 (I think I called it B32P originally) was the first time I redesigned the CPU architecture to be a pipelined design. This design was heavily inspired by classic MIPS pipeline design (that ended up comming back in B32P3). Aside from learning how to pipeline a CPU, there were also some performance optimizations like the SLT instruction as this was being used by the C compiler. In the end, the CPU pipeline had to stall every instruction to access the SDRAM, and the design did not lend well for single cycle execution by attempting to add L1 cache, which resulted in just having a L2 cache behind the SDRAM controller (which did speed up things a lot, but there were some clear inefficiencies remaining that needed a complete redesign).

---

## B322

This was the second complete CPU design of the FPGC, introduced in the FPGC4. This design was an evolution and refactor of the B32, with support for things like additional interrupts. All memory access still went through the Memory Unit (memory map). This was the first CPU design that ran the first version of BDOS and could run simple C programs compiled by a Python based C compiler (modified ShivyC).

---

## B32

This was the first complete CPU design of the FPGC, introduced in the FPGC3. This was the first time I learned how to design a CPU from scratch using simulation, and ended up being able to work with my first SDRAM controller.
