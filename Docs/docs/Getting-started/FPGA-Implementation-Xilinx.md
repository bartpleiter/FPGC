# FPGA Implementation (Xilinx)

After the design has been tested in simulation, the next step is to test the design on the actual hardware. This involves synthesizing the design, generating a bitstream, and then programming the FPGA and SPI flash memory. This guide assumes you use a Linux (tested Ubuntu 25.04) system, with Vivado (tested 2025.1) installed.

## Setting up Vivado

After installing Vivado, you need to make sure that Vivado can access the JTAG programmer. This is done by running as root:

```bash
cd {VIVADO_INSTALL_DIR}/data/xicom/cable_drivers/lin64/install_script/install_drivers
sudo su
./install_drivers
```

You may need to reboot your system after this step (I did not, only had to replug the JTAG programmer and restart Vivado).

Finally, for this specific starlite FPGA board I am using, you need to hack the data/xicom/xicom_cfgmem_part_table.csv if you want to program the SPI flash memory. This is done by adding the following line to the end of the file:

```csv
1522,0,w25q256jwq-spi-x1_x2_x4,- artix7 artixuplus kintex7 kintexu kintexuplus spartan7 virtex7 virtexu virtexuplus,w25q256jwq,spi,256,x1_x2_x4,,Winbond,,1,,w25q
```

## Synthesizing and deploying the design

1. After opening the xpr project in Vivado, run Synthesis, Implementation, and then Generate Bitstream. This will create a .bit file in the .runs/impl_1 directory.
2. To already test the design on the FPGA without persistence, you can program the FPGA directly with the generated .bit file. This is done by going to Open Hardware Manager, Open Target, Auto Connect (note that on my machine for unknown reasons this takes over a minute). Then right click on the FPGA part number (xc7a75t_0) and select Program Device. Here you can select the .bit file from .runs/impl_1 and then click Program. The FPGA should now be programmed with the design.
3. After the bitstream is generated, if you want to program the SPI flash memory, you also need to generate a .mcs file. This can be done by going to Tools -> Generate Memory Configuration File. Here you need to select 32MB custom Memory Size, create an output file name (e.g. FPGC.mcs), select SPIx4 Interface, check load bitstream files, and select the .bit file from .runs/impl_1. Finally, click OK to generate the .mcs file.
4. To program the SPI flash memory, still in the Hardware Manager, right click the FPGA part number (xc7a75t_0) and select Add Configuration Memory Device. Here you need to select the w25q128jvq-spi-x1_x2_x4 device if using the same board as me (otherwise, check the part number of your FPGA board), and click OK. Now the SPI flash device should appear under the FPGA part number. Right click it and select Program Configuration Memory Device. Here you can select the .mcs file you generated earlier, and then click Program. If no error messages about wrong part numbers, the SPI flash memory should now be programmed with the design and will start after a power cycle.

## Testing the design

After programming the FPGA or SPI flash memory, you can test the design. If you programmed the FPGA directly, the design should start running immediately and be gone after a power cycle. If you programmed the SPI flash memory, you need to power cycle the board to start the design. To verify that the design is running, you should be able to see a picture on the HDMI output.
