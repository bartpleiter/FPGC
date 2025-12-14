# TODO list

!!! Warning
    This is my TODO list for development purposes, not really documentation for how to do development.

## Copilot stories

- [ ] Convert the l1 cache design to reflect the SDRAM controller instead of MIG7 (less addresses)
- [ ] Given longest path is from CacheController|EXMEM2_result into cpu|PC_FE1, look into reducing this path
- [ ] Find a way to benchmark the l1i and l1d cache performance, and optimize them (especially l1d)

## Verilog/FPGA design

- [ ] Rewrite regbank to just use logic cells instead of attempting block RAM inference
- [ ] Rewrite stack to force use of block RAM
- [ ] Look into reducing path from CacheController|EXMEM2_result into cpu|PC_FE1
- [ ] External SRAM framebuffer design
- [ ] Optimize Cache memory path to reduce latency

## Software development

- [ ] Make test assembly programs to validate EP4CE40 PCB
- [ ] Make DTR reset and Magic Sequence configurable in makefile scripts
- [ ] Continue with C compiler to allow starting on the following:
    - [ ] BDOS V2
    - [ ] Benchmark
    - [ ] Write simple test programs in C to validate the compiler and runtime

## Documentation

- [ ] Document bootloader with current implementation
- [ ] Check for further outdated information in docs, or docs that can be written given project state/progress

---

## Development Workflow Improvements

These are potential todos for improving the development workflows.


### Simulation Infrastructure

#### Peripheral Inclusion

- [ ] Modular peripheral inclusion in simulation (`make sim-cpu UART=on SPI=off GPIO=on`) to reduce simulation complexity when not testing specific peripherals

### Performance Profiling

#### Execution Profiling

- [ ] Instruction execution counts in simulation
- [ ] Cycle-accurate performance measurement
- [ ] Cache hit/miss statistics

---

## Verilog/Hardware Improvements

### Refactor and Cleanup

- [ ] Extract Repeated Logic into Functions/Modules (possibly L1i and L1d cache miss handlers, or refactor them somewhere else)
- [ ] Consolidate State Machine Constants (they are scattered now)
- [ ] Add Stage Comments to Wire Declarations (Group wires by pipeline stage for easier navigation
- [ ] Consider Prefix Conventions (hazard_, dep_, stall conditions, flush conditions, etc
- [ ] Use forwarding logic for AddressDecoder and controlUnit so that there is no need for instantiating them in different stages
- [ ] Document Magic Numbers (named constants for magic numbers)
- [ ] Perhaps, split B32P2 into smaller modules like _hazards, _forwarding, _pipeline, _top
