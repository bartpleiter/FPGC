# FPGA

The FPGC project supports multiple FPGA options. In general there is no specific reason to choose a particular FPGA, as long as it has enough block RAM to contain the 320x240x8-bit framebuffer (unless you want to use an external SRAM chip with some arbiteration logic)  and enough I/O pins to connect all the devices you need. This page describes the different FPGA options that are supported by the project.

## FPGA requirements

The minimum requirements for an FPGA to run the FPGC design is mainly determined by the amount of block RAM needed, pins for external memory and I/O, as well as the amount of logic elements are available to implement the CPU, GPU and I/O logic. There are no exact numbers on these requirements, as certain components and sizes can be adjusted in the Verilog design. One main requirement if you do not want to use an external SRAM chip is that the FPGA must have at least 320x240x8bit = 614,400 bits = ~600kb of block RAM available to store the framebuffer, aside from the block RAM needed for CPU caches and such.

## Supported FPGAs

The FPGC is being tested for the following FPGAs, with their code in the respective folders in the `Hardware/FPGA` directory of the project.

### Intel/Altera Cyclone IV EP4CE40

The Cyclone IV EP4CE40 is currently used on the custom PCB. This is an older but very capable FPGA that was really cheap to buy new from LCSC (20 euro). The performance should be somewhat equal to the newer Cyclone 10 series, but less than the Cyclone V (note that with less performance I do not mean of the CPU design, but of the timing headroom left in the design). The EP4CE40F23I7N variant is used, which comes in a 484-pin FBGA package.

| Specification | Value |
|---------------|-------|
| Logic Elements | 39,600 |
| Block RAM | 1.1Mb |
| HW multipliers | 116 |

### Intel/Altera Cyclone 10 LP 10CL120

The Cyclone 10 LP 10CL120 is supported via the QMTECH Cyclone 10 module. This is a newer FPGA from Intel that should be similar in performance to the Cyclone IV, but with lower power consumption. The 10CL120 variant is the highest model in the Cyclone 10 LP family.

| Specification | Value |
|---------------|-------|
| Logic Elements | 119,088 |
| Block RAM | 3.9Mb |
| HW multipliers | 288 |

### Xilinx Artix 7 XC7A75T

The Artix 7 XC7A75T is supported via the PZ-A75T StarLite development board. This is a more modern FPGA from Xilinx focussed on performance (like the Cyclone V from Altera), and has native TMDS support for HDMI output.

| Specification | Value |
|---------------|-------|
| Logic Cells | 75,520 |
| Block RAM | 3.7Mb |
| DSP Slices | 180 |
