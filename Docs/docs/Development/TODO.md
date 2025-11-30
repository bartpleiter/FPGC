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

---

## Development Workflow Improvements

These are potential todos for improving the development workflows.

### Testing Infrastructure

#### Simulation speed

- [ ] Implement parallel testing (need to make sure there are no file conflicts)

#### CPU Testing Improvements

##### Single Test Execution

Currently `make test-cpu` runs all tests. It would be useful to run individual tests:

- [ ] Add `make test-cpu-single file=<test>` similar to `test-b32cc-single`
- [ ] Add `make debug-cpu file=<test>` for debugging specific CPU tests with GTKWave

##### Test Coverage Reporting

- [ ] Generate coverage reports showing which instructions are tested

#### Memory Controller Testing

##### Memory Configuration Switching

- [ ] Easy switching between MIG7 mock and real SDRAM controller in tests
- [ ] Configuration flag: `make test-cpu MEMORY=mig7mock` vs `make test-cpu MEMORY=sdram`

#### C Compiler Testing

##### Extended Test Coverage

- [ ] Pointer and array tests
- [ ] Struct tests
- [ ] Global variable tests
- [ ] Multi-file compilation tests
- [ ] Standard library function tests
- [ ] Look at FPGC6 for more examples

### Simulation Infrastructure

#### Simulation Mode Switching

##### Memory Backend Selection

Currently, the simulation is hardcoded to use either SDRAM or MIG7mock.

- [ ] Add makefile options and verilog ifdefs to select memory backend for simulation

##### Peripheral Inclusion

- [ ] Modular peripheral inclusion in simulation (`make sim-cpu UART=on SPI=off GPIO=on`) to reduce simulation complexity when not testing specific peripherals

### Build System

#### Makefile Improvements

##### Target Discovery

- [ ] `make list-tests` - List all available tests
- [ ] `make list-sim` - List all simulation targets

### Performance Profiling

#### Execution Profiling

- [ ] Instruction execution counts in simulation
- [ ] Cycle-accurate performance measurement
- [ ] Cache hit/miss statistics
