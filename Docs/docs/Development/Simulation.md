# Simulation

To simulate the verilog design before running it on an FPGA I mostly use iverilog and GTKWave. The plan is to also start using Vivado simulator when I need to test the connection to the SDRAM controller (which is a Xilinx IP).

It is required to use iverilog >= 12.0.
A simulation can be started by running the relevant script in `Scripts/Simulation/`. 
Depending on the script started, test code might be assembled and placed as initialization file in memory, with the purpose to simulate certain instructions.
Simulation test code can be found in `Software/BareMetalASM/Simulation/`.

Running the simulation script will show logs from `vvp` in the terminal:

![vvp](../images/vvp.png)

## Waveform

The simulation script will also start GTKWave with the generated waveform and some pre-configured configuration file.

![GTKwave](../images/gtkwave.png)
