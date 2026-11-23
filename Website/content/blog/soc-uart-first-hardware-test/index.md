---
title: SoC part 2 - UART and first sign of life
date: 2026-11-23T01:00:00
authors:
  - name: Bart Pleiter
    link: https://github.com/bartpleiter
tags: [SoC]
---

The last part that prevents me from testing the FPGC on real hardware is the UART controller. UART is needed to communicate with in this case my PC so I can validate that the CPU works and does not lock up somewhere. While the SDRAM controller, and by extension the SDRAM itself, is still not connected, with UART I can already test a simple ROM program that allows me to validate that the CPU is actually working in hardware instead of just in simulation. So this post is about building the UART controller and seeing the first sign of life on real hardware.

<!--more-->

## What is UART

A UART (Universal Asynchronous Receiver-Transmitter) sends bytes one bit at a time over a single wire, and receives them on another. It is one of the oldest and simplest ways for two devices to talk, and almost every microcontroller has one. For the FPGC, it allows it to communicate with another device (a host PC usually), and later will be used to load programs into the FPGC using a bootloader.

"Asynchronous" means that there is no clock wire. Both sides agree on the speed beforehand, the baud rate: the number of bits per second. Each byte then goes over the line like this:

```text
line   idle  start  bit0 bit1 bit2 bit3 bit4 bit5 bit6 bit7  stop  idle
       1 1 1   0     0    0    0    1    0    0    1    0     1    1 1 1
```

This is the letter `H` (`0x48`, `01001000` in binary). The line is high when nothing is sent. A byte starts with a start bit, which is always 0, so the receiver sees the line fall. Then come the 8 data bits, lowest bit first, and a stop bit, which is always 1, so the line is high again before the next start bit. That makes 10 bits per byte.

## The speed: 1 Mbaud

On the FPGC board, the FPGA's UART pins go to a CH340C, a small chip that turns the serial line into USB, allowing connections to most modern devices like PCs. The CH340C makes its baud rate from its own 12 MHz clock by dividing it, and the FPGA makes it from its 100 MHz clock. So the useful baud rates are the ones that both can make exactly:

| Baud rate | CH340C | FPGA at 100 MHz |
| --- | --- | --- |
| 115,200 | 12 MHz / 104.17, not exact | 100 MHz / 868.06, not exact |
| 921,600 | not exact | not exact |
| 1,000,000 | 12 MHz / 12, exact | 100 MHz / 100, exact |
| 2,000,000 | 12 MHz / 6, exact | 100 MHz / 50, exact |

A small difference in speed is fine, as the receiver starts counting again at every start bit, but exact is better. I use 1 Mbaud, which I also used in the previous FPGC versions (which ran at 50 and 25 MHz): one bit takes 100 clock cycles, and one byte 10 µs. That is 100 KB per second, which is plenty for now. 2 Mbaud might be something to try later.

## The transmitter

Sending is the easy part. The transmitter has a shift register with the 10 bits of the byte (start bit, data, stop bit), and a counter that counts 100 cycles per bit. Every 100 cycles, the next bit goes out on the line. When all 10 bits are out, it takes the next byte, if there is one.

## The receiver

Receiving is harder, because the receiver has to find the bits in a line that is not in step with its own clock. Also, in contrast to SPI or I2C, there is no separate clock line that tells the receiver when to sample. I ended up doing the following:

1. The line first goes through a synchronizer: two flip-flops in a row, to stabilize the incoming signal from a different clock domain. The line can change at any moment, and this makes sure the rest of the receiver only sees a clean 0 or 1 (See previous post for more details).
2. When the line falls, a byte might start. The receiver waits half a bit (50 cycles), and looks at the line again, in the middle of the start bit. If it is high again, it was just a short glitch, and the receiver goes back to waiting.
3. From there, it waits a whole bit (100 cycles) at a time, and samples each data bit in its middle.
4. Then it checks the stop bit. If the stop bit is 1, the byte is good. If it is 0, something went wrong: maybe the receiver started in the middle of a byte. That is called a framing error. The byte is dropped, and a flag is set that software can read.

Counted in clock cycles from the moment the line falls:

```text
cycle      0      50     150    250    350   ...   850    950
           |      |      |      |      |            |      |
           fall   start  bit 0  bit 1  bit 2        bit 7  stop
                  check
```

Sampling in the middle gives the most room for error: the receiver can be almost half a bit off at the end of the byte and still read every bit right.

There is one more thing it does. After a reset, and after a framing error, the receiver waits until the line has been high for a whole byte (10 bits) before it accepts a new start bit. That way it never starts in the middle of a byte that was already on its way. It also means a line that stays low, for example when the CH340C is not ready for whatever reason, gives nothing at all instead of a stream of garbage.

There are fancier ways to receive. The 16550, the UART chip in old PCs, takes 16 samples per bit and uses a majority vote of the middle ones, so a short spike in the middle of a bit cannot flip it. That makes sense for a long serial cable next to a noisy motor. On the FPGC, the line is a short trace on the same PCB to a stable IC, so one sample in the middle is probably enough.

## Buffers

At 1 Mbaud, a byte arrives every 10 µs: 1,000 cycles, and when a byte arrives, the CPU will get an interrupt to handle it. As long as you do not spend a long time in the interrupt handler (which is bad design anyway), this should give the CPU enough time to keep up with the incoming data before it is overwritten by the next byte. The previous FPGC version was able to make this work quite well, but it does require careful handling.

In an FPGA, a buffer in hardware is cheap, and to reduce the need for careful handling the UART gets a buffer of 32 bytes in each direction, just like many microcontrollers get a UART buffer. The buffer is implemented as a FIFO: a small memory with a write pointer and a read pointer. The receiver writes at the write pointer and moves it on, the CPU reads at the read pointer and moves that on, and both wrap around at the end. When the pointers are equal, the FIFO is empty. To tell "full" apart from "empty" (in both cases the pointers point at the same place), each pointer has one extra bit that flips at every wrap-around: when the addresses are equal but the extra bits differ, the writer is a whole round ahead, and the FIFO is full.

32 bytes gives a program 320 µs to get to the received bytes. Practically, that is more than enough time, especially since an interrupt is triggered for each byte, allowing the CPU to just copy the buffer into memory before it overflows without needing to handle logic for each byte. In hindsight, I could have connected the CH340C's CTS pin to the FPGA to implement hardware flow control (using CTS and RTS the host and device can communicate if they are ready for another byte), but since most development boards (like an Arduino or ESP32) do not have it connected, I also did not do it as I never really needed it. Worst case, I could increase the buffer to a huge size using an M9K block in the FPGA.

The transmit buffer works the other way around: a program writes bytes into it, and the transmitter sends them one by one. A program can write a whole line of text at once and go on with its work. When the buffer is full, a write simply waits/stalls until there is room again, so a program does not have to check anything before it writes. On modern PCs, it is also not possible as far as I know to overflow the receive buffer, so lacking the CTS pin does not matter for transmission.

## The registers

| Address | Register | A write | A read |
| --- | --- | --- | --- |
| `0xFFFF8000` | TRANSMIT | puts a byte into the transmit buffer, waits while it is full | 0 |
| `0xFFFF8004` | RECEIVE | nothing | takes the oldest byte from the receive buffer, or 0 when it is empty |
| `0xFFFF8008` | STATUS | clears the two error flags | bit 0: a byte is waiting. Bit 1: the transmit buffer is full. Bit 2: a byte was lost, the buffer was full. Bit 3: a framing error. |
| `0xFFFF800C` | CONTROL | bit 0: enable the interrupt | the same bit |

The interrupt follows the method I described in a previous post: its flag is "a byte is waiting", and with the enable bit set, the interrupt stays up as long as there is a byte in the buffer. The handler reads bytes until the buffer is empty.

As you can see, the UART design is relatively complex with its buffering, interrupt handling, and error detection mechanisms, but I wanted to improve on the bare minimal design of the previous FPGC versions, since those occasionally had some unexplainable issues with receiving a program in the UART bootloader after a reset, and designing the UART bootloader was also quite tricky without insights into missed bytes or interrupts.

## Simulation

![The UART receiver in GTKWave](/images/sim-uart-rx.png "The UART receiver in GTKWave")

Here you can see the receiver on its own:

- After the reset, it waits until the line has been high for a whole byte.
- Then `H` and `i` come in. You can see the bits shift into `data` one by one, and `valid` goes high for one cycle at the end of each byte.
- Then a short low glitch: the receiver checks the line in the middle of the start bit, sees that it is high again, and ignores it.
- The last byte has no stop bit, so it is dropped as a framing error, and the receiver waits for a quiet line again.

![The UART in GTKWave](/images/sim-uart.png "The UART in GTKWave")

And here the whole UART: `Hi` goes into the transmit buffer and out on the tx line, while a `!` comes in on the rx line. When the interrupt is enabled, it goes up because a byte is waiting, and down again when the byte is read.

## First sign of life

With UART done, I finally have everything ready for testing the CPU on real hardware. Especially since there is no GPU yet, and therefore no visual output over the HDMI port, having a working UART controller allows me to get feedback from the CPU over the serial connection. While the SDRAM controller is also still to be done, I can just tie off its signals and test a ROM only program. This means a Top Level Design (TLD) was ready to be made, which contains the following:

- the CPU, with a 4 KiB ROM inside the FPGA for its program
- the device bridge, with the UART controller, timer, cycle counter and board register
- a PLL that makes the 100 MHz clock from the board's 50 MHz oscillator
- a reset from three sources: reset button, the PLL (until its clock is stable), and from the CH340C control signals (like an ESP32 does)
- every other pin of the FPGA in a safe state, making sure not to potentially damage the other ICs connected on the board

### Test program

The ROM program is small. It prints the contents of the error register and a ready message over UART:

```text
boot: err 00000000 pc 00000000
boot: ready
```

and then sends back every byte it receives. As mentioned in a previous post, the CPU's error register survives a reset. When a program crashes or misbehaves and I press reset, this line tells me what went wrong and where, which hopefully provides useful debugging information.

### Resetting over USB

Like previous FPGC versions, I want to be able to reset the board from the host PC, so I do not have to physically press the reset button each time I want to program the FPGC. The serial port has two extra signals for this, DTR and RTS, which the CH340C passes to the FPGA. I got inspiration from microcontroller boards to implement this feature, as they want to achieve the same convenience.

Arduino boards reset when DTR goes low. The problem with that: every program that opens the serial port sets DTR, so just opening a serial monitor resets the board, and whatever was running is gone. This is how I implemented the reset in previous FPGC versions as it is easy to implement: just a single edge detector on the DTR line.

ESP32 boards solve it differently, and this time I followed the same design. When a program opens the port, DTR and RTS change together. So the board only resets on a combination that a program has to make on purpose: RTS on and DTR off. Because the two lines may change a few nanoseconds apart when the port opens, the combination has to last 1 ms before the board resets.

This way, I can now just open a serial monitor at any time without accidentally resetting the board.

### First hardware-discovered bug

Sadly, the first boot after programming did not look right (although there could have been much worse problems! So I am actually quite happy with this first result):

```text
?????  err 00000000 pc 00000000
boot: ready
```

The first five characters, `boot:`, arrived as garbage. Everything after it was fine, and after a reset from the PC, both lines were perfect. In simulation, this did not occur.

The cause was the very first moment after the FPGA is configured. The tx line comes from a register in the transmitter, and that register only got its 1 (the idle level) from the reset, at the first clock edge. But right after configuration, every register starts at 0, and the PLL takes a moment before the first clock edge comes. So for that moment, the tx line was low.

For the CH340C, a long low line looks like a very long start bit. Then the boot text followed right after the reset, byte after byte without a pause. The CH340C had lost track of where the bytes start, and picked the wrong edges as start bits, until it fell back into step a few bytes later. After a reset from the PC, the line had been high for a long time before, which is why that case was fine. And the simulations started with the line high, so it never saw this either. This shows that simulations are not perfect, as they typically miss points like:

- What happens before the FPGA design is running
- What happens during metastable moments like cross domain crossings
- What happens outside of the FPGA, like the behavior of connected chips and things like trace length delays

In this case, the fix was simple by initializing the tx register to 1 outside of the reset logic that depends on the clock to be running. Technically, this could have been detected in simulation though, if I modeled the startup behavior a bit better and not just immediately assert a reset, but honestly this is a typical kind of issue that only shows up on real hardware testing.

## Next steps

The CPU can only run the 4 KiB of ROM so far, and only use its registers to store temporary data. To allow actual programs to run, the FPGC needs RAM, and for this there is 64 MiB of SDRAM present on the PCB. Next post will be about getting that connected to the FPGC.
