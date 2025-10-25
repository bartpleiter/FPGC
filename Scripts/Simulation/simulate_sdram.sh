#!/bin/bash

# Run simulation and open gtkwave (in X11 as Wayland has issues)
iverilog -o Hardware/FPGA/Verilog/Simulation/Output/sdram.out Hardware/FPGA/Verilog/Simulation/sdram_tb.v &&\
vvp Hardware/FPGA/Verilog/Simulation/Output/sdram.out &&\
if ! pgrep -x "gtkwave" > /dev/null
then
    GDK_BACKEND=x11 gtkwave --dark Hardware/FPGA/Verilog/Simulation/GTKWave/sdram.gtkw &
else
    echo "gtkwave is already running."
fi
