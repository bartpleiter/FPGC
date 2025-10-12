set_property CFGBVS VCCO [current_design]
set_property CONFIG_VOLTAGE 3.3 [current_design]
set_property BITSTREAM.GENERAL.COMPRESS true [current_design]
set_property BITSTREAM.CONFIG.CONFIGRATE 50 [current_design]
set_property BITSTREAM.CONFIG.SPI_BUSWIDTH 4 [current_design]
set_property BITSTREAM.CONFIG.SPI_FALL_EDGE Yes [current_design]

create_clock -period 5.000 -name sys_clk [get_ports sys_clk_p]
#create_generated_clock -name clk100 -source [get_ports sys_clk_p] -divide_by 2 [get_pins mig7_ddr3/ui_clk]

set_property -dict {PACKAGE_PIN R4 IOSTANDARD DIFF_SSTL15} [get_ports sys_clk_p]
set_property -dict {PACKAGE_PIN R14 IOSTANDARD LVCMOS33} [get_ports sys_rstn]

set_property -dict {PACKAGE_PIN V17  IOSTANDARD TMDS_33} [get_ports HDMI_D2_P]
#set_property -dict {PACKAGE_PIN W17  IOSTANDARD TMDS_33} [get_ports HDMI_D2_N]
set_property -dict {PACKAGE_PIN AA19 IOSTANDARD TMDS_33} [get_ports HDMI_D1_P]
#set_property -dict {PACKAGE_PIN AB20 IOSTANDARD TMDS_33} [get_ports HDMI_D1_N]
set_property -dict {PACKAGE_PIN V18  IOSTANDARD TMDS_33} [get_ports HDMI_D0_P]
#set_property -dict {PACKAGE_PIN V19  IOSTANDARD TMDS_33} [get_ports HDMI_D0_N]
set_property -dict {PACKAGE_PIN Y18  IOSTANDARD TMDS_33} [get_ports HDMI_CLK_P]
#set_property -dict {PACKAGE_PIN Y19  IOSTANDARD TMDS_33} [get_ports HDMI_CLK_N]

# USB UART
set_property -dict {PACKAGE_PIN P15 IOSTANDARD LVCMOS33} [get_ports uart_tx]
set_property -dict {PACKAGE_PIN P14 IOSTANDARD LVCMOS33} [get_ports uart_rx]

# SPI Flash 1 (W25Q128)
set_property -dict {PACKAGE_PIN G16 IOSTANDARD LVCMOS33} [get_ports SPI0_clk]
set_property -dict {PACKAGE_PIN G15 IOSTANDARD LVCMOS33} [get_ports SPI0_miso]
set_property -dict {PACKAGE_PIN G18 IOSTANDARD LVCMOS33} [get_ports SPI0_mosi]
set_property -dict {PACKAGE_PIN G17 IOSTANDARD LVCMOS33} [get_ports SPI0_cs]

# SPI Flash 2 (W25Q128)
set_property -dict {PACKAGE_PIN H18 IOSTANDARD LVCMOS33} [get_ports SPI1_clk]
set_property -dict {PACKAGE_PIN H17 IOSTANDARD LVCMOS33} [get_ports SPI1_miso]
set_property -dict {PACKAGE_PIN M22 IOSTANDARD LVCMOS33} [get_ports SPI1_mosi]
set_property -dict {PACKAGE_PIN N22 IOSTANDARD LVCMOS33} [get_ports SPI1_cs]

# MAX3421E USB Host 1
set_property -dict {PACKAGE_PIN L20 IOSTANDARD LVCMOS33} [get_ports SPI2_clk]
set_property -dict {PACKAGE_PIN L19 IOSTANDARD LVCMOS33} [get_ports SPI2_mosi]
set_property -dict {PACKAGE_PIN L18 IOSTANDARD LVCMOS33} [get_ports SPI2_miso]
set_property -dict {PACKAGE_PIN M18 IOSTANDARD LVCMOS33} [get_ports SPI2_cs]
set_property -dict {PACKAGE_PIN H22 IOSTANDARD LVCMOS33} [get_ports SPI2_nint]

# MAX3421E USB Host 2
set_property -dict {PACKAGE_PIN H19 IOSTANDARD LVCMOS33} [get_ports SPI3_clk]
set_property -dict {PACKAGE_PIN J19 IOSTANDARD LVCMOS33} [get_ports SPI3_mosi]
set_property -dict {PACKAGE_PIN J21 IOSTANDARD LVCMOS33} [get_ports SPI3_miso]
set_property -dict {PACKAGE_PIN J20 IOSTANDARD LVCMOS33} [get_ports SPI3_cs]
set_property -dict {PACKAGE_PIN K18 IOSTANDARD LVCMOS33} [get_ports SPI3_nint]

# ENC28J60
set_property -dict {PACKAGE_PIN K22 IOSTANDARD LVCMOS33} [get_ports SPI4_clk]
set_property -dict {PACKAGE_PIN K21 IOSTANDARD LVCMOS33} [get_ports SPI4_mosi]
set_property -dict {PACKAGE_PIN G20 IOSTANDARD LVCMOS33} [get_ports SPI4_miso]
set_property -dict {PACKAGE_PIN H20 IOSTANDARD LVCMOS33} [get_ports SPI4_cs]
set_property -dict {PACKAGE_PIN J22 IOSTANDARD LVCMOS33} [get_ports SPI4_nint]

# SD Card
set_property -dict {PACKAGE_PIN AA20 IOSTANDARD LVCMOS33} [get_ports SPI5_clk]
set_property -dict {PACKAGE_PIN AB21 IOSTANDARD LVCMOS33} [get_ports SPI5_mosi]
set_property -dict {PACKAGE_PIN AB18 IOSTANDARD LVCMOS33} [get_ports SPI5_miso]
set_property -dict {PACKAGE_PIN AA21 IOSTANDARD LVCMOS33} [get_ports SPI5_cs]

set_property -dict {PACKAGE_PIN H14 IOSTANDARD LVCMOS33} [get_ports boot_mode]
set_property -dict {PACKAGE_PIN H15 IOSTANDARD LVCMOS33} [get_ports switch2]

set_property -dict {PACKAGE_PIN W22 IOSTANDARD LVCMOS33} [get_ports led1]
set_property -dict {PACKAGE_PIN Y22 IOSTANDARD LVCMOS33} [get_ports led2]

# Setting pullups for possibly unconnected inputs
set_property PULLTYPE PULLUP [get_ports SPI0_miso]
set_property PULLTYPE PULLUP [get_ports SPI1_miso]
set_property PULLTYPE PULLUP [get_ports SPI2_miso]
set_property PULLTYPE PULLUP [get_ports SPI2_nint]
set_property PULLTYPE PULLUP [get_ports SPI3_miso]
set_property PULLTYPE PULLUP [get_ports SPI3_nint]
set_property PULLTYPE PULLUP [get_ports SPI4_miso]
set_property PULLTYPE PULLUP [get_ports SPI4_nint]
set_property PULLTYPE PULLUP [get_ports SPI5_miso]

set_property PULLTYPE PULLUP [get_ports boot_mode]
set_property PULLTYPE PULLUP [get_ports switch2]
