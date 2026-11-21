---
title: UART
weight: 1
reader: programming
---

The UART is the serial port to the PC, through the USB serial chip on the board (a CH340C). Programs use it to print text and to read what's typed on the PC.

## The line

- 1,000,000 baud, 8 data bits, no parity, 1 stop bit. A terminal on the PC needs the same settings.
- One byte takes 10 µs on the line, which is 1,000 cycles.
- There is no flow control. The PC can send faster than a program reads, so a program that receives a lot must keep up, or tell the PC when to send.
- Each direction has a buffer of 32 bytes.

## Registers

| Address | Register | Read | Write |
| --- | --- | --- | --- |
| `0xFFFF8000` | TRANSMIT | 0 | bits [7:0] go into the transmit buffer. Waits while the buffer is full. |
| `0xFFFF8004` | RECEIVE | takes the oldest byte from the receive buffer, or gives 0 when it's empty | ignored |
| `0xFFFF8008` | STATUS | the bits below | clears bits 2 and 3 |
| `0xFFFF800C` | CONTROL | bit 0: the interrupt is enabled | bit 0: enable the interrupt (1) or not (0) |

| STATUS bit | Meaning |
| --- | --- |
| 0 | a byte is waiting in the receive buffer |
| 1 | the transmit buffer is full |
| 2 | a byte arrived while the receive buffer was full, and was lost. Stays 1 until STATUS is written. |
| 3 | a byte arrived without its stop bit, and was dropped. Stays 1 until STATUS is written. |

A 0 from RECEIVE can also be a received 0 byte. A program that receives binary data checks bit 0 of STATUS first.

## Sending

A write to TRANSMIT puts the byte in the transmit buffer, and the program goes on. The UART sends the bytes from the buffer one after the other.

- 33 bytes fit before a write has to wait: 32 in the buffer, and one that's being sent.
- A write to a full buffer waits until there's room again, at most one byte time (10 µs). So a program can write as fast as it likes, and doesn't have to check the status. Bit 1 of STATUS is there for a program that wants to do something else instead of waiting.
- After the last write, the buffer may still be sending, for up to 330 µs. A reset empties the buffer, so bytes that are still in it are lost.

Printing a text that's stored one character per word, ending with a 0:

```text
    addr2reg Text r1
Next:
    read 0 r1 r2
    beq r2 r0 Done
    write 0xFFFF8000 r0 r2      ; TRANSMIT
    add r1 4 r1
    jump Next
Done:
```

## Receiving

A program can poll for bytes, or get an interrupt.

Polling: wait until bit 0 of STATUS is 1, then read RECEIVE.

```text
Wait:
    read 0xFFFF8008 r0 r1       ; STATUS
    and r1 1 r1
    beq r1 r0 Wait              ; nothing is waiting yet
    read 0xFFFF8004 r0 r2       ; RECEIVE: the byte
```

With the interrupt: write 1 to CONTROL, and interrupt 2 comes when a byte is waiting. The handler reads RECEIVE until bit 0 of STATUS is 0, as in the example on the [I/O devices](_index.md#interrupts) page. A byte that arrives while the handler runs is read by the same loop, or gives a new interrupt after `reti`.

- The receive buffer holds 32 bytes: 320 µs of bytes sent back to back. A byte that arrives when it's full is lost, and sets bit 2 of STATUS.
- A read of RECEIVE that takes a byte costs 5 extra cycles instead of 3: the device bridge first takes the byte from the buffer, which is a small RAM that needs a cycle to read.

## How the receiver reads the line

- Each bit is sampled once, in its middle, counted from the start of the byte. So the PC's baud rate may differ a few percent from the FPGC's. At 1 Mbaud, both sides are exact.
- A low pulse on the line that's shorter than half a bit is a glitch, and is ignored.
- After a reset, and after a byte without its stop bit, the receiver waits until the line has been high for a whole byte (10 µs) before it accepts a new byte. So it never starts in the middle of a byte. Bytes that the PC sends without a pause in that moment are lost, until the line has been quiet for 10 µs.
- A line that stays low, for example when the USB serial chip has no power, gives nothing.

## After a reset

Both buffers are empty, STATUS is 0, and the interrupt is disabled. A byte that was being sent is cut off.

The transmitter then keeps the line high for one byte time (10 µs) before it sends the next byte, so the PC's side always sees a cut-off byte end before a new one starts. After the FPGA is configured, the line is high from the start.
