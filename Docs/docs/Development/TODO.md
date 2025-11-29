# TODO list

!!! Warning
    This is my TODO list for development purposes, not really documentation for how to do development.

## Verilog/FPGA design

- [x] Create top level module for EP4CE40 (custom PCB)
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

- [ ] Write getting started for Altera FPGAs (memory programming, Quartus setup, etc)
- [x] Move modules in PCB documentation down the page
