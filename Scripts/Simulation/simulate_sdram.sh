#!/bin/bash

# Run simulation and open gtkwave (in X11 as Wayland has issues)
iverilog -o Hardware/Vivado/FPGC.srcs/simulation/output/sdram.out Hardware/Vivado/FPGC.srcs/simulation/sdram_tb.v &&\
vvp Hardware/Vivado/FPGC.srcs/simulation/output/sdram.out &&\
GDK_BACKEND=x11 gtkwave --dark Hardware/Vivado/FPGC.srcs/simulation/gtkwave/sdram.gtkw
