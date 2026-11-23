// FPGC
// Top level: every pin of the board. Pins that aren't used yet are held in a safe state.
module fpgc #(
    parameter ROM_CONTENTS       = "rom.hex",
    parameter PC_RESET_CYCLES    = 100000,  // 1 ms
    parameter LED_TICK_BITS      = 23,      // 84 ms
    parameter CLOCKS_PER_BIT     = 100,     // 1 Mbaud
    parameter SDRAM_PAUSE_CYCLES = 20000    // 200 us
) (
    // Clocks
    input  wire        sys_clk_50,
    // verilator lint_off UNUSEDSIGNAL
    input  wire        sys_clk_header,
    // verilator lint_on UNUSEDSIGNAL

    // SDRAM
    output wire        SDRAM_CLK,
    output wire        SDRAM_CSn,
    output wire        SDRAM_WEn,
    output wire        SDRAM_CASn,
    output wire        SDRAM_RASn,
    output wire        SDRAM_CKE,
    output wire [12:0] SDRAM_A,
    output wire [1:0]  SDRAM_BA,
    output wire [3:0]  SDRAM_DQM,
    inout  wire [31:0] SDRAM_DQ,

    // SRAM
    output wire        SRAM_CSn,
    output wire        SRAM_WEn,
    output wire        SRAM_OEn,
    output wire [18:0] SRAM_A,
    inout  wire [7:0]  SRAM_DQ,

    // USB hosts
    output wire        usb1_cs,
    output wire        usb1_clk,
    output wire        usb1_mosi,
    // verilator lint_off UNUSEDSIGNAL
    input  wire        usb1_miso,
    input  wire        usb1_nint,
    // verilator lint_on UNUSEDSIGNAL
    output wire        usb2_cs,
    output wire        usb2_clk,
    output wire        usb2_mosi,
    // verilator lint_off UNUSEDSIGNAL
    input  wire        usb2_miso,
    input  wire        usb2_nint,
    // verilator lint_on UNUSEDSIGNAL

    // HDMI
    output wire        HDMI_CLK_P,
    output wire        HDMI_CLK_N,
    output wire        HDMI_D0_P,
    output wire        HDMI_D0_N,
    output wire        HDMI_D1_P,
    output wire        HDMI_D1_N,
    output wire        HDMI_D2_P,
    output wire        HDMI_D2_N,

    // SPI flash
    output wire        flash1_cs,
    output wire        flash1_clk,
    output wire        flash1_mosi,
    // verilator lint_off UNUSEDSIGNAL
    input  wire        flash1_miso,
    // verilator lint_on UNUSEDSIGNAL
    output wire        flash1_wp_n,
    output wire        flash1_hold_n,
    output wire        flash2_cs,
    output wire        flash2_clk,
    inout  wire        flash2_mosi,    // IO0
    inout  wire        flash2_miso,    // IO1
    inout  wire        flash2_wp_n,    // IO2
    inout  wire        flash2_hold_n,  // IO3

    // UART, to the USB serial chip
    input  wire        uart_rx,
    output wire        uart_tx,
    input  wire        uart_rts_n,
    input  wire        uart_dtr_n,

    // Micro SD card
    output wire        sd_cs,
    output wire        sd_clk,
    output wire        sd_mosi,
    // verilator lint_off UNUSEDSIGNAL
    input  wire        sd_miso,
    // verilator lint_on UNUSEDSIGNAL
    output wire        sd_data2_nc,
    // verilator lint_off UNUSEDSIGNAL
    input  wire        sd_data1_nc,
    // verilator lint_on UNUSEDSIGNAL

    // DIP switches, the rightmost is dipsw[0]
    input  wire [3:0]  dipsw,

    // Audio DAC
    output wire [7:0]  audio_dac_data,

    // Ethernet
    output wire        eth_cs,
    output wire        eth_clk,
    output wire        eth_mosi,
    // verilator lint_off UNUSEDSIGNAL
    input  wire        eth_miso,
    input  wire        eth_nint,
    // verilator lint_on UNUSEDSIGNAL

    // LEDs, 1 is on
    output wire        led_gpu,
    output wire        led_flash,
    output wire        led_usb,
    output wire        led_eth,
    output wire        led_user,
    output wire        led_uart_rx,
    output wire        led_uart_tx,

    // Reset button, low while pressed
    input  wire        reset_n,

    // Display and GPIO headers, inputs until they're used
    // verilator lint_off UNUSEDSIGNAL
    input  wire [6:1]  disp,
    input  wire [9:1]  gpio
    // verilator lint_on UNUSEDSIGNAL
);

// Clock and reset
wire clk;
wire clk_sdram;
wire locked;
wire reset;

system_pll pll (
    .clk_50(sys_clk_50),
    .clk_100(clk),
    .clk_sdram(clk_sdram),
    .locked(locked)
);

reset_control #(
    .PC_RESET_CYCLES(PC_RESET_CYCLES)
) reset_control (
    .clk(clk),
    .locked(locked),
    .button_n(reset_n),
    .rts_n(uart_rts_n),
    .dtr_n(uart_dtr_n),
    .reset(reset)
);

// CPU
// verilator lint_off UNUSEDSIGNAL
wire [31:0] fetch_addr;  // only bits [11:2] reach the ROM
// verilator lint_on UNUSEDSIGNAL
wire        fetch_en;
wire [31:0] rom_word;
wire        bus_request;
wire        bus_write;
wire [31:0] bus_addr;
wire [31:0] bus_write_data;
wire [3:0]  bus_byte_enable;
wire        bus_ready;
wire        bus_error;
wire [31:0] bus_read_word;
wire        timer_irq;
wire        uart_irq;
wire        sdram_req;
wire        sdram_write;
wire [25:5] sdram_line;
wire [2:0]  sdram_first;
wire        sdram_take;
wire        sdram_read_valid;
wire [31:0] sdram_read_word;
wire        sdram_next;
wire [31:0] sdram_write_word;

/* verilator lint_off PINCONNECTEMPTY */
b32p4 cpu (
    .clk(clk),
    .reset(reset),
    .fetch_addr(fetch_addr),
    .fetch_en(fetch_en),
    .rom_word(rom_word),
    .sdram_req(sdram_req),
    .sdram_write(sdram_write),
    .sdram_line(sdram_line),
    .sdram_first(sdram_first),
    .sdram_take(sdram_take),
    .sdram_read_valid(sdram_read_valid),
    .sdram_read_word(sdram_read_word),
    .sdram_next(sdram_next),
    .sdram_write_word(sdram_write_word),
    .bus_request(bus_request),
    .bus_write(bus_write),
    .bus_addr(bus_addr),
    .bus_write_data(bus_write_data),
    .bus_byte_enable(bus_byte_enable),
    .bus_ready(bus_ready),
    .bus_error(bus_error),
    .bus_read_word(bus_read_word),
    .interrupts({uart_irq, timer_irq}),
    .retire_valid(),
    .retire_pc(),
    .retire_instr(),
    .retire_writes_dreg(),
    .retire_dreg(),
    .retire_dreg_value(),
    .retire_write(),
    .retire_write_addr(),
    .retire_write_data(),
    .retire_write_be(),
    .retire_push(),
    .retire_push_value(),
    .retire_pop(),
    .retire_pop_value(),
    .retire_redirect(),
    .retire_target(),
    .retire_irq(),
    .retire_irq_id()
);
/* verilator lint_on PINCONNECTEMPTY */

// SDRAM
sdram_controller #(
    .PAUSE_CYCLES(SDRAM_PAUSE_CYCLES)
) sdram (
    .clk(clk),
    .reset(reset),
    .sdram_req(sdram_req),
    .sdram_write(sdram_write),
    .sdram_line(sdram_line),
    .sdram_first(sdram_first),
    .sdram_take(sdram_take),
    .sdram_read_valid(sdram_read_valid),
    .sdram_read_word(sdram_read_word),
    .sdram_next(sdram_next),
    .sdram_write_word(sdram_write_word),
    .sdram_cs_n(SDRAM_CSn),
    .sdram_ras_n(SDRAM_RASn),
    .sdram_cas_n(SDRAM_CASn),
    .sdram_we_n(SDRAM_WEn),
    .sdram_cke(SDRAM_CKE),
    .sdram_a(SDRAM_A),
    .sdram_ba(SDRAM_BA),
    .sdram_dqm(SDRAM_DQM),
    .sdram_dq(SDRAM_DQ)
);

assign SDRAM_CLK = clk_sdram;

// Boot ROM
wire [11:2] rom_addr;
wire [31:0] rom_read_word;

rom #(
    .CONTENTS(ROM_CONTENTS)
) rom (
    .clk(clk),
    .enable_a(fetch_en),
    .addr_a(fetch_addr[11:2]),
    .word_a(rom_word),
    .addr_b(rom_addr),
    .word_b(rom_read_word)
);

// Devices
device_bridge #(
    .CLOCKS_PER_BIT(CLOCKS_PER_BIT)
) bridge (
    .clk(clk),
    .reset(reset),
    .bus_request(bus_request),
    .bus_write(bus_write),
    .bus_addr(bus_addr),
    .bus_write_data(bus_write_data),
    .bus_byte_enable(bus_byte_enable),
    .bus_ready(bus_ready),
    .bus_error(bus_error),
    .bus_read_word(bus_read_word),
    .rom_addr(rom_addr),
    .rom_read_word(rom_read_word),
    .uart_rx(uart_rx),
    .uart_tx(uart_tx),
    .user_led(led_user),
    .switches(dipsw),
    .timer_irq(timer_irq),
    .uart_irq(uart_irq)
);

// Activity LEDs: a UART line is active while it's low
activity_leds #(
    .LEDS(2),
    .TICK_BITS(LED_TICK_BITS)
) activity_leds (
    .clk(clk),
    .reset(reset),
    .activity({!uart_tx, !uart_rx}),
    .leds({led_uart_tx, led_uart_rx})
);

assign led_gpu   = 1'b0;
assign led_flash = 1'b0;
assign led_usb   = 1'b0;
assign led_eth   = 1'b0;

// SRAM: deselected, no read or write, data lines not driven
assign SRAM_CSn = 1'b1;
assign SRAM_WEn = 1'b1;
assign SRAM_OEn = 1'b1;
assign SRAM_A   = 19'd0;
assign SRAM_DQ  = 8'bz;

// SPI chips: deselected, clock low
assign usb1_cs       = 1'b1;
assign usb1_clk      = 1'b0;
assign usb1_mosi     = 1'b0;
assign usb2_cs       = 1'b1;
assign usb2_clk      = 1'b0;
assign usb2_mosi     = 1'b0;
assign eth_cs        = 1'b1;
assign eth_clk       = 1'b0;
assign eth_mosi      = 1'b0;
assign sd_cs         = 1'b1;
assign sd_clk        = 1'b0;
assign sd_mosi       = 1'b0;
assign sd_data2_nc   = 1'b1;
assign flash1_cs     = 1'b1;
assign flash1_clk    = 1'b0;
assign flash1_mosi   = 1'b0;
assign flash1_wp_n   = 1'b1;
assign flash1_hold_n = 1'b1;
assign flash2_cs     = 1'b1;
assign flash2_clk    = 1'b0;
assign flash2_mosi   = 1'b0;
assign flash2_miso   = 1'bz;
assign flash2_wp_n   = 1'b1;
assign flash2_hold_n = 1'b1;

// HDMI: a constant level on every pair
assign HDMI_CLK_P = 1'b0;
assign HDMI_CLK_N = 1'b1;
assign HDMI_D0_P  = 1'b0;
assign HDMI_D0_N  = 1'b1;
assign HDMI_D1_P  = 1'b0;
assign HDMI_D1_N  = 1'b1;
assign HDMI_D2_P  = 1'b0;
assign HDMI_D2_N  = 1'b1;

// Audio DAC: silent
assign audio_dac_data = 8'd0;

endmodule
