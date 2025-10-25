set_time_format -unit ns -decimal_places 3

create_clock -name {clock} -period 20.000 -waveform { 0.000 10.000 } [get_ports {sys_clk_50}]

derive_pll_clocks -create_base_clocks
derive_clock_uncertainty
