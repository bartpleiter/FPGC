---
title: RTL structure
weight: 6
reader: rtl
---

How the Verilog of B32P4 is organized, the conventions it follows, and the design rules that keep it at 100 MHz. How the parts work together is on the [pipeline](pipeline.md) and [caches](caches.md) pages.

## Files

The RTL is in `Hardware/cpu/`, one folder per pipeline stage. A file in a stage folder is used by that stage's module, so the file tree is also the tree of instances.

```text
Hardware/cpu/
  b32p4.v                 the top level: instances and wires
  cpu_hazard.v            busy signals and redirect -> stall and flush
  cpu_memory_map.v        address -> region
  if/
    cpu_if.v              IF: PC, fetch address, instruction select
  id/
    cpu_id.v              ID: IF/ID, decode, register read, targets
    cpu_decoder.v         instruction word -> fields and control signals
    cpu_regfile.v         the 15 registers, two read ports, one write port
    cpu_forward.v         forwarding selects, late-result stall
  ex/
    cpu_ex.v              EX: ID/EX, forwarding mux, address, decisions
    cpu_alu.v             the ALU operations
    cpu_branch.v          the branch conditions
    cpu_multiplier.v      two-stage multiplier
    cpu_divider.v         one bit per cycle divider
  mem/
    cpu_mem.v             MEM: EX/MEM, L1D, slow path, result, commit
    cpu_read_align.v      byte and halfword reads
    cpu_write_align.v     byte and halfword writes, byte enables
    cpu_stack.v           the 256-word stack and SP
    cpu_io_regs.v         the CPU-internal I/O registers
    cpu_interrupts.v      synchronizers, edges, pending, priority
  wb/
    cpu_wb.v              WB: MEM/WB, register write, retire port
  cache/
    cpu_l1i.v             L1I word and tag arrays
    cpu_l1d.v             L1D word arrays (4 byte lanes) and tag array
    cpu_cache_controller.v  misses, write-backs, clears, line port
  harness/                a top level for compiling the CPU on its own
                          in Quartus; not part of the CPU
```

## Two kinds of modules

Units do one job and know nothing about the pipeline: no valid bits, no stall or flush, no stage names. A unit is either a pure function (`cpu_alu`, `cpu_decoder`, `cpu_branch`, `cpu_memory_map`, the align modules) or a small block with its own state and plain control inputs (`cpu_regfile`, `cpu_stack`, `cpu_multiplier`, `cpu_divider`, `cpu_interrupts`, `cpu_io_regs`).

Stages are `cpu_if`, `cpu_id`, `cpu_ex`, `cpu_mem` and `cpu_wb`. A stage module contains:

- the pipeline register in front of it, with its valid bit and its stall and flush behavior. So `cpu_ex` contains ID/EX, and everything EX does with it.
- the logic of the stage, using the units that belong to it.

The other modules are neither:

- `b32p4` connects the stages, `cpu_hazard` and the cache modules.
- `cpu_hazard` is the only place where stall and flush signals are made.
- The cache modules are instantiated by `b32p4`, because both IF and MEM use them through the cache controller.

## Conventions

- One module per file, and the file name is the module name. CPU modules start with `cpu_`, because Verilog has one global namespace for modules, and the rest of the system will have its own.
- Each encoding is known by one module: the opcodes and instruction formats by `cpu_decoder`, the ALU operations by `cpu_alu`, the branch conditions by `cpu_branch`, the address map by `cpu_memory_map`, and the CPU-internal register addresses by `cpu_io_regs`. Everything else sees named signals, such as `read` or `in_rom`. There's no shared file with constants.
- A unit belongs to the stage where its result is made or its state changes. The register file belongs to ID, where it's read, and WB writes it through ports of `cpu_id`. The multiplier and divider belong to EX. The stack, the I/O registers and the interrupt controller belong to MEM. `cpu_memory_map` is used by three stages, for the PC, the data address and the jump target.
- The pipeline registers are named after the two stages: `if_id_*`, `id_ex_*`, `ex_mem_*` and `mem_wb_*`.
- Names follow the instruction set: `areg`, `breg` and `dreg` for the registers, `constant` for the value in the instruction, `read` and `write` for memory accesses.
- Pipeline control has fixed names: `busy_*` for a stage that needs more time, `stall_*` and `flush_*` for each pipeline register, `redirect` for a change of the PC.
- A request and its answer are a pair like `l1d_request` and `l1d_take`, or `bus_request` and `bus_ready`.
- Simulation-only code is marked. Checks that end a simulation when the design does something impossible are in `ifndef SYNTHESIS` blocks, next to the logic they check. The retire port is only used in simulation.
- Comments are short: a one-line header per file, short headers above blocks, and a few words per port. The explanations are on these pages.

## Design rules for timing

At 100 MHz, a cycle is 10 ns, and the CPU has little slack left. These rules keep it that way. Each one comes from a path that was too slow at some point.

- Mux selects come from registers. A select is computed one stage early and registered. The forwarding selects are computed in ID, the region of an address in EX, and every redirect comes from bits in EX/MEM.
- The late signal enters last. A signal that comes late in the cycle, such as a tag compare or a block RAM output, is combined with the others as the very last step. The stall signals are written out flat for each stage, not as a chain from one stage to the next.
- IF doesn't wait for a late signal from MEM. IF and its block RAMs are far from MEM on the chip. IF only stalls for the part of an L1D miss that's known from registers, and fetches an instruction again when it moved on too early (see [the L1D miss and IF](pipeline.md#the-l1d-miss-and-if)).
- No reset or flush on wide data. Only the valid bits and the control bits that cause side effects are cleared. Data in a bubble keeps any value. The register file has no reset at all, which lets Quartus put part of it in block RAM.
- Large storage goes in block RAM: the stack and the caches. Stall signals don't fan out into large flip-flop arrays.
- Fetch block RAMs are held with their read enable. Data block RAMs are never held. Quartus can't build a true dual-port block RAM with an enable, so the L1D reads every cycle, and the [current rule](caches.md#the-current-rule) tells MEM which output belongs to its instruction.
- What leaves the CPU comes from a register, or straight from a block RAM. The bus write data and byte enables only pass a small alignment step, and the fetch address and enable are the one exception: they go straight into the ROM's block RAM. So the CPU's timing measured on its own stays valid in the full system.
- Requests to the outside are made from a register, one cycle after the instruction enters MEM. That costs a cycle for each bus access, but keeps EX's long paths away from the bus.
- A unit that needs its operands for several cycles keeps them itself, like the divider, or EX keeps them for it (the capture rule of [forwarding](pipeline.md#forwarding)).

The tightest paths start at the L1D's block RAM outputs: the tag compare that decides an L1D miss goes through the stall logic into the clock enables of the pipeline registers.
