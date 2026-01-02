#!/bin/bash

# Run timing analysis on the FPGA design using Quartus Prime Timing Analyzer

# Compile first
/home/bart/altera_lite/25.1std/quartus/bin/quartus_sh -t Scripts/Tests/quartus_compile.tcl

# Run timing analysis
/home/bart/altera_lite/25.1std/quartus/bin/quartus_sta -t Scripts/Tests/quartus_timing.tcl

echo "Timing analysis completed. Check the Quartus Prime project (Hardware/FPGA/CycloneIV_EP4CE40) for timing reports (timing_summary.txt and timing_setup_paths.txt)."
