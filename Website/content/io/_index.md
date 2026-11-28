---
title: I/O devices
weight: 3
reader: programming
---

The devices that software reaches through registers: the UART, the timer, the cycle counter, the board register with the user LED and the DIP switches, and the SPI buses. They sit behind the device bridge on the CPU's bus, in the device registers of the [memory map](../memory-map.md).

## The devices

| Number | Address | Device | Interrupt | Page |
| --- | --- | --- | --- | --- |
| 0 | `0xFFFF8000` | UART | 2 | [UART](uart.md) |
| 1 | `0xFFFF8100` | cycle counter | none | [Timer and cycle counter](timer.md) |
| 2 | `0xFFFF8200` | timer | 1 | [Timer and cycle counter](timer.md) |
| 3 | `0xFFFF8300` | board: user LED and DIP switches | none | [below](#board-register) |
| 4 to 9 | `0xFFFF8400` to `0xFFFF8900` | SPI buses: flash 1, flash 2, USB 1, USB 2, Ethernet, the SD card | none | to come |

Device n has the 256 bytes from `0xFFFF8000` + 256 × n. A device that's added later gets the next number.

## Rules for the registers

- Every register is a 32-bit word at a multiple of 4, and only `read` and `write` reach it. A byte or halfword access to a device register is a bad address.
- A register that the device doesn't have, and a device number without a device (10 to 126), are bad addresses too. A bad read gives 0 and a bad write does nothing, and both set the bad-address flag of the [error register](../cpu/isa.md#error-register).
- Bits that a register doesn't use read as 0, and writes to them are ignored.
- With r0 as the base register, every device register is one instruction away. The assembler takes the address as the offset:

  ```text
  write 0xFFFF8000 r0 r1      ; send the low byte of r1 over the UART
  read 0xFFFF8304 r0 r2       ; read the DIP switches
  ```

- A read can have an effect, such as taking a byte from the UART's receive buffer. That's safe, because the CPU makes each access exactly once, and only for an instruction that completes.
- An access costs 3 extra cycles, like every bus access that's answered in the next cycle (see [Programming notes](../cpu/programming.md#the-bus)). The exceptions are on the UART page: a read that takes a received byte, and a write to a full transmit buffer.

## Interrupts

Every device that can interrupt works the same way:

- A flag in its status register says that something happened: a byte is waiting in the UART, or the timer expired.
- An enable bit in its control register lets the flag interrupt. It's 0 after a reset.
- The device's interrupt output is "flag and enable". The CPU makes the interrupt pending when that output goes from 0 to 1.
- The handler clears the cause: it reads the UART until nothing is waiting, or writes the timer's status register. Until then, the output stays 1, so the device can't interrupt again.
- Enabling the interrupt while the flag is already set gives an interrupt right away.

So a handler must always clear the cause before `reti`. If it doesn't, the output never goes back to 0, and that device never interrupts again.

Interrupt 1 is the timer and interrupt 2 the UART. When both are pending, the CPU takes 1 first, and 2 right after the `reti`. The CPU's side of interrupts (where they're taken, `readintid`, `reti`, enabling them with `INT_ENABLE`) is in the [Programming notes](../cpu/programming.md#interrupts).

A handler for both devices:

```text
    jump Main
    jump Interrupt              ; address 0x4
Interrupt:
    push r1
    push r2
    readintid r1
    load 1 r2
    bne r1 r2 Uart
    write 0xFFFF8208 r0 r0      ; the timer: clear its flag
    ...                         ; the timer's work
    jump Done
Uart:
    read 0xFFFF8004 r0 r1       ; take a received byte
    ...                         ; use it
    read 0xFFFF8008 r0 r2
    and r2 1 r2
    bne r2 r0 Uart              ; until nothing is waiting
Done:
    pop r2
    pop r1
    reti
```

## Board register

The user LED and the four DIP switches.

| Address | Register | Read | Write |
| --- | --- | --- | --- |
| `0xFFFF8300` | LED | bit 0: the user LED | bit 0: turns the user LED on (1) or off (0) |
| `0xFFFF8304` | SWITCHES | bits [3:0]: the four DIP switches | ignored |

The switches are read through a synchronizer, so a change shows up in the register two cycles later.

The user LED is the only LED that software controls. The board's other LEDs show what the hardware does, such as the UART's lines, and software can't change them (see [System](../system.md#leds)).

## After a reset

- Every register is 0: the user LED is off, the timer is stopped, and every interrupt is disabled.
- The UART's buffers are empty.
- The cycle counter starts again at 0.

## Pages

| Page | For | What it answers |
| --- | --- | --- |
| [UART](uart.md) | programmers | how to send and receive bytes over the serial port |
| [Timer and cycle counter](timer.md) | programmers | how to measure time and get an interrupt after a number of cycles |
| [Device bridge](bridge.md) | hardware designers | how the devices connect to the bus, and how to add one |
