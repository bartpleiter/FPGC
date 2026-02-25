# FPGA Implementation (Altera)

After the design has been tested in simulation, the next step is to synthesize it for the actual hardware. This involves generating a bitstream and programming the FPGA and SPI flash memory. This guide assumes Linux (tested Ubuntu 25.04) with Quartus Prime installed (tested 2025.1 Lite).

## Setting Up Quartus Prime

After installing Quartus Prime, you need to make sure it can access the JTAG programmer. I use a cheap Chinese USB-Blaster clone, which does not work out of the box on Linux. To fix this:

```bash
sudo apt install libudev-dev libhidapi-dev
cd /usr/lib/x86_64-linux-gnu
sudo ln -s libudev.so.1 libudev.so.0
sudo nano /etc/udev/rules.d/51-altera-usb-blaster.rules
sudo udevadm control --reload-rules
sudo udevadm trigger
```

In the nano editor, paste:

```text
SUBSYSTEM=="usb", ATTR{idVendor}=="09fb", ATTR{idProduct}=="6001", MODE="0666"
SUBSYSTEM=="usb", ATTR{idVendor}=="09fb", ATTR{idProduct}=="6002", MODE="0666"
SUBSYSTEM=="usb", ATTR{idVendor}=="09fb", ATTR{idProduct}=="6003", MODE="0666"
SUBSYSTEM=="usb", ATTR{idVendor}=="09fb", ATTR{idProduct}=="6010", MODE="0666"
SUBSYSTEM=="usb", ATTR{idVendor}=="09fb", ATTR{idProduct}=="6810", MODE="0666"
```

I did not have to reboot during this process. I only replugged the JTAG programmer and restarted Quartus Prime.

## Synthesizing and Deploying

1. Open the `.qpf` project in Quartus and run Start Compilation. This creates a `.sof` file in `output_files/`.
2. **Quick test (volatile):** Open the Programmer (Tools, Programmer), add the `.sof` file, select it, and click Start. Make sure the correct hardware is selected in Hardware Setup (usually "USB-Blaster"). The FPGA is now programmed but will lose the design on power cycle.
3. **Persistent (SPI Flash):** Go to File, Convert Programming Files. Click "Open Conversion Setup Data...", select the `gen_jic.cof` file from the project directory, and click Generate. This creates a `.jic` file in `output_files/`.
4. In the Programmer, change the file type to "JTAG Indirect Configuration File (.jic)", add the `.jic` file, and click Start. The SPI flash is now programmed and the design will load on every power-up.

To verify that the design is running, you should see the FPGC boot logo on the HDMI output.
