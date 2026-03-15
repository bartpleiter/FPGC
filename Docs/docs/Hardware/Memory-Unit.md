# Memory Unit

The Memory Unit (MU) is the bridge between the CPU and all slow I/O peripherals. It presents a simple start/done interface to the pipeline: the CPU sends an address and optional write data, the MU talks to the appropriate peripheral, and signals completion when the result is ready. The pipeline stalls for the entire duration.

High-performance memories (SDRAM, ROM, VRAM) bypass the Memory Unit entirely and connect directly to the CPU pipeline. The MU only handles the I/O address range (`0x1C000000` - `0x1DFFFFFF`).

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

- **TX** (`0x1C000000`): Write a byte. The MU waits for transmission to complete. 1 Mbaud, 8N1 format.
- **RX** (`0x1C000004`): Read the most recently received byte. Single-cycle read. An interrupt (`uart_irq`) fires when new data arrives.

### Timers

Three identical one-shot countdown timers. Each timer has two registers: a value register and a trigger register. Write the desired delay (in milliseconds) to the value register, then write anything to the trigger register to start the countdown. When the timer expires, it generates an interrupt.

- **Timer 1**: `0x1C000008` (value), `0x1C00000C` (trigger)
- **Timer 2**: `0x1C000010` (value), `0x1C000014` (trigger)
- **Timer 3**: `0x1C000018` (value), `0x1C00001C` (trigger)

### SPI Masters

Six independent SPI master controllers, each with a data register and a chip-select register. Writing to the data register initiates an 8-bit SPI transfer and returns the received byte. The chip-select register directly controls the CS pin.

| SPI | Data | CS | Device | Clock Speed |
|---|---|---|---|---|
| SPI0 | `0x1C000020` | `0x1C000024` | Flash 1 (128 Mbit) | 25 MHz |
| SPI1 | `0x1C000028` | `0x1C00002C` | Flash 2 (128 Mbit) | 25 MHz |
| SPI2 | `0x1C000030` | `0x1C000034` | USB Host 1 (CH376) | 12.5 MHz |
| SPI3 | `0x1C00003C` | `0x1C000040` | USB Host 2 (CH376) | 12.5 MHz |
| SPI4 | `0x1C000048` | `0x1C00004C` | Ethernet (ENC28J60) | 12.5 MHz |
| SPI5 | `0x1C000054` | `0x1C000058` | SD Card | 25 MHz |

SPI2, SPI3, and SPI4 also have interrupt pin registers (`0x1C000038`, `0x1C000044`, `0x1C000050`) that read the active-low interrupt output from the connected device.

### Special Registers

- **Boot mode** (`0x1C000064`): Read-only. Returns the position of a hardware DIP switch, used by the bootloader to select boot source (UART or SPI Flash).
- **Microsecond counter** (`0x1C000068`): Read-only. A free-running 32-bit counter that increments every microsecond. Overflows after about 71.5 minutes. Use subtraction-based comparisons to handle overflow correctly.
- **User LED** (`0x1C00006C`): Write-only. Controls a status LED on the PCB.

### GPIO

GPIO mode (`0x1C00005C`) and state (`0x1C000060`) registers are declared in the address map but not yet implemented. Reads return 0.

## Design Philosophy

The Memory Unit is intentionally simple. There is no DMA, no interrupt-driven transfers, and no buffering. The CPU busy-waits for every byte of every SPI or UART transfer. This keeps the hardware straightforward and the software predictable. For the peripherals the FPGC uses (bootloader flashing, SD card file access, Ethernet packets), the overhead of busy-waiting is acceptable.
