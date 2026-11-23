---
title: SDRAM controller
weight: 6
reader: hardware
---

`sdram_controller` connects the CPU's [SDRAM line port](cpu/interface.md#sdram-line-port) to the board's SDRAM: two W9825G6KH-6 chips, used together as one SDRAM of 64 MiB with 32-bit words, at 100 MHz. It transfers one cache line of 8 words at a time, as a burst of 8, and refreshes the chips in between.

```text
                 +-------------------+           +--------------------------+
 CPU line port   | sdram_controller  |  command, | U13  W9825G6KH-6         |
 ------------->  |                   |  address, | DQ[15:0], DQM[1:0]       |
 sdram_req,      |  state machine    |  bank     +--------------------------+
 sdram_line, ... |  data schedules   | --------> +--------------------------+
 <-------------  |  pin registers    |  DQ[31:0] | U14  W9825G6KH-6         |
 sdram_take,     |                   | <-------> | DQ[31:16], DQM[3:2]      |
 read words, ... +-------------------+           +--------------------------+
```

In the [top level](system.md), the controller is connected to the CPU's line port and to the board's SDRAM pins.

## The chips

| | |
| --- | --- |
| Chips | two W9825G6KH-6. U13 has the low half of each word, U14 the high half. They share the clock, the command, the address and the bank lines. |
| Each chip | 4 banks of 8,192 rows of 512 columns of 16 bits: 32 MiB |
| Together | 64 MiB, at addresses `0x00000000` to `0x03FFFFFF` |
| Mode | CAS latency 2, sequential bursts of 8 |

A byte address is split like this:

```text
byte address   [25:24]   [23:11]   [10:5]          [4:2]    [1:0]
               bank      row       column [8:3]    word     byte
                                   (the line)      in line
```

A line is 8 columns in a row, so the 8 words of a line are one burst. The burst wraps around inside the line, so a read can start at any word: a read from word 5 gives the words 5, 6, 7, 0, 1, 2, 3, 4. That's the order in which the line port asks for them.

## Parameters

| Parameter | Default | Meaning |
| --- | --- | --- |
| `PAUSE_CYCLES` | 20,000 | the pause before the initialization, in cycles: 200 µs |
| `REFRESH_CYCLES` | 750 | the cycles from one refresh to the next: 7.5 µs |

The testbenches set `PAUSE_CYCLES` shorter.

## Ports

The line port is the CPU's: `sdram_req`, `sdram_write`, `sdram_line`, `sdram_first` and `sdram_write_word` in, and `sdram_take`, `sdram_read_valid`, `sdram_read_word` and `sdram_next` out. See the CPU's [Interface](cpu/interface.md#sdram-line-port) page for the protocol.

| Port | Direction | Meaning |
| --- | --- | --- |
| `clk`, `reset` | in | the system's clock and reset |
| `sdram_cs_n`, `sdram_ras_n`, `sdram_cas_n`, `sdram_we_n` | out | the command |
| `sdram_cke` | out | the clock enable, always 1 |
| `sdram_a[12:0]` | out | the row, the column, or the mode |
| `sdram_ba[1:0]` | out | the bank |
| `sdram_dqm[3:0]` | out | the byte masks: 1 until the initialization is done, then 0 |
| `sdram_dq[31:0]` | in and out | the data |

Every output except `sdram_cke` comes straight from a register. The chips get their own copy of the clock, later than the FPGA's, so that a word read from the chips arrives in the input registers 3 cycles after the READ was sent (see [Pin timing](#pin-timing)).

## A read

Counting from the cycle in which the controller sees `sdram_req`, for a read with `sdram_first` = 5. The command and the address are what's on the pins in that cycle:

```text
cycle        0   1   2   3   4   5   6   7   8   9  10  11  12  13  14
req          1   1   0   0   0   0   0   0   0   0   0   0   0   0   0
take         0   1   0   0   0   0   0   0   0   0   0   0   0   0   0
command      -  ACT  -  RD   -   -   -   -   -   -   -   -   - (ACT) -
read_valid   0   0   0   0   0   0   0   1   1   1   1   1   1   1   1
read_word    -   -   -   -   -   -   -  w5  w6  w7  w0  w1  w2  w3  w4
```

The signals are those of the line port without `sdram_`, and the commands are ACTIVATE (ACT) and READ (RD). (ACT) is the earliest ACTIVATE of the next request.

- ACTIVATE opens the row in its bank. Two cycles later (tRCD, 15 ns), READ with auto-precharge starts the burst at the column of word 5.
- The first word is in the input registers in cycle 6, and in `sdram_read_word` in cycle 7: one more register, because the CPU uses the word in the same cycle.
- The bank closes itself after the burst (auto-precharge). The controller can send the next ACTIVATE in cycle 13. It sees the next request from cycle 12 on, while the last words of this one still arrive.

## A write

```text
cycle        0   1   2   3   4   5   6   7   8   9  10  11  12  13  14
req          1   1   0   0   0   0   0   0   0   0   0   0   0   0   0
take         0   1   0   0   0   0   0   0   0   0   0   0   0   0   0
next         0   1   1   1   1   1   1   1   1   0   0   0   0   0   0
write_word   -   -  w0  w1  w2  w3  w4  w5  w6  w7   -   -   -   -   -
command      -  ACT  -  WR   -   -   -   -   -   -   -   -   -   - (ACT)
DQ           -   -   -  w0  w1  w2  w3  w4  w5  w6  w7   -   -   -   -
```

WR is the WRITE command, and DQ the data pins.

- `sdram_next` asks for the 8 words from the cycle of the take on. Each word comes on `sdram_write_word` in the cycle after, goes into the DQ output registers, and is on the pins in the cycle after that.
- So word 0 is on the pins in cycle 3, with the WRITE command, 2 cycles after the ACTIVATE (tRCD). The other 7 words follow, one per cycle.
- After the last word, the chips need 2 cycles before the bank closes (tWR), and the closing takes 2 more (tRP). The controller sees the next request from cycle 13 on, and sends its ACTIVATE in cycle 14.

## Refresh

The chips must refresh each of their 8,192 rows every 64 ms. The controller sends an AUTO REFRESH every 750 cycles, which refreshes the next row in all four banks of both chips. 750 is a little less than the 781 that 64 ms allows, so that a refresh that has to wait for a write can still be on time.

- A refresh only comes between line transfers. A refresh that's due goes before a waiting request.
- A refresh takes 6 cycles (tRC, 60 ns). A request that arrives in that time waits up to 6 cycles longer.

## Initialization and reset

After every reset, the controller initializes the chips as the datasheet says:

| Step | Command | Wait after it |
| --- | --- | --- |
| 1 | NOP, with CKE and DQM high, for `PAUSE_CYCLES` (200 µs) | |
| 2 | PRECHARGE ALL: every bank closes | 2 cycles (tRP) |
| 3 | AUTO REFRESH, 8 times | 6 cycles each (tRC) |
| 4 | MODE REGISTER SET: CAS latency 2, sequential bursts of 8. DQM goes low. | 2 cycles (tRSC) |

Until it's done, a request waits. The CPU starts from the ROM after a reset, so this doesn't delay it unless the program uses SDRAM in its first 200 µs.

A reset can come at any moment, also in the middle of a line. The controller then goes back to the pause at once, with every pin in its idle state: NOP, DQM high and DQ not driven. DQM high also masks the words of a write that the reset cut off, so a line that was being written is partly old and partly new. The SDRAM's contents after a reset are undefined anyway.

The pause leaves the chips without a refresh for 200 µs. That's within the datasheet: with a refresh every 7.5 µs, the 8,192 rows take 61.44 ms, so each row is still refreshed within 64 ms.

## Pin timing

The FPGA and the chips run on the same 100 MHz, but the chips get their own copy of the clock, `SDRAM_CLK`, from a second output of the PLL, 7.81 ns later than the FPGA's clock. Every signal between them must arrive inside a window around the edge that takes it in:

- Commands, addresses and write data leave the FPGA's registers at the FPGA's edge, and must be at the chips at least 1.5 ns before the chips' edge (tS), and stay at least 0.8 ns after it (tH).
- A word read from the chips appears at most 6 ns after the chips' edge (tAC), and stays until at least 3 ns after the next one (tOH). The FPGA's input register must take it in between.

A later `SDRAM_CLK` gives the commands more time and the read words less. From the datasheet, the FPGA's delays and the board's trace lengths, the phase must be between about 6.0 and 9.7 ns, and 7.81 ns is close to the middle. The PLL sets the phase in steps of 312.5 ps.

How it's built so that the timing is the same in every compile:

- The PLL is at PLL 2, whose dedicated clock output is the `SDRAM_CLK` pin (B20). So the clock doesn't go through ordinary routing.
- The registers of the command, address, DQM and DQ pins are in the FPGA's I/O cells, next to the pins: the output registers, DQ's output enable (one per pin) and DQ's input registers. `SDRAM_CSn` has no register: every command selects the chips, so it's a constant 0.
- The timing constraints (`Hardware/system/fpgc.sdc`) give the timing analysis the window: a clock on the `SDRAM_CLK` pin, and the chips' setup and hold, tAC and tOH, with the shortest and longest traces of each group of pins. A word is captured 3 cycles after its READ, one cycle later than the analysis assumes by default, which the constraints say too.

So the timing analysis checks every SDRAM pin at every corner, together with the rest of the design.

## What it costs

| Number | Cycles | What |
| --- | --- | --- |
| `L` | 7 | from the cycle in which the controller sees a read request, to its first word |
| `W` | 13 | from the cycle in which it sees a write request, to the cycle in which it can see the next one |
| refresh | 6 every 750 | a request that comes during a refresh waits for it |

The [Programming notes](cpu/programming.md#memory) give what this means for a cache miss.

## The timing in cycles

All timing is in cycles of 10 ns, each from the W9825G6KH-6 datasheet, without a margin:

| Datasheet | Value | Cycles | Between |
| --- | --- | --- | --- |
| tRCD | 15 ns | 2 | ACTIVATE and READ or WRITE |
| tRP | 15 ns | 2 | the start of a precharge and the next ACTIVATE |
| tRC | 60 ns | 6 | AUTO REFRESH and the next command |
| tWR | 2 cycles | 2 | the last word of a write and the precharge |
| tRSC | 2 cycles | 2 | MODE REGISTER SET and the next command |
| tRAS | 42 ns | (5) | ACTIVATE and the precharge. Always met, because the burst of 8 takes longer. |

## How it works

The controller is one state machine with one wait counter, and two shift registers that schedule the data.

| State | What it does when the wait is over |
| --- | --- |
| `PAUSE` | sends PRECHARGE ALL |
| `INIT_REFRESH` | sends AUTO REFRESH, 8 times |
| `INIT_MODE` | sends MODE REGISTER SET |
| `IDLE` | sends AUTO REFRESH if one is due, or else an ACTIVATE for a waiting request, and takes it |
| `ROW` | sends the READ or the WRITE, with auto-precharge, and goes back to `IDLE` |

- Each command loads `wait_count` with the cycles to the next command, and every state waits until it's 0. After a READ or a WRITE, the wait covers the burst, tWR and tRP, so `IDLE` only sends the next ACTIVATE when the bank is closed again.
- `refresh_count` counts down from 750 after each AUTO REFRESH. At 0, a refresh is due.
- `asking` is 8 bits of 1, loaded at the take of a write and shifted every cycle: its lowest bit is `sdram_next`. The words follow it to the pins by 2 cycles, the same as tRCD, so the first word arrives with the WRITE.
- `reading` is 8 bits of 1 behind 4 bits of 0, loaded with the READ and shifted every cycle: its lowest bit is `sdram_read_valid`. The controller never looks at the words themselves.

## Design rules

- Every output, the line port's and the pins', comes straight from a register, with no logic after it (`sdram_cke` is a constant). So the pin registers can be in the FPGA's I/O cells, and the CPU's paths don't start at the SDRAM's pins.
- The read word goes through two registers: the input register at the pin, and `sdram_read_word`. The CPU uses `sdram_read_word` in the same cycle, in its fetch and memory stages.
- The data path runs every cycle without a condition: the input registers follow DQ, and the output registers follow `sdram_write_word`. The schedules decide when a word counts.
- The cycle counts are constants for 100 MHz. Another clock needs other numbers. `READ_DELAY`, the 4 cycles from the READ to `sdram_read_word`, depends on the phase of the chips' clock, and so do the constraints of the pins.
- The pins' registers must stay in the I/O cells: nothing else may read the output registers, and the input registers may only feed `sdram_read_word`.

## Files

The controller is `Hardware/sdram/sdram_controller.v`. The phase of the SDRAM's clock is in `Hardware/system/system_pll.v`, the pins' I/O cell registers in `Hardware/system/pins.qsf`, and their constraints in `Hardware/system/fpgc.sdc`.
