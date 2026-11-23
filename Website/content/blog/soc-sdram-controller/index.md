---
title: SoC part 3 - SDRAM controller
date: 2026-11-23T02:00:00
authors:
  - name: Bart Pleiter
    link: https://github.com/bartpleiter
tags: [SoC]
---

With the FPGC's first sign of life, it still cannot run real programs as it misses a critical component: RAM. It is still not connected to the L1 cache controller, and needs a dedicated controller to interface with. That is what this post is about, including some background on the type of RAM the FPGC uses.

<!--more-->

## Why SDRAM

There are two common kinds of RAM to connect to an FPGA:

- SRAM keeps each bit in a small circuit of a few transistors. You give it an address, and a few nanoseconds later you have the data. It is simple to use, but it takes a lot of chip area per bit, so it is expensive and small.
- DRAM keeps each bit as a charge in a tiny capacitor, with one transistor to reach it. That is much smaller, so a chip holds far more, for much less money. The price is that it is more complex to use.

SDRAM is DRAM with a synchronous interface: it has a clock, and everything happens at its rising edges. On the FPGC PCB I placed two W9825G6KH-6 chips of 32 MiB each to be used as main memory. (It also has a small SRAM chip, but that is reserved for the GPU which I will cover later.) The reason for using these two chips is because these are widely available (still produced as of writing) cheap SDRAM chips with relatively high capacity per chip compared to other SDRAM chips. 64 MiB is basically overkill for a CPU with 90's performance, but having two chips has two advantages:

- It doubles the memory bandwidth for faster reads and writes of full cache lines
- It doubles the bus width from 16 bits to 32 bits, which aligns perfectly with the CPU's 32-bit architecture. This has the added benefit that the maximum burst of 8 (more details later in this post) now matches an entire cache line.

## How SDRAM works

The capacitors within DRAM leak, and reading them is destructive, which shapes everything else about SDRAM. While there are many different SDRAM architectures, I will focus on the one used by the W9825G6KH-6 chips on the FPGC PCB unless stated otherwise.

Each chip is split into 4 banks. A bank is a grid of 8,192 rows of 512 columns, and each column holds 16 bits. You cannot read a single column straight from the grid. Instead:

1. ACTIVATE opens a row: the whole row of 512 columns is copied into a buffer next to the bank (the sense amplifiers).
2. READ and WRITE then work on columns in that buffer.
3. PRECHARGE writes the row back into the grid and closes the bank, so that another row can be opened.

Each bank has its own buffer, so each bank can have one row open at a time.

The capacitors also lose their charge on their own, within milliseconds. So every row must be read and written back regularly: that is refresh. The W9825G6KH-6 needs every one of its 8,192 rows refreshed within 64 ms. An AUTO REFRESH command refreshes the next row in all four banks (the chip counts the rows itself), so the controller has to send 8,192 of them every 64 ms: one every 7.8 µs.

All of this is controlled with just four pins: chip select (CS#), RAS#, CAS# and WE#. At each rising edge of the clock, the chip looks at them as a command:

| Command | CS# | RAS# | CAS# | WE# |
| --- | --- | --- | --- | --- |
| NOP | 0 | 1 | 1 | 1 |
| ACTIVATE | 0 | 0 | 1 | 1 |
| READ | 0 | 1 | 0 | 1 |
| WRITE | 0 | 1 | 0 | 0 |
| PRECHARGE | 0 | 0 | 1 | 0 |
| AUTO REFRESH | 0 | 0 | 0 | 1 |
| MODE REGISTER SET | 0 | 0 | 0 | 0 |

(Note: you can see that CS# is not used to determine the command, but rather it is used to enable or disable the entire chip, so it is still important to set this low!)

The address pins say which row (with ACTIVATE) or which column (with READ and WRITE), and two bank pins say which bank. With a READ or WRITE, address bit 10 has a special meaning: when it is 1, the chip closes the bank by itself after the access (auto-precharge), so no separate PRECHARGE is needed.

## Two chips as one

Each chip has 16 data pins. The PCB I made for the FPGC has two chips next to each other, which share the clock, command, address and bank pins (as this saves routing and pins). One chip has the low 16 bits of each word, the other the high 16 bits. To the controller, they are one SDRAM of 64 MiB with 32-bit words.

A byte address is then split like this:

```text
byte address   [25:24]   [23:11]   [10:5]          [4:2]    [1:0]
               bank      row       column [8:3]    word     byte
                                   (the line)      in line
```

A cache line of 8 words is 8 columns next to each other in one row.

## Datasheet specs

Every step takes time inside the chip, and the datasheet says how long the controller must wait between commands. At 100 MHz, a cycle is 10 ns, so I round each time up to whole cycles:

| Rule | Datasheet | Cycles | What it means |
| --- | --- | --- | --- |
| tRCD | 15 ns | 2 | from ACTIVATE to READ or WRITE: the row must be in the buffer first |
| CAS latency | 2 cycles | 2 | from READ to the first word on the data pins |
| tWR | 2 cycles | 2 | from the last word of a write to closing the bank |
| tRP | 15 ns | 2 | from closing a bank to opening it again |
| tRAS | 42 ns | 5 | a row must stay open at least this long |
| tRC | 60 ns | 6 | from AUTO REFRESH to the next command |

The CAS latency is not fixed: it is one of the settings in the mode register, together with the burst length. At 100 MHz this chip can do a CAS latency of 2, which is the shortest it allows.

## Bursts and cache lines

A READ or WRITE does not just transfer one column, but a burst: a number of columns in a row, one per cycle. I set the burst length to 8 (the maximum according to the datasheet), which is exactly one cache line.

The nice part is the order. With a sequential burst, the chip starts at the column you give it, counts up, and wraps around inside the 8 columns. So a READ at word 5 of a line gives:

```text
word   5  6  7  0  1  2  3  4
```

This is exactly what the caches want. In the [post on early restart](/blog/cpu-early-restart), the CPU continues as soon as the word it waits for arrives, so that word should come first. The SDRAM does that for free: the controller only has to start the burst at the right column.

## Open or closed rows

The biggest choice in an SDRAM controller is what to do with a row after an access.

- Open rows: leave the row open after the burst. If the next access is to the same row, the ACTIVATE can be skipped, which saves a few cycles. If it is to another row in the same bank, the controller must close the old row first, which costs a few cycles more.
- Closed rows: close the row right after every burst, with auto-precharge. Every access then costs the same: ACTIVATE, wait, READ or WRITE.

Memory controllers in PCs go much further. They keep rows open in all banks, open a row in one bank while another bank is still bursting, and reorder the requests so that accesses to the same row are done together. That gets a lot more out of the same chips, but it is a lot of logic, and it only pays off with many requests waiting at the same time.

The FPGC has one request at a time, coming from the caches, which already keep the most used data close to the CPU. So I use closed rows: every line transfer is the same simple sequence, and the controller never has to remember which rows are open. An open row could save some cycles when code or data runs through memory line by line, as consecutive lines are in the same row. That is something to try later, with a benchmark to show whether it is worth it. Since memory will probably still be the main bottleneck in the FPGC, optimizations in the SDRAM controller could be worth it, together with L1 cache improvements.

## The controller

The controller sits between the line port of the CPU and the pins of the chips. Inside, it is a small state machine with one counter for every wait:

| State | What it does when its wait is over |
| --- | --- |
| PAUSE | the pause after a reset is done: closes every bank |
| INIT_REFRESH | sends an AUTO REFRESH, 8 times |
| INIT_MODE | sets the mode: CAS latency 2, sequential bursts of 8 |
| IDLE | sends a refresh if one is due, or else opens the row of a waiting request |
| ROW | sends the READ or WRITE, with auto-precharge |

Each command loads the counter with the number of cycles until the next command is allowed, and each state waits until the counter is 0. After a READ or WRITE, the wait covers the whole burst and the closing of the bank. That way all the rules of the datasheet are in one place: the number each command loads into the counter.

### A read

This is a read of a line from word 5, counting from the cycle in which the controller sees the request. ACT is the ACTIVATE and RD the READ. `take` tells the cache that the controller has the request, and `valid` is 1 for every word it gives:

```text
cycle        0   1   2   3   4   5   6   7   8   9  10  11  12  13  14
command      - ACT   -  RD   -   -   -   -   -   -   -   -   -   -   -
take         0   1   0   0   0   0   0   0   0   0   0   0   0   0   0
valid        0   0   0   0   0   0   0   1   1   1   1   1   1   1   1
word         -   -   -   -   -   -   -  w5  w6  w7  w0  w1  w2  w3  w4
```

- The row opens in cycle 1, and two cycles later (tRCD) the READ starts the burst at word 5.
- With a CAS latency of 2, the chip puts the first word out two cycles after the READ. It reaches a register in the FPGA a cycle after that (more on that in the next post), and the controller gives it to the cache one cycle later.
- From then on, one word per cycle, in the burst's order.

That last register is there for the CPU. The CPU uses the word in the same cycle that it arrives, in its fetch or memory stage. If the word came straight from the pins, the path from the SDRAM chip through the FPGA into the CPU's logic would be far too long for 100 MHz. With the extra register, the CPU's paths start at a register right next to it. It costs one cycle per miss, which I happily pay for a design that passes timing.

### A write

For a write, the controller has to ask the cache for the words, with `next`. The cache's word comes out of a block RAM a cycle later, goes into the register at the pin, and is on the pins the cycle after that. WR is the WRITE:

```text
cycle        0   1   2   3   4   5   6   7   8   9  10  11  12  13  14
command      - ACT   -  WR   -   -   -   -   -   -   -   -   -   - ACT
next         0   1   1   1   1   1   1   1   1   0   0   0   0   0   0
data pins    -   -   -  w0  w1  w2  w3  w4  w5  w6  w7   -   -   -   -
```

- The first `next` comes in the same cycle as the ACTIVATE. Two cycles later, word 0 is on the pins, exactly when the WRITE goes out (a write has no CAS latency: the first word goes with the command).
- After the last word, the chip needs two cycles before it closes the bank (tWR) and two more to close it (tRP). So the next request can open its row in cycle 14.

Both schedules, the `next` for a write and the `valid` for a read, are just a row of eight 1-bits that is shifted one place every cycle, started by the request. The controller itself never looks at the data.

## Refresh

The controller sends an AUTO REFRESH every 750 cycles. 64 ms divided by 8,192 rows would allow 781, but a refresh only goes between line transfers. If it is due while a write runs, it waits for the write to end, so it starts a bit early to always be on time.

A refresh takes the chips 6 cycles (tRC). A request that comes in that time waits for it, so once every 7.5 µs a miss takes a few cycles longer.

## Starting up, and every reset

After power-up, the datasheet asks for a fixed sequence before the chip can be used:

1. Wait 200 µs, with the clock running, only NOPs, and the clock enable and data masks high.
2. PRECHARGE ALL, to close every bank.
3. 8 AUTO REFRESH commands.
4. MODE REGISTER SET, to set the CAS latency and the burst.

The controller does the whole sequence again after every reset, not only after power-up. The reset can come at any moment, also in the middle of a burst. Starting again from the same sequence means that there is only one way the controller can start, which keeps it simple and easy to test. The 200 µs do not matter for a reset that already takes a moment, and the CPU starts from the ROM anyway.

During those 200 µs the chips get no refresh. That is fine: with a refresh every 750 cycles, refreshing all 8,192 rows takes 61.44 ms, so every row is still refreshed well within its 64 ms. And after a reset, the program in SDRAM is loaded again anyway.

## How to test?

We are dealing with an external chip here, so how can we test that the controller works correctly in simulation? Luckily, many vendors provide simulation models of their chips to be used in testbenches. Sadly, I could not find any model of the winbond chip I am using, but I knew that there is a Micron SDRAM model that is basically a drop in replacement which does have a verilog model provided by Micron. This is exactly what I used for validation in previous FPGC versions. While I could use it this time too, I decided to create a simpler model from it that only supports the specific commands and features I am using (and ignore things like interleaved bursts) with the goal to make simulation faster for automated testing. Usually, this would not be worth my time, but this is a very suited task for AI models to help with, as it can iterate with a closed feedback loop using the datasheet and Micron model as reference. Once the model behaves exactly like the Micron one, it was ready.

One thing worth to note is that a simulation model does not allow you to test everything needed for a proper SDRAM connection. It is important that the setup and hold times of the signals are correct too, which usually involves a phase shift on the SDRAM clock (which can easily be done using the PLLs in the FPGA). I will go into more detail on this in a future post.

## Simulation

![The SDRAM controller in GTKWave](/images/sim-sdram-controller.png "The SDRAM controller in GTKWave")

Here you can see the controller with two chip models, and a testbench that acts as the L1 cache controller:

- The initialization ends with the MODE command. Then the testbench writes a line of 8 words: the controller opens the row, asks for the words with `next`, and they go out on the data pins with the WRITE.
- Then it reads the same line back from word 5. The words come back in the burst's order: 5, 6, 7, 0, 1, 2, 3, 4.

## Compared to the previous controller

In general, the controller is the same as in the previous FPGC version: closed rows, a CAS latency of 2 and bursts of 8. What is new is how it connects to the caches. The previous controller started every burst at word 0, and gave the whole line of 256 bits at once at the end, so a miss always waited for all 8 words. The new one starts at the word the CPU needs and gives the words one by one as they come, so the CPU continues after the first. This also has the added benefit of reducing the internal data bus width between the cache controller and SDRAM, which makes routing and timings slightly easier. It also starts over cleanly at every reset, and its refresh comes a bit early instead of a bit late.

## Next steps

The controller works in simulation, also with the CPU on top of it. Before the next hardware test, I still need to configure the timings between the FPGA and SDRAM chips correctly, which is very important for a 100 MHz bus, to ensure setup and hold times are met.
