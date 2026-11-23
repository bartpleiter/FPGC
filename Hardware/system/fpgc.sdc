# Timing constraints of the top level fpgc

# The board's 50 MHz oscillator, and the PLL's clocks made from it
create_clock -name clk_50 -period 20.000 [get_ports sys_clk_50]
derive_pll_clocks
derive_clock_uncertainty

# Asynchronous inputs, each through a synchronizer
set_false_path -from [get_ports {reset_n uart_rx uart_rts_n uart_dtr_n dipsw[*]}]

# Slow outputs: the UART's line at 1 Mbaud, and the LEDs
set_false_path -to [get_ports {uart_tx led_*}]

# SDRAM
#
# The chips get their clock from the PLL's c2 on the SDRAM_CLK pin. The delays below are
# from the W9825G6KH-6 datasheet (tS 1.5, tH 0.8, tAC 6, tOH 3 ns) and the board's
# traces (5.5 to 7 ps per mm): SDRAM_CLK 25.9 to 40.9 mm, the address, bank and command
# 28.4 to 54.9 mm, DQ and DQM 5.8 to 27.1 mm. See Website/content/sdram.md.
create_generated_clock -name sdram_clk -source [get_pins {pll|pll|auto_generated|pll1|clk[2]}] [get_ports SDRAM_CLK]

# Outputs: max = tS + longest trace - shortest clock trace, min = -tH + shortest trace -
# longest clock trace
set sdram_command [get_ports {SDRAM_A[*] SDRAM_BA[*] SDRAM_CSn SDRAM_RASn SDRAM_CASn SDRAM_WEn}]
set_output_delay -clock sdram_clk -max 1.742 $sdram_command
set_output_delay -clock sdram_clk -min -0.930 $sdram_command
set sdram_data_out [get_ports {SDRAM_DQ[*] SDRAM_DQM[*]}]
set_output_delay -clock sdram_clk -max 1.548 $sdram_data_out
set_output_delay -clock sdram_clk -min -1.054 $sdram_data_out

# Inputs: max = tAC + both longest traces, min = tOH + both shortest traces
set_input_delay -clock sdram_clk -max 6.476 [get_ports {SDRAM_DQ[*]}]
set_input_delay -clock sdram_clk -min 3.174 [get_ports {SDRAM_DQ[*]}]

# A word comes out after the SDRAM's edge one cycle after the READ (CAS latency 2), and
# is captured at the FPGA's edge 3 cycles after the READ: one cycle later than the
# default
set_multicycle_path -setup 2 -from [get_ports {SDRAM_DQ[*]}]

# CKE is a constant
set_false_path -to [get_ports SDRAM_CKE]

