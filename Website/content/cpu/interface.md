---
title: Interface
weight: 3
reader: hardware
---

The ports of `b32p4`, the top-level module of the CPU, and what the logic on the other side must do. The CPU has four connections to the rest of the system: the ROM fetch port, the SDRAM line port, the bus and the interrupt inputs. A fifth one, the retire port, is only for simulation.

The outputs to the SDRAM controller and the bus come from registers (the bus write data and byte enables after a small alignment step), or straight from a block RAM inside the CPU. The fetch address and its enable are the exception: they go straight into the ROM's block RAM. So the CPU's timing doesn't depend on what it's connected to, as long as the other side also starts its paths from registers.

## Parameters

| Parameter | Default | Meaning |
| --- | --- | --- |
| `INTERRUPTS` | 2 | the number of interrupt inputs, at most 255 |
| `L1I_LINES` | 128 | the number of L1I lines, a power of two. A line is 32 bytes, so the default is 4 KiB. |
| `L1D_LINES` | 512 | the number of L1D lines, a power of two. The default is 16 KiB. |

## Clock and reset

| Port | Direction | Meaning |
| --- | --- | --- |
| `clk` | in | the CPU clock, 100 MHz. Everything in the CPU runs on its rising edge. |
| `reset` | in | synchronous reset, active high |

- Reset must be 1 for at least one rising edge. During reset, the fetch port reads the first instruction at `0x04000000`, so it's there when reset ends.
- The reset must come from a register in the `clk` domain, for example after a synchronizer when it comes from a button.
- After reset, the cache controller empties both caches for as many cycles as the larger cache has lines. It doesn't use the SDRAM line port for this.

## ROM fetch port

The CPU reads its instructions from the boot ROM directly, without the bus. The ROM must be a block RAM of 1,024 words (4 KiB) with a registered output.

| Port | Direction | Meaning |
| --- | --- | --- |
| `fetch_addr[31:0]` | out | the address of the next instruction. The ROM uses bits [11:2]. |
| `fetch_en` | out | read enable: the ROM reads `fetch_addr` at this edge |
| `rom_word[31:0]` | in | the word that the ROM read at the last edge with `fetch_en` = 1 |

- At each rising edge with `fetch_en` = 1, the ROM reads the word at `fetch_addr`. It's on `rom_word` in the next cycle.
- At an edge with `fetch_en` = 0, `rom_word` keeps its value. This is the clock enable of the block RAM's address register, and the CPU relies on it while it stalls.
- `fetch_addr` and `fetch_en` are not registers. They go straight into the block RAM's address and clock enable inputs, with nothing in between.
- The CPU drives the port for every fetch, also when it runs from SDRAM. It only uses `rom_word` when the program counter is in the ROM.

```text
cycle         1      2      3      4
fetch_en      1      0      1      1
fetch_addr    A      -      B      C
rom_word      -      w(A)   w(A)   w(B)
```

## SDRAM line port

The cache controller transfers whole cache lines (8 words, 32 bytes) to and from the SDRAM controller. It makes one request at a time.

| Port | Direction | Meaning |
| --- | --- | --- |
| `sdram_req` | out | a request for a line transfer. It stays 1 until `sdram_take`. |
| `sdram_write` | out | 1 to write the line to SDRAM, 0 to read it |
| `sdram_line[25:5]` | out | the line's address: bits [25:5] of the byte address |
| `sdram_first[2:0]` | out | for a read, the word of the line to start with. Always 0 for a write. |
| `sdram_take` | in | 1 for one cycle: the SDRAM controller has taken the request |
| `sdram_read_valid` | in | 1 for each word of a read, 8 times in total |
| `sdram_read_word[31:0]` | in | the word, in the cycle of `sdram_read_valid` |
| `sdram_next` | in | 1 for each word of a write, 8 times in total: the SDRAM controller takes the next word in the cycle after |
| `sdram_write_word[31:0]` | out | the word to write, in the cycle after `sdram_next` |

The SDRAM controller must follow these rules:

- Take each request once, with a one-cycle pulse on `sdram_take` while `sdram_req` is 1. `sdram_write`, `sdram_line` and `sdram_first` stay the same until then.
- A read gives the 8 words of the line, starting at word `sdram_first` and wrapping around inside the line. For example, with `sdram_first` = 5 the words come in the order 5, 6, 7, 0, 1, 2, 3, 4. This is the SDRAM's own sequential burst order.
- `sdram_read_valid` and `sdram_read_word` must come straight from registers. The CPU uses the word in the same cycle, in the fetch stage and in the memory stage (early restart), so there's no time for logic in front of it.
- A write asks for its 8 words with `sdram_next`, from the cycle of `sdram_take` on. Words 0 to 7 come in order, each in the cycle after its `sdram_next`.
- There may be gaps before the first word and between the words, in both directions. The CPU only gets slower.
- A miss with a changed line is a write of the old line, followed by a read request for the new one.

A read, with `sdram_first` = 5:

```text
cycle             1    2    3    4    5    6    7    8    9   10   11
sdram_req         1    0    0    0    0    0    0    0    0    0    0
sdram_take        0    1    0    0    0    0    0    0    0    0    0
sdram_read_valid  0    0    0    1    1    1    1    1    1    1    1
sdram_read_word   -    -    -   w5   w6   w7   w0   w1   w2   w3   w4
```

A write:

```text
cycle             1    2    3    4    5    6    7    8    9   10   11
sdram_req         1    0    0    0    0    0    0    0    0    0    0
sdram_take        0    1    0    0    0    0    0    0    0    0    0
sdram_next        0    1    1    1    1    1    1    1    1    0    0
sdram_write_word  -    -   w0   w1   w2   w3   w4   w5   w6   w7    -
```

How long the SDRAM controller takes for a read and a write decides what a cache miss costs. See [Programming notes](programming.md#memory).

## Bus

Every read and write outside SDRAM and the CPU-internal I/O registers goes over the bus: data reads from the ROM, the memories at `0x04000000` to `0x07FFFFFF`, and the device registers at `0xFFFF8000` to `0xFFFFFEFF` (see the [memory map](../memory-map.md)). The CPU doesn't decide which device answers. The logic behind the bus, the device bridge, decodes `bus_addr`.

| Port | Direction | Meaning |
| --- | --- | --- |
| `bus_request` | out | 1 for one cycle: a new access |
| `bus_write` | out | 1 for a write, 0 for a read |
| `bus_addr[31:0]` | out | the byte address |
| `bus_write_data[31:0]` | out | the data of a write, already shifted to its byte lanes |
| `bus_byte_enable[3:0]` | out | which bytes of the word are written: bit n for bits [8n+7:8n]. For a read, the bytes that the CPU uses. |
| `bus_ready` | in | 1 for one cycle: the access is done |
| `bus_error` | in | with `bus_ready`: nothing answers at this address |
| `bus_read_word[31:0]` | in | the word of a read, in the cycle of `bus_ready` |

The logic behind the bus must follow these rules:

- Answer every access exactly once, reads and writes, with a pulse on `bus_ready` at least one cycle after `bus_request`.
- `bus_write`, `bus_addr`, `bus_write_data` and `bus_byte_enable` stay the same from the request until the answer. A device can read them in any of those cycles.
- A read returns the whole word that contains `bus_addr`. The CPU picks the byte or halfword itself.
- A device may take as long as it needs, for example a UART that waits until it has room for the next byte. The CPU waits.
- Accesses are never speculative. Every access on the bus is made by an instruction that is certain to complete, and only once. So a device can safely do something on a read, such as taking a byte from a receive buffer.
- There is one access at a time. A new request only comes after the answer of the last one.
- An address where nothing is, such as a gap between devices, is answered too, with `bus_error` = 1 in the cycle of `bus_ready`. A read then gives 0, and a write must do nothing. The CPU sets the bad-address flag of its error register. `bus_error` is only looked at together with `bus_ready`.

The CPU never puts these on the bus: SDRAM addresses, the CPU-internal I/O registers, writes to the ROM, and addresses outside the bus's regions. The last two set a flag in the error register instead.

```text
cycle           1    2    3    4
bus_request     1    0    0    0
bus_addr        A    A    A    -
bus_ready       0    0    1    0
bus_read_word   -    -    w    -
```

This device answers 2 cycles after the request. The access costs the CPU 4 extra cycles, see [Programming notes](programming.md#the-bus).

## Interrupts

| Port | Direction | Meaning |
| --- | --- | --- |
| `interrupts[INTERRUPTS:1]` | in | bit n is interrupt n. Interrupt 1 has the highest priority. |

- A rising edge makes an interrupt pending. It stays pending until the CPU takes it, so a short pulse is enough.
- The inputs can come from any clock domain or from a pin. Each one goes through a two-flip-flop synchronizer. A pulse must be 1 for at least one `clk` cycle, and 0 again for at least one cycle before the next edge.
- An edge is seen about 3 cycles after it arrives, after the synchronizer and the edge detection.

## Retire port

The retire port reports each instruction that finishes, in program order. It's used by the testbench to compare the CPU with the instruction set simulator. In the FPGA it's left unconnected, and Quartus removes the logic that only feeds it.

| Ports | Meaning |
| --- | --- |
| `retire_valid` | an instruction finished in this cycle |
| `retire_pc`, `retire_instr` | its address and instruction word |
| `retire_writes_dreg`, `retire_dreg`, `retire_dreg_value` | the register it wrote, if any |
| `retire_write`, `retire_write_addr`, `retire_write_data`, `retire_write_be` | the memory write it made, if any |
| `retire_push`, `retire_push_value` | a push |
| `retire_pop`, `retire_pop_value` | a pop |
| `retire_redirect`, `retire_target` | a jump, taken branch or other redirect, and its target |
| `retire_irq`, `retire_irq_id` | an interrupt was taken instead of this instruction, and its number |
