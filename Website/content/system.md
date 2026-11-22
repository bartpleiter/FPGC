---
title: System
weight: 4
reader: hardware
---

The top level of the FPGC, `fpgc`: the CPU, the boot ROM and the [device bridge](io/bridge.md) on the board, with the clock, the reset and the LEDs. The [SDRAM controller](sdram.md) isn't in the top level yet, so the CPU only runs from the ROM.

```text
                +-----------------------------------------------------------+
 sys_clk_50 --> | system_pll --> clk (100 MHz), locked                      |
 reset_n -----> | reset_control --> reset                                   |
 uart_rts_n --> |                                                           |
 uart_dtr_n --> |   rom (4 KiB) <-- fetch --- b32p4 --- bus --> device_bridge |
                |               <-- data reads ---------------------+       |
                |                                 <-- interrupts ---+       |
 uart_rx -----> | ----------------------------------------------> UART      |
 uart_tx <----- | <---------------------------------------------- UART      |
 dipsw -------> | ----------------------------------------------> board     |
 led_user <---- | <---------------------------------------------- board     |
 led_uart_rx <- | <-- activity_leds <-- the UART's lines                    |
 led_uart_tx <- |                                                           |
                +-----------------------------------------------------------+
```

## Clock

The board's 50 MHz oscillator, on pin T2, goes into the PLL, which makes the 100 MHz clock of the whole design. The PLL is placed at PLL 2, whose dedicated output reaches the SDRAM's clock pin, for the SDRAM controller later. The oscillator's pin isn't a dedicated input of PLL 2, so the input reaches it through ordinary routing. That shifts all of the PLL's outputs together, and doesn't change the timing between them.

## Reset

One reset, `reset`, for the whole design, from three sources:

| Source | Pins | Resets while |
| --- | --- | --- |
| the PLL | its `locked` output | the PLL isn't locked, after configuration or when its input clock is lost |
| the button | `reset_n` (Y2) | the button is pressed |
| the PC | `uart_rts_n` (A6), `uart_dtr_n` (B6) | RTS# is low and DTR# is high, once that has lasted 1 ms |

- Every source goes through a synchronizer, and the reset is a register: the reset is fully synchronous. Its registers start at reset after the FPGA's configuration, so the design is in reset from the start.
- The reset ends a few cycles after the last source ends. Every register with a reset gets it on a clock edge.
- Opening a serial port on the PC sets RTS and DTR together, and closing it releases both together, so neither is a reset. Only a program that sets RTS without DTR on purpose resets the board, such as `fpgc board console --reset`. The 1 ms filters out the moment between the two lines' changes, when they are briefly in the reset combination.

## Boot ROM

4 KiB with two read ports: one for the CPU's instruction fetches, and one for data reads through the device bridge. Its contents come from `Software/boot/boot.asm`, assembled into a hex file when the design is built. The CPU starts at its first word, `0x04000000`, after every reset.

The program quiets the devices, prints the error register and `boot: ready` over the UART, and then sends back every byte it receives.

## LEDs

| LED | Pin | Shows |
| --- | --- | --- |
| user | W1 | the board register's LED, written by software |
| UART rx | W2 | activity on the UART's receive line |
| UART tx | Y1 | activity on the UART's transmit line |
| flash | U2 | off, until the flash's SPI controller exists |
| USB | V1 | off, until the USB chips' SPI controllers exist |
| Ethernet | V2 | off, until the Ethernet chip's SPI controller exists |
| GPU | U1 | off, until the GPU exists |

Software only controls the user LED. The others show what the hardware does, through `activity_leds`:

- Activity is a level that's 1 while the hardware does something with the outside world. For a UART line, that's the line being low, which it is during every start bit and every 0 bit.
- Every 2^23 cycles (about 84 ms), a tick ends a period. An LED that's off turns on for the next period if there was activity in the last one. An LED that's on turns off for the next period.
- So a single event of one cycle gives a flash of one period, and continuous activity blinks at about 6 Hz.

```text
tick period          1      2      3      4      5      6      7
activity             x      -      -      x x x  x x x  x x x  -
LED                  off    on     off    off    on     off    on
```

## Pins

The top level has every pin of the board, 169 of them, as 3.3-V LVTTL. Pins that aren't used yet are held in a state that's safe for the chip on them:

| Chip | State |
| --- | --- |
| SDRAM | deselected (CS# high), clock enable low, clock low, data masks high, data lines not driven |
| SRAM | deselected (CS# high), no read or write (OE# and WE# high), data lines not driven |
| SPI flash, SD card, USB chips, Ethernet chip | deselected (CS# high), clock low, the flash's write protect and hold high |
| HDMI | a constant level on every pair |
| audio DAC | 0 |
| display and GPIO headers | inputs |

Pins on no net at all are inputs with a weak pull-up.

## Timing

The SDC constrains the 50 MHz clock, and the PLL's clocks follow from it. The asynchronous inputs (the button, the UART's lines, the DIP switches) and the slow outputs (the UART's transmit line, the LEDs) aren't timed: each goes through a synchronizer, or changes far slower than the clock.

## Building and programming

| Command | What |
| --- | --- |
| `fpgc system build` | assembles the boot ROM, generates the Quartus project, compiles it, and reports the timing at every corner |
| `fpgc board program` | programs the FPGA over JTAG. The board's flash isn't changed, so the design is gone after a power cycle. |
| `fpgc board console` | shows what the board sends, and sends each typed line. With `--reset`, it resets the board first. |

## Modules

The files are in `Hardware/system/`.

| File | What it is |
| --- | --- |
| `fpgc.v` | the top level |
| `system_pll.v` | the PLL |
| `reset_control.v` | the reset from the PLL, the button and the PC |
| `activity_leds.v` | the activity LEDs |
| `rom.v` | the boot ROM |
| `pins.qsf` | the pins, and the PLL's place |
| `fpgc.sdc` | the timing constraints |
