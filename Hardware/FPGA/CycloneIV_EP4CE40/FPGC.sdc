set_time_format -unit ns -decimal_places 3

create_clock -name {clock} -period 20.000 -waveform { 0.000 10.000 } [get_ports {sys_clk_50}]

# OV7670 pixel clock is now sampled as a DATA input (not a clock).
# No clock constraint needed — CDC handled by synchronizer in CameraCapture.

derive_pll_clocks -create_base_clocks
derive_clock_uncertainty
