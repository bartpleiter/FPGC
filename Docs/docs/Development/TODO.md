# TODO list

!!! Warning
    This is my TODO list for development purposes, not really documentation for how to do development.

## Verilog/FPGA design

- [ ] Remove line buffer in FSX_SRAM and access SRAM instead again for the second line to save up resources
- [ ] Improve VRAMPX write fifo design now we run at 100MHz
- [ ] Status led modules for the Cyclone IV PCB

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
