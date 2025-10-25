#!/bin/bash

# Run simulation and open gtkwave (in X11 as Wayland has issues)
iverilog -o Hardware/FPGA/Verilog/Simulation/Output/FSX.out Hardware/FPGA/Verilog/Simulation/FSX_tb.v &&\
vvp Hardware/FPGA/Verilog/Simulation/Output/FSX.out &&\
if ! pgrep -x "gtkwave" > /dev/null
then
    GDK_BACKEND=x11 gtkwave --dark Hardware/FPGA/Verilog/Simulation/GTKWave/FSX.gtkw &
else
    echo "gtkwave is already running."
fi
