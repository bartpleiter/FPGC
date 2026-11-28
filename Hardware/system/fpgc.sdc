# Timing constraints of the top level fpgc

# The board's 50 MHz oscillator, and the PLL's clocks made from it
create_clock -name clk_50 -period 20.000 [get_ports sys_clk_50]
derive_pll_clocks
derive_clock_uncertainty

# Asynchronous inputs, each through a synchronizer
set_false_path -from [get_ports {reset_n uart_rx uart_rts_n uart_dtr_n dipsw[*] usb1_nint usb2_nint
    eth_nint}]

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


# SPI
#
# Each bus's clock pin is a register that changes every N cycles of the system clock: N
# is 1 for the flashes (50 MHz), 2 for the SD card (25 MHz), and 4 for USB and Ethernet
# (12.5 MHz). Each bus gets two generated clocks: one on that register, and one on the
# pin, which adds the delay of the pin. MOSI changes at the falling edge and the device
# takes it at the rising edge, N cycles later. The device changes its data at the
# falling edge, and the FPGA takes it one cycle after the rising edge: N + 1 cycles
# after the falling edge.
#
# The delays are from the datasheets and the board's traces (5.5 to 7 ps per mm):
# inputs: max = the device's clock to output (tV) + both longest traces, min = its
# output hold + both shortest traces; MOSI: max = the device's setup + the longest
# data trace - the shortest clock trace, min = -its hold + the shortest data trace -
# the longest clock trace.
proc spi_bus {name n clock_pin inputs mosi in_max in_min out_max out_min} {
    # The fitter copies the register for its own feedback; the original drives the pin
    set register [get_registers -no_duplicates "*|spi:${name}|spi_clk"]
    create_generated_clock -name ${name}_reg -divide_by [expr {2 * $n}] \
        -source [get_pins {pll|pll|auto_generated|pll1|clk[1]}] $register
    create_generated_clock -name ${name}_clk -source $register [get_ports $clock_pin]

    set_input_delay -clock ${name}_clk -clock_fall -max $in_max [get_ports $inputs]
    set_input_delay -clock ${name}_clk -clock_fall -min $in_min [get_ports $inputs]
    set_multicycle_path -setup -end [expr {$n + 1}] -from [get_ports $inputs]
    set_multicycle_path -hold -end $n -from [get_ports $inputs]

    # MOSI only changes at a falling edge, half a period from any rising edge, which the
    # analysis can't know: it checks the setup, and the hold has that half period
    set_output_delay -clock ${name}_clk -max $out_max [get_ports $mosi]
    set_output_delay -clock ${name}_clk -min $out_min [get_ports $mosi]
    set_multicycle_path -setup -start $n -to [get_ports $mosi]
    set_false_path -hold -to [get_ports $mosi]
}

# W25Q128JV: tV 6 ns, output hold 1.5 ns, setup 1 ns, hold 2 ns. Clock 18.4 and 16.4 mm,
# IO0 to IO3 18.2 to 24.5 and 15.7 to 25.0 mm, MOSI 18.5 and 15.7 mm.
spi_bus flash1 1 flash1_clk {flash1_mosi flash1_miso flash1_wp_n flash1_hold_n} \
    flash1_mosi 6.300 1.701 1.028 -2.027
spi_bus flash2 1 flash2_clk {flash2_mosi flash2_miso flash2_wp_n flash2_hold_n} \
    flash2_mosi 6.290 1.677 1.020 -2.028

# CH376T at 3.3 V: tV 18 ns, output hold 3 ns, setup 8 ns, hold 2 ns. Clock 24.9 and
# 31.4 mm, MISO 23.1 and 30.9 mm, MOSI 24.6 and 32.3 mm.
spi_bus usb1 4 usb1_clk usb1_miso usb1_mosi 18.336 3.264 8.035 -2.039
spi_bus usb2 4 usb2_clk usb2_miso usb2_mosi 18.436 3.343 8.053 -2.042

# ENC28J60: tV 10 ns, no output hold given, setup 10 ns, hold 10 ns. Clock 35.0 mm,
# MISO 32.5 mm, MOSI 37.0 mm.
spi_bus eth 4 eth_clk eth_miso eth_mosi 10.472 0.371 10.066 -10.042

# SD card: tV 14 ns, no output hold given, setup 5 ns, hold 5 ns. Clock 57.9 mm, MISO
# 59.1 mm, MOSI 64.6 mm.
spi_bus sd 2 sd_clk sd_miso sd_mosi 14.819 0.643 5.134 -5.050

# The chip selects change only between transfers, and the flashes' IO1 to IO3 are only
# released or driven between them (QUAD), many cycles from a clock edge
set_false_path -to [get_ports {flash1_cs flash2_cs usb1_cs usb2_cs eth_cs sd_cs}]
set_false_path -to [get_ports {flash1_miso flash1_wp_n flash1_hold_n flash2_miso flash2_wp_n
    flash2_hold_n}]
