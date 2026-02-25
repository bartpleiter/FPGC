# Hardware Assembly

This page covers how to get the FPGC hardware built. The FPGC uses a fully custom PCB designed in EasyEDA and manufactured by JLCPCB with assembly, so you don't need to hand-solder most of the components.

## Ordering the PCB

The PCB design is in the `Hardware/PCB/` folder as an EasyEDA project (`.eprj`). Open it in EasyEDA and order directly through JLCPCB. The board is a 6-layer design (120mm x 88.3mm). JLCPCB's assembly service can populate most of the SMD components.

Some components may not be available through JLCPCB's parts library and will need to be ordered separately and hand-soldered. Check the BOM in EasyEDA to see what's covered by assembly.

## Key Components

| Component | Part | Notes |
|---|---|---|
| FPGA | EP4CE40F23I7N | Cyclone IV, 484-pin FBGA |
| SDRAM | 2x W9825G6JH-6 | 32-bit bus, 64 MiB total |
| SRAM | IS61LV5128AL-10TLI | Pixel framebuffer |
| SPI Flash | 2x W25Q128JVSIQ | FPGA config + filesystem |
| USB-UART | CH340C | Programming and serial console |
| Ethernet | ENC28J60 | 10 Mbit SPI Ethernet |
| USB Host | 2x CH376T | HID keyboard/mouse input |
| MicroSD | Standard slot | Mass storage via SPI |
| HDMI | AC-coupled, TVS protected | 640x480 output |
| Power | IP5306 + TPS563201DDCR | Battery or USB powered |

See [PCB](../Hardware/PCB.md) for the full component list, design notes, schematic PDF, and board renders.

## Manual Fixes (Rev 1)

The first PCB revision has wiring errors that need manual correction:

1. **TVS protection ICs** (HDMI and USB): Supply and ground pins are swapped. These need to be reworked or the TVS ICs should be left unpopulated.
2. **MicroSD slot**: Supply and ground are swapped. The GND trace runs under the connector, so the connector must be desoldered to access it.

## Programming Hardware

You need a USB-Blaster JTAG programmer (cheap clones work fine) to program the FPGA. See [FPGA Implementation](FPGA-Implementation.md) for Quartus setup and programming instructions.

## After Assembly

Once the board is assembled and the FPGA is programmed:

1. Connect HDMI to a monitor. You should see the FPGC boot logo.
2. Connect the USB-UART port for serial communication (programming and debug console).
3. Set the boot DIP switch to UART mode for development, or SPI Flash mode to boot from persistent storage.

See [FPGA Simulation](FPGA-Simulation.md) to test the design in simulation before deploying to hardware.
