---
title: SoC part 4 - SDRAM hardware test
date: 2026-11-23T03:00:00
authors:
  - name: Bart Pleiter
    link: https://github.com/bartpleiter
tags: [SoC]
---

The SDRAM controller from the previous post works in simulation, but a simulation does not know anything about what happens between the pins of the FPGA and the pins of the SDRAM chips. At 100 MHz, a cycle is only 10 ns, and a signal that leaves the FPGA needs to be stable at the SDRAM pin when it samples, and vice versa. This post is about getting that part right: the timing between two chips on a PCB, and how I calculated it instead of finding it by trial and error. And at the end, the FPGC finally runs programs from SDRAM.

<!--more-->

## Inside and outside the FPGA

Inside the FPGA, Quartus (the build tool from Intel that must be used to go from design to FPGA bitstream/configuration) knows everything: where each register is placed, how long every wire is, and how late the clock arrives at each register. After every compile, it checks that every path between two registers is fast enough for 100 MHz, and tells you if one is not.

At the pins, Quartus knows only half of the story. It knows how long a signal takes from a register to the pin, but not what is on the other side: how long the trace on the PCB is, and when the chip on the other end wants to see the signal. That part I have to tell it, and if I tell it nothing, it simply does not check it. The design then compiles without any warning but might not work.

## Setup and hold

Every register, inside the FPGA or inside the SDRAM chip, takes its input at the rising edge of its clock. For that to work, the input has to be stable for a short moment around the edge:

- setup time: how long before the edge the input must already be stable
- hold time: how long after the edge the input must stay stable

```text
                      edge
clock    _______________/----------------
input    xx|=========================|xxxx
           |<- setup -> | <- hold -> |
```

For the W9825G6KH-6, the datasheet gives a setup time of 1.5 ns and a hold time of 0.8 ns for every input: commands, addresses and data to write. So every signal from the FPGA has to arrive at least 1.5 ns before the SDRAM's clock edge, and must not change until 0.8 ns after it.

Reads work the other way around. The SDRAM puts a word on its data pins after its clock edge, and the FPGA has to catch it. The datasheet says when the word is there:

- tAC: the word is on the pins at most 6 ns after the SDRAM's clock edge
- tOH: the word stays there until at least 3 ns after the next edge

So the FPGA's input register has to take the word somewhere in that window, with its own setup and hold time around it.

## A clock for the SDRAM

The SDRAM chips do not use the FPGA's internal clock directly. They get their own copy of the 100 MHz clock on a separate pin, `SDRAM_CLK`, and the PLL in the FPGA can make that copy with a phase shift: the same clock, but a bit later. That shift is the knob that makes everything fit:

- A later SDRAM clock gives the writes more time: the commands, addresses and data have more time to travel from the FPGA to the chip before the chip takes them.
- But a later SDRAM clock also makes the chip put its read data out later, so the reads get less time to reach the FPGA before the FPGA takes them.

So writes want a late clock, and reads want an early one. Somewhere in between is a range where both work, and the best phase is in the middle of that range, where the margin to both sides is the same.

## Where the delays come from

To find that range, I needed three kinds of numbers.

The first is the datasheet of the SDRAM: the setup, hold, tAC and tOH from above.

The second is the FPGA itself: how long a signal takes from an output register to the pin, how long the clock takes from the PLL to the `SDRAM_CLK` pin, and the setup and hold of the input registers. Quartus knows these, and they are not just one number each. Chips are not all exactly the same, and they get slower when they get warm. So Quartus gives the delays for a few corners: a slow chip at 100 °C, a slow chip at -40 °C, and a fast chip at -40 °C. The design has to work in every corner. To give an idea, a signal from an output register to its pin takes 4.2 to 4.8 ns in the slow, hot corner, but only 2.1 to 2.5 ns in the fast, cold corner.

The third is the PCB. A signal on a trace of FR4 takes about 5.5 ps per mm on the outer layers, and about 7 ps per mm on the inner layers. As I already designed the PCB, I could just measure them within EasyEDA (the tool I used to design the PCB):

| Nets | Length |
| --- | --- |
| `SDRAM_CLK` | 26 mm to the first chip, 41 mm to the second |
| address, bank and command | 28 to 55 mm, first to one chip, then on to the other |
| data and data masks | 6 to 27 mm, point to point |

That makes 0.03 to 0.38 ns. Small compared to the 10 ns of a cycle, but these fractions of a nanosecond are exactly what this calculation is about.

![SDRAM traces of the FPGC board (inner layers hidden)](/images/pcb-sdram-traces.png "SDRAM traces of the FPGC board (inner layers hidden)")

## The window

With all those numbers, for each corner, the range of phases where the writes work and the range where the reads work:

```text
                      0       2       4       6       8       10      12  ns
                      |-------|-------|-------|-------|-------|-------|
slow, 100 °C writes                     =========================
slow, 100 °C reads                         =======================
slow, -40 °C writes                   ==========================
slow, -40 °C reads                          ========================
fast, -40 °C writes               ============================
fast, -40 °C reads                            ========================
every corner                                  ################
chosen                                               ^ 7.81
```

- The writes need a phase of at least 3 to 4.5 ns, because a signal needs that long to get out of the FPGA, over the trace, and to the chip 1.5 ns before its edge. The fast corner allows the earliest phase, because there the FPGA's outputs are fastest.
- The reads need a phase of at least 5.2 to 6 ns, so that the chip does not change its word before the FPGA has taken it. Here the fast corner is the strictest one.
- Where every line overlaps, from 5.98 ns to 9.73 ns, everything works in every corner. The middle is 7.86 ns.

A shift of 7.86 ns is almost a whole cycle, which might look strange. But a clock is a repeating signal: 7.86 ns later is the same as 2.14 ns earlier. So in practice, the SDRAM's clock comes a bit before the FPGA's.

The PLL cannot make every phase. It shifts in steps of 312.5 ps (an eighth of the period of its internal 400 MHz oscillator), so the closest phase is 25 steps: 7.81 ns. With that phase, every signal has about 2 ns of margin on both sides, in every corner. That is a comfortable margin at 100 MHz, and enough to cover what this calculation does not know exactly, like the jitter of the clock, the exact delay of each trace, or that the datasheet specifies the chip's timing with a heavier load than my short traces.

In the previous FPGC version, I used a phase of 7.5 ns, which I found by trial and error. I setup a small test program that writes and reads from the SDRAM and prints the results over UART for me to read. I then recompiled the entire design with different degrees of phase shift, which actually gave me a quite good view of the window of valid phases for my specific chips used and board layout: below a certain phase I would get increasingly wrong data, and above a certain phase it would also fail. I then took the center of that window, which ended up at 7.5 ns (compilation time is long, so I used course values) which matches the calculation quite well. Two different approaches, both work well!

## Reading one cycle later

One more thing follows from the window. Because the SDRAM's clock is almost a whole cycle late compared to the FPGA's clock, a word that the SDRAM puts out arrives at the FPGA a few nanoseconds after the FPGA's next clock edge. So the FPGA does not take it at that edge, but at the one after it, 12.2 ns after the SDRAM's edge. In the read example of the previous post, that is why the word reaches the register in the FPGA a cycle after the chip puts it out.

Quartus normally assumes that a signal is taken at the first edge after it was sent, so this has to be told explicitly. Otherwise it would check the reads against an edge that the design does not use, and report them as failing.

## Telling Quartus

All of this goes into the constraints file of the design (the SDC file), so that Quartus checks the SDRAM's pins after every compile, just like every path inside the FPGA:

- The `SDRAM_CLK` pin is a clock, made by the PLL, with its phase.
- For every output pin to the SDRAM, the latest and earliest moment the signal may arrive, relative to the SDRAM's clock. These come from the chip's setup and hold and the trace lengths.
- For the data pins as inputs, the latest and earliest moment the SDRAM's word arrives, from tAC, tOH and the trace lengths.
- The reads are taken one edge later, as described above.

Quartus then calculates the margin of every SDRAM pin in every corner, with the real design around it. Its numbers came out a little better than my calculation, which took the worst case of everything at the same time: at least 2.08 ns everywhere. If I ever change the design in a way that breaks it, the compile tells me.

In previous FPGC versions I only defined the main clock and to derrive other clocks from the PLL config. This worked, but now having a proper SDC file allows me to better capture timing issues once my design grows larger and more complex.

## Good practices

Besides the constraints, a few choices make the timing at the pins reliable:

- Registers in the I/O cells. Every pin of the FPGA has a few registers right next to it. When a signal to the SDRAM comes from such a register, its delay to the pin is short, and the same in every compile. When it comes from a register somewhere in the middle of the FPGA, the delay depends on where Quartus happens to place it, and the timing at the pin can change with every unrelated change to the design. So every SDRAM pin has its output register, and for the data pins also its input register and output enable register, in the I/O cell. (Except chip select: the controller keeps it low all the time, so there is no register to place.) The SDRAM controller's design is made for this: every signal to the pins comes straight from a register, without logic after it.
- A dedicated clock output. A PLL has dedicated pins that it can drive directly, with a short and fixed delay. `SDRAM_CLK` is on such a pin, of the FPGA's second PLL. However, when I designed the PCB I did not know I also had to use the clock input pin in the same IO bank as the PLL, so now it sits on the other side of the FPGA, connected to another PLL. So one of the two has to go through the FPGA's ordinary routing. I chose to route the oscillator's input to the PLL that way, and keep the dedicated output for `SDRAM_CLK`. A slightly later input clock shifts all of the PLL's outputs together, so the relation between the FPGA's clock and the SDRAM's clock, which is what matters here, stays the same. The other way around would put the routing delay between the two clocks. Honestly, it probably does not matter too much at these speeds (my design worked fine in previous FPGC versions with the same issue), but good to know for the future.
- Similar trace lengths. Within a bus, the traces should be about the same length, so that the signals arrive at the same time and the window is the same for all of them. On the FPGC PCB, the lengths differ a bit, but at 100 MHz, a few centimeters are only a few tenths of a nanosecond, so it does not matter that much. I knew about this beforehand, but compacting the design (trace length matching costs extra space on the PCB) was more important.
- Constraints for every pin. Without them, Quartus does not check anything at the pins. For slow signals this does not matter, but this could have been the reason that I never was able to get high speed SPI flash working before.

## Second hardware test

With the SDRAM pins constrained and the timing met, it was time for the second hardware test. To use the SDRAM, a program has to get into it first, and for this the ROM program from the previous hardware test grew into a small bootloader that receives a program over UART, writes it to SDRAM, and runs it. How that works, I will describe in a future post, probably in a new series about the toolchain, where I can also go into more detail on the assembly language and assembler I have been using, since those are also custom for this project.

After seeing a hello world program from SDRAM, the majority of the L1 cache and SDRAM controller functionality had been proven!

### Memory test

A hello world only uses a few words of the SDRAM, so the next program was a memory test. It goes over the whole SDRAM after its own first 64 KiB in three passes: first every word gets its own address as its value, then its address inverted, and then a pseudo-random number. Each pass first writes every word, then writes the caches back to the SDRAM, and then reads every word back and compares it. The address patterns catch address lines that are stuck or connected to each other, as two addresses would end up with the same word. The random pass catches data bits that influence each other, and is different from anything that was in the memory before. This is the typical memtest procedure that is used a lot, so it was a fun thing to have implemented here too.

No errors! That means the design works correctly and is ready for proper programs to be run from SDRAM. I also tested some edge cases where I reset the board mid-run, and it was nice to see that the FPGC was able to recover from that properly.

```text
memtest: pass 00000000 done, errors 00000000
memtest: pass 00000001 done, errors 00000000
memtest: pass 00000002 done, errors 00000000
memtest: done, errors 00000000
```

## Next steps

The FPGC now has a CPU, a ROM, 64 MiB of working SDRAM and a UART, which makes it a complete (although still minimal) computer: definitely a milestone! The next posts will be about how programs for it are written and loaded, starting with the assembly language, in a new Toolchain series. The SoC series will continue when new hardware is added, and at some point the GPU should also be brought to life so the FPGC can actually have a display output.
