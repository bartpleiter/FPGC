# Memory Unit

The Memory Unit (MU) is the bridge between the CPU and all slow I/O peripherals. It presents a simple start/done interface to the pipeline: the CPU sends an address and optional write data, the MU talks to the appropriate peripheral, and signals completion when the result is ready. The pipeline stalls for the entire duration.

High-performance memories (SDRAM, ROM, VRAM) bypass the Memory Unit entirely and connect directly to the CPU pipeline. The MU only handles the I/O address range (`0x7000000` - `0x77FFFFF`).

## Architecture

The MU is placed alongside the CPU and GPU as one of the three main modules in the FPGC design. While the CPU handles computation and the GPU handles video output, the MU handles everything external: serial communication, storage, timers, and miscellaneous hardware.

Internally, the MU instantiates all peripheral controllers (UART, SPI masters, timers, etc.) and contains a state machine that dispatches CPU requests to the correct peripheral based on the address.

## How It Works

1. The CPU pipeline's MEM stage detects an I/O address and asserts `mu_start`.
2. The MU's state machine decodes the address and jumps to a per-device handler state.
3. For fast peripherals (boot mode register, microsecond counter), the result is returned immediately.
4. For slow peripherals (SPI, UART), the MU waits for the device controller's done signal.
5. The MU asserts `done`, the pipeline stall drops, and execution resumes.

Each I/O access takes at least 2 cycles (dispatch + completion). SPI transfers take roughly 16 cycles per byte. UART transmit takes about 100 cycles per byte at 1 Mbaud.

## Peripherals

### UART

A single UART channel connected via USB (CH340C on the PCB). Used for programming, debugging, and serial console.

- **TX** (`0x7000000`): Write a byte. The MU waits for transmission to complete. 1 Mbaud, 8N1 format.
- **RX** (`0x7000001`): Read the most recently received byte. Single-cycle read. An interrupt (`uart_irq`) fires when new data arrives.

### Timers

Three identical one-shot countdown timers. Each timer has two registers: a value register and a trigger register. Write the desired delay (in milliseconds) to the value register, then write anything to the trigger register to start the countdown. When the timer expires, it generates an interrupt.

- **Timer 1**: `0x7000002` (value), `0x7000003` (trigger)
- **Timer 2**: `0x7000004` (value), `0x7000005` (trigger)
- **Timer 3**: `0x7000006` (value), `0x7000007` (trigger)

### SPI Masters

Six independent SPI master controllers, each with a data register and a chip-select register. Writing to the data register initiates an 8-bit SPI transfer and returns the received byte. The chip-select register directly controls the CS pin.

| SPI | Data | CS | Device | Clock Speed |
|---|---|---|---|---|
| SPI0 | `0x7000008` | `0x7000009` | Flash 1 (128 Mbit) | 25 MHz |
| SPI1 | `0x700000A` | `0x700000B` | Flash 2 (128 Mbit) | 25 MHz |
| SPI2 | `0x700000C` | `0x700000D` | USB Host 1 (CH376) | 12.5 MHz |
| SPI3 | `0x700000F` | `0x7000010` | USB Host 2 (CH376) | 12.5 MHz |
| SPI4 | `0x7000012` | `0x7000013` | Ethernet (ENC28J60) | 12.5 MHz |
| SPI5 | `0x7000015` | `0x7000016` | SD Card | 25 MHz |

SPI2, SPI3, and SPI4 also have interrupt pin registers (`0x700000E`, `0x7000011`, `0x7000014`) that read the active-low interrupt output from the connected device.

### Special Registers

- **Boot mode** (`0x7000019`): Read-only. Returns the position of a hardware DIP switch, used by the bootloader to select boot source (UART or SPI Flash).
- **Microsecond counter** (`0x700001A`): Read-only. A free-running 32-bit counter that increments every microsecond. Overflows after about 71.5 minutes. Use subtraction-based comparisons to handle overflow correctly.
- **User LED** (`0x700001B`): Write-only. Controls a status LED on the PCB.

### GPIO

GPIO mode (`0x7000017`) and state (`0x7000018`) registers are declared in the address map but not yet implemented. Reads return 0.

## Design Philosophy

The Memory Unit is intentionally simple. There is no DMA, no interrupt-driven transfers, and no buffering. The CPU busy-waits for every byte of every SPI or UART transfer. This keeps the hardware straightforward and the software predictable. For the peripherals the FPGC uses (bootloader flashing, SD card file access, Ethernet packets), the overhead of busy-waiting is acceptable.
