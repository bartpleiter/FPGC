# PCB

Previous versions of the FPGC used an FPGA development board or module with a custom designed I/O PCB. This was mainly done as soldering a BGA FPGA was not easy given the equipment and BGA soldering experience. This actually worked really well since the QMTECH modules used were cheap, compact, contained SDRAM and a lot of I/O pins on headers. However, at this stage of the project I am able to spend more money on the project (as I am not a student anymore), and these days it is exceptionally easy to order a custom PCB with assembly of (BGA) components for a reasonable price. Therefore I designed a complete custom PCB for the FPGC.

## Main PCB features

The FPGC contains the following main components:

- Intel/Altera Cyclone IV FPGA (EP4CE40F23I7N)
- 2x 32MB SDRAM (W9825G6JH-6)
- 1x 4Mbit@8bit SRAM (IS61LV5128AL-10TLI)
- IP5306 power bank management IC for 18650 Li-ion battery operation
- 3.3V, 2.5V and 1.2V voltage regulators (TPS563201DDCR)
- Several I/O devices:
    - HDMI output (AC decoupled with TVS diodes)
    - 2x USB A Host using CH376t (no MAX3421E as these were out of stock or very expensive)
    - MicroSD card slot
    - 10Mbit Ethernet via ENC28J60
    - USB UART via CH340C
    - 2x 128Mbit SPI Flash (W25Q128JVSIQ)
    - 8 bit R2R DAC for future audio output
    - Display header for a display such as the ST7920 or Nokia 5110 module
    - A buch of status LEDs and input switches
    - Extra GPIO pins for future expansion

The result is a 120mm x 88.3mm 6 layer PCB designed with EasyEDA, and manufactured and assembled by JLCPCB (with parts from LCSC/JLCPCB). The project file is available in the `Hardware/PCB` folder of the project.

## Schematic

The schematic PDF export can be downloaded here: [Schematic.pdf](../assets/schematic.pdf)

## PCB Layout

A screenshot of the PCB layout in EasyEDA is shown below.

![pcb_layout](../images/layout.png)

## PCB Render

The following 3d renders show the top and bottom layers of the assembled PCB (in copper color for visibility).

### Top

![3d_render_pcb_top](../images/top.png)

### Bottom

![3d_render_pcb_bottom](../images/bottom.png)
