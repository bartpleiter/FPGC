# TODO list

!!! Warning
    This is my TODO list for development purposes, not really documentation for how to do development.

## Verilog/FPGA design

- [ ] Remove line buffer in FSX_SRAM and access SRAM instead again for the second line to save up resources, or assign the SRAM to the write FIFO during the second line to fix fast writes now the CPU runs at 100MHz
- [ ] Status led modules for the Cyclone IV PCB
- [ ] Implement Debugger (via JTAG FIFO and eventually PCI if success)

## Software development

- [ ] Make DTR reset and Magic Sequence configurable in makefile scripts, or drop support for Cyclone 10 if at the point where we need hardware features of the Cyclone IV PCB
- [ ] BDOS V2 design and implementation

## Documentation

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

!!! Note
    This might actually be better to implement when the debugger module is implemented.

---

## Verilog/Hardware Improvements

### Refactor and Cleanup

- [ ] Determine what improvements did not really affect the timings and clean them up
