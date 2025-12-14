# FPGA Implementation (Altera)

After the design has been tested in simulation, the next step is to test the design on the actual hardware. This involves synthesizing the design, generating a bitstream, and then programming the FPGA and SPI flash memory. This guide assumes you use a Linux (tested Ubuntu 25.04) system, with Quartus prime (tested 2025.1 lite) installed.

## Setting up Quartus Prime

After installing Quartus Prime, you need to make sure that Quartus Prime can access the JTAG programmer. I use a cheap Chinese USB-Blaster clone, which does not work out of the box on linux. To fix this, I had to do the following:

```bash
sudo apt install libudev-dev libhidapi-dev
cd /usr/lib/x86_64-linux-gnu
sudo ln -s libudev.so.1 libudev.so.0
sudo nano /etc/udev/rules.d/51-altera-usb-blaster.rules
sudo udevadm control --reload-rules
sudo udevadm trigger
```

In the nano editor, paste the following lines:

```text
SUBSYSTEM=="usb", ATTR{idVendor}=="09fb", ATTR{idProduct}=="6001", MODE="0666"
SUBSYSTEM=="usb", ATTR{idVendor}=="09fb", ATTR{idProduct}=="6002", MODE="0666"
SUBSYSTEM=="usb", ATTR{idVendor}=="09fb", ATTR{idProduct}=="6003", MODE="0666"
SUBSYSTEM=="usb", ATTR{idVendor}=="09fb", ATTR{idProduct}=="6010", MODE="0666"
SUBSYSTEM=="usb", ATTR{idVendor}=="09fb", ATTR{idProduct}=="6810", MODE="0666"
```

I did not have to reboot during this process, I only replugged the JTAG programmer and restarted Quartus Prime.

## Synthesizing and deploying the design

1. After opening the qpf project in Quartus, run start Compilation. This will create a .sof file in the output_files directory.
2. To already test the design on the FPGA without persistence, you can program the FPGA directly with the generated .sof file. This is done by opening the Programmer (Tools -> Programmer), adding the .sof file if not already present, selecting it, and clicking Start. The FPGA should now be programmed with the design. (Make sure the correct hardware setup is selected in the Hardware Setup dialog, usually "USB-Blaster".)
3. If you want to program the SPI flash memory, you first need to convert the .sof file to a .jic file. This is done by going to File -> Convert Programming Files. Here you can just click the "Open Conversion Setup Data..." button, select the gen_jic.cof file from the project directory, and then click Generate. This will create a .jic file in the output_files directory.
4. To program the SPI flash memory, still in the Programmer, you need to change the programming file type to "JTAG Indirect Configuration File (.jic)". Then add the .jic file if not already present, select it, and click Start. If there are no error messages, the SPI flash memory should now be programmed with the design and will start after a power cycle.

To verify that the design is running, you should be able to see a picture on the HDMI output with the FPGC logo.
