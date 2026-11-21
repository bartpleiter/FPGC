# Timing constraints of the top level fpgc

# The board's 50 MHz oscillator, and the PLL's clocks made from it
create_clock -name clk_50 -period 20.000 [get_ports sys_clk_50]
derive_pll_clocks
derive_clock_uncertainty

# Asynchronous inputs, each through a synchronizer
set_false_path -from [get_ports {reset_n uart_rx uart_rts_n uart_dtr_n dipsw[*]}]

# Slow outputs: the UART's line at 1 Mbaud, and the LEDs
set_false_path -to [get_ports {uart_tx led_*}]
