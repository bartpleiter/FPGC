---
title: Device bridge
weight: 3
reader: hardware
---

`device_bridge` connects the CPU's [bus](../cpu/interface.md#bus) to the ROM's data port and to the devices. It has the address of every device register, answers every access on the bus from its own registers, and tells the devices what to do with strobes. The devices are modules inside the bridge, each with ports that suit it. They know nothing about the bus.

The bridge has no state of a device. It decodes, answers and gives strobes. Each device has its own state and logic: the UART's buffers, the timer's count, the cycle counter's snapshot.

```text
                   +------------------------------------------------+
 CPU bus --------> | device_bridge                                  |
                   |   decodes the address, answers the bus         |
 <-- ready, error, |                                                |
     read word     |     strobes, data |      ^ outputs             |
                   |                   v      |                     |
 ROM data port <-> |   +------+ +---------------+ +-------+ +-----+ |
                   |   | uart | | cycle_counter | | timer | |board| |
                   |   +------+ +---------------+ +-------+ +-----+ |
                   |   +------------------------------------------+ |
                   |   | spi: flash1, flash2, usb1, usb2, eth, sd | |
                   |   +------------------------------------------+ |
                   +------------------------------------------------+

 uart: rx, tx, interrupt 2     timer: interrupt 1     board: user LED, switches
 spi: one per SPI bus, each with its own pins
```

## What goes where

| Address | Goes to |
| --- | --- |
| `0x04000000`–`0x04000FFF` | the ROM's data port, for a read of any size |
| the device registers in the [I/O devices](_index.md#the-devices) table | the register's device, for a word access |
| anything else on the bus | nothing: the bridge answers with `error` |

The rest of the bus's regions, such as the GPU's memories and the external SRAM, aren't built yet. Until then, the bridge answers them with an error.

The device registers are grouped per device, 256 bytes each:

```text
0xFFFF8204 = 1111 1111 1111 1111 1 | 000 0010 | 0000 0100
             bits [31:15] all 1     | [14:8]   | [7:0]
             a device register      | device 2 | register 0x04
```

The bridge compares the whole address with the address of each register, so the groups only keep each device's registers together, with room to grow.

## Parameters

| Parameter | Default | Meaning |
| --- | --- | --- |
| `CLOCKS_PER_BIT` | 100 | the UART's clock cycles per bit: 1 Mbaud at 100 MHz |

## Ports

The bus side is the CPU's bus: `bus_request`, `bus_write`, `bus_addr`, `bus_write_data` and `bus_byte_enable` in, and `bus_ready`, `bus_error` and `bus_read_word` out. See the CPU's [Interface](../cpu/interface.md#bus) page for the protocol. The bus's outputs are registers.

| Port | Direction | Meaning |
| --- | --- | --- |
| `rom_addr[11:2]` | out | the word in the ROM: bits [11:2] of `bus_addr` |
| `rom_read_word[31:0]` | in | the ROM's word, in the cycle after `rom_addr` |
| `uart_rx` | in | the UART's receive line |
| `uart_tx` | out | the UART's transmit line |
| `user_led` | out | the user LED, 1 is on |
| `switches[3:0]` | in | the DIP switches, not synchronized |
| `timer_irq` | out | the timer's interrupt, to the CPU's interrupt 1 |
| `uart_irq` | out | the UART's interrupt, to the CPU's interrupt 2 |

## Answering an access

The bridge answers an access in the cycle after the request, except for three accesses, which wait:

| Access | `bus_ready` |
| --- | --- |
| a register of a device | 1 cycle after the request |
| a data read from the ROM | 2 cycles after the request: the ROM gives its word a cycle after its address |
| a read of the UART's RECEIVE, with a byte waiting | 3 cycles after the request: the bridge tells the UART to take the byte, and the UART's buffer gives it a cycle later |
| a write to the UART's TRANSMIT, with the transmit buffer full | in the cycle after there's room again |
| a read of RECEIVE with nothing waiting | 1 cycle after the request, with 0 |
| an access that nothing takes | 1 cycle after the request, with `error` |

The bus keeps the address and the data the same until the answer. So a waiting access is handled by looking at the same address again, in every cycle, until it can be answered. The bridge only remembers that an access is waiting.

A read of the timer's CONTROL register, at `0xFFFF8204`:

```text
cycle            1          2
bus_request      1          0
bus_addr         FFFF8204   FFFF8204
bus_ready        0          1
bus_read_word    0          control bits
```

A data read from the ROM, at `0x04000010`:

```text
cycle            1          2          3
bus_request      1          0          0
bus_addr         04000010   04000010   04000010
rom_addr         004        004        004
rom_read_word    -          w          w
waiting          0          1          0
bus_ready        0          0          1
bus_read_word    0          0          w
```

## Telling the devices

A write, or a read that takes something, gives a strobe to the device in the cycle after the request: 1 for one cycle, in the same cycle as `bus_ready`. The data of the write is in a register of the bridge in that cycle too. So the device does it at the end of that cycle, and the next access on the bus already sees the result.

A write of N to the timer's COUNT register:

```text
cycle            1          2          3
bus_request      1          0          0
bus_write        1          1          -
bus_addr         FFFF8200   FFFF8200   -
bus_ready        0          1          0
timer start      0          1          0
data             -          N          -
timer count      ...        ...        N
```

A read gives the device's outputs as they are in the request's cycle.

What each device gets from the bridge, and what it gives back:

| Device | From the bridge | To the bridge |
| --- | --- | --- |
| `uart` | `transmit` with a byte, `receive`, `clear` (the errors), `configure` with the interrupt enable | `tx_full`, `received` with the byte taken, `rx_waiting`, `overflow`, `framing_error`, `irq_enable`, `irq` |
| `cycle_counter` | `take_snapshot` | `snapshot`, 64 bits |
| `timer` | `start` with a count, `configure` with periodic and the interrupt enable, `clear` | `count`, `periodic`, `irq_enable`, `expired`, `irq` |
| `board_io` | `set_led` with the LED | `led`, `switches` (synchronized) |

The bridge knows where each bit of a register is. A device only knows what it's told: the timer gets `periodic` as a signal of its own, not as bit 0 of a word.

## Answering with an error

For an access that nothing takes, the bridge answers in the cycle after the request with `ready` and `error`, and a `read_word` of 0:

- an address on the bus outside the ROM and the device registers
- an address in the device registers that isn't a register
- a device register accessed with a size other than a word
- a write to the ROM

The CPU then sets the bad-address flag of its error register. So every access gets an answer, and the CPU never waits forever.

## Adding a device

1. Write the device as a module with ports that suit it: strobes for what it does, and outputs for what software reads. If it can interrupt, it gets a flag, an enable bit and an interrupt output, as described on the [I/O devices](_index.md#interrupts) page.
2. Give its registers addresses in the next free 256 bytes.
3. In the bridge: add the addresses, a line per register for writes and one for reads, the strobes, and the instance of the device.
4. Add its pins and its interrupt output to the bridge's ports, and connect the interrupt to a free interrupt input of the CPU, raising the CPU's `INTERRUPTS` if needed.
5. Add a model of it to the instruction set simulator, in `Tools/b32sim/devices.py`.

## Modules

The files are in `Hardware/io/`.

| File | What it is |
| --- | --- |
| `device_bridge.v` | the device bridge, with the devices inside |
| `uart.v` | the UART: two buffers, a receiver and a transmitter, the error flags and the interrupt |
| `uart_rx.v` | the UART's receiver |
| `uart_tx.v` | the UART's transmitter |
| `fifo.v` | a buffer in a small RAM, for the UART: a byte that's taken comes out in the next cycle |
| `cycle_counter.v` | the cycle counter and its snapshot |
| `timer.v` | the timer |
| `board_io.v` | the user LED, and the DIP switches with a synchronizer |
