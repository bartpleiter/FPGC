# Open project
project_open /home/bart/repos/FPGC/Hardware/FPGA/CycloneIV_EP4CE40/FPGC.qpf

# Build timing netlist
create_timing_netlist
read_sdc
update_timing_netlist

# Timing summary
report_timing -summary -file timing_summary.txt

# Top failing SETUP paths
report_timing \
    -setup \
    -npaths 10 \
    -detail full_path \
    -file timing_setup_paths.txt

# # Top failing HOLD paths (optional)
# report_timing \
#     -hold \
#     -npaths 10 \
#     -detail full_path \
#     -file timing_hold_paths.txt

project_close
