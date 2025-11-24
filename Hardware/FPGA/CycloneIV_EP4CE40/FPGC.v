module FPGC (
    // Clocks
    input wire          sys_clk_50,
    input wire          sys_clk_header,

    // SDRAM
    output wire         SDRAM_CLK,
    output wire         SDRAM_CSn,
    output wire         SDRAM_WEn,
    output wire         SDRAM_CASn,
    output wire         SDRAM_RASn,
    output wire         SDRAM_CKE,
    output wire [12:0]  SDRAM_A,
    output wire [1:0]   SDRAM_BA,
    output wire [3:0]   SDRAM_DQM,
    inout  wire [31:0]  SDRAM_DQ,

    // SRAM
    output wire         SRAM_CSn,
    output wire         SRAM_WEn,
    output wire         SRAM_OEn,
    output wire [18:0]  SRAM_A,
    inout  wire [7:0]   SRAM_DQ,

    // USB Host
    output wire         usb1_cs,
    output wire         usb1_clk,
    output wire         usb1_mosi,
    input  wire         usb1_miso,
    input  wire         usb1_nint,

    output wire         usb2_cs,
    output wire         usb2_clk,
    output wire         usb2_mosi,
    input  wire         usb2_miso,
    input  wire         usb2_nint,

    // HDMI
    output wire         HDMI_CLK_P,
    output wire         HDMI_CLK_N,
    output wire         HDMI_D0_P,
    output wire         HDMI_D0_N,
    output wire         HDMI_D1_P,
    output wire         HDMI_D1_N,
    output wire         HDMI_D2_P,
    output wire         HDMI_D2_N,

    // SPI Flash
    output wire         flash1_cs,
    output wire         flash1_clk,
    output wire         flash1_mosi,
    input  wire         flash1_miso,
    output wire         flash1_wp_n,
    output wire         flash1_hold_n,

    output wire         flash2_cs,
    output wire         flash2_clk,
    output wire         flash2_mosi,
    input  wire         flash2_miso,
    output wire         flash2_wp_n,
    output wire         flash2_hold_n,

    // UART
    input wire          uart_rx,
    output wire         uart_tx,
    input wire          uart_rts_n,
    input wire          uart_dtr_n,

    // Micro SD Card
    output wire         sd_cs,
    output wire         sd_clk,
    output wire         sd_mosi,
    input  wire         sd_miso,
    output wire         sd_data2_nc,
    input wire          sd_data1_nc,

    // Dipswitch
    input wire [3:0]    dipsw,

    // Audio DAC
    output wire [7:0]   audio_dac_data,

    // Ethernet
    output wire         eth_cs,
    output wire         eth_clk,
    output wire         eth_mosi,
    input  wire         eth_miso,
    input  wire         eth_nint,

    // LEDs
    output wire         led_gpu,
    output wire         led_flash,
    output wire         led_usb,
    output wire         led_eth,
    output wire         led_user,
    output wire         led_uart_rx,
    output wire         led_uart_tx,
    
    // Buttons
    input wire          reset_n,

    // Display header
    output wire         disp_1,
    output wire         disp_2,
    output wire         disp_3,
    output wire         disp_4,
    output wire         disp_5,
    output wire         disp_6,

    // GPIO Header
    inout wire          gpio_1,
    inout wire          gpio_2,
    inout wire          gpio_3,
    inout wire          gpio_4,
    inout wire          gpio_5,
    inout wire          gpio_6,
    inout wire          gpio_7,
    inout wire          gpio_8,
    inout wire          gpio_9
);


endmodule
