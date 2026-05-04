set_time_format -unit ns -decimal_places 3

create_clock -name {clock} -period 20.000 -waveform { 0.000 10.000 } [get_ports {sys_clk_50}]

# OV7670 pixel clock input (~25 MHz, asynchronous to system clock)
create_clock -name {cam_pclk} -period 40.000 -waveform { 0.000 20.000 } [get_ports {sys_clk_header}]

# PCLK is asynchronous to all system clocks — no timing paths to analyze across domains
# (CDC is handled by async FIFO with Gray-coded pointers in CameraCapture)
set_clock_groups -asynchronous -group {cam_pclk} -group {clock}

derive_pll_clocks -create_base_clocks
derive_clock_uncertainty
