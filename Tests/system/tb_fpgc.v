// Testbench of the top level, fpgc, with the simulation model of the PLL, the serial
// line of serial_line.vh on the UART, and two SDRAM chip models (sdram_chip.v) on the
// SDRAM's pins. The model passes the board clock through, so the testbench drives
// sys_clk_50 at 100 MHz. Each flash has a flash model (flash_chip.v, erased, 64 KiB),
// and every SPI data line is pulled up. The PC's reset takes 50 cycles here, the
// activity LEDs' tick comes every 256 cycles, the SDRAM's pause is 100 cycles, and the
// UART runs at 5 Mbaud (20 cycles per bit), so a program loads 5 times faster. When a
// chip model reports a broken rule, the run stops with an error.
//
// Plusargs, besides those of serial_line.vh:
//
//   +cycles=<n>        stop after n cycles (required)
//   +events=<file>     lines "c p v": at cycle c, set pin p to v. Pins: 0 reset_n,
//                      1 uart_rts_n, 2 uart_dtr_n, 3 the PLL's locked (0 forces it low,
//                      1 releases it)
//   +trace=<file>      a line "c reset user rx tx flash" at every cycle in which the
//                      reset or an LED changed, and at the end lines "pin <name>
//                      <value>" with the pins that aren't used yet, and "sdram
//                      initialized 1" once both chip models have their mode and 8
//                      refreshes
//   +vcd=<file>        also dump the top level's signals
`include "flash_chip.v"
`timescale 1ns / 1ps
module tb_fpgc;

parameter ROM_CONTENTS = "rom.hex";
parameter SDRAM_PAUSE_CYCLES = 100;

reg clk = 1'b0;
always #5 clk = !clk;

integer cycle = 0;
always @(posedge clk)
    cycle <= cycle + 1;

reg       reset_n    = 1'b1;
reg       uart_rts_n = 1'b1;
reg       uart_dtr_n = 1'b1;
wire      uart_tx_line;
wire      led_user;
wire      led_uart_rx;
wire      led_uart_tx;
wire      led_gpu;
wire      led_flash;
wire      led_usb;
wire      led_eth;
wire      SDRAM_CLK;
wire      SDRAM_CSn;
wire      SDRAM_WEn;
wire      SDRAM_CASn;
wire      SDRAM_RASn;
wire      SDRAM_CKE;
wire [12:0] SDRAM_A;
wire [1:0] SDRAM_BA;
wire [3:0] SDRAM_DQM;
wire [31:0] SDRAM_DQ;
wire [4:0] sdram_error_lo;
wire [4:0] sdram_error_hi;
wire      SRAM_CSn;
wire      SRAM_WEn;
wire      SRAM_OEn;
wire [7:0] SRAM_DQ;
wire      usb1_cs;
wire      usb2_cs;
wire      eth_cs;
wire      sd_cs;
wire      flash1_cs;
wire      flash1_clk;
tri1 [3:0] flash1_io;
wire [3:0] flash1_error;
wire      flash2_cs;
wire      flash2_clk;
tri1 [3:0] flash2_io;
wire [3:0] flash2_error;
wire [7:0] audio_dac_data;

`define UART_BIT 20
`include "serial_line.vh"

/* verilator lint_off PINCONNECTEMPTY */
fpgc #(
    .ROM_CONTENTS(ROM_CONTENTS),
    .PC_RESET_CYCLES(50),
    .LED_TICK_BITS(8),
    .CLOCKS_PER_BIT(20),
    .SDRAM_PAUSE_CYCLES(SDRAM_PAUSE_CYCLES)
) dut (
    .sys_clk_50(clk),
    .sys_clk_header(1'b0),
    .SDRAM_CLK(SDRAM_CLK),
    .SDRAM_CSn(SDRAM_CSn),
    .SDRAM_WEn(SDRAM_WEn),
    .SDRAM_CASn(SDRAM_CASn),
    .SDRAM_RASn(SDRAM_RASn),
    .SDRAM_CKE(SDRAM_CKE),
    .SDRAM_A(SDRAM_A),
    .SDRAM_BA(SDRAM_BA),
    .SDRAM_DQM(SDRAM_DQM),
    .SDRAM_DQ(SDRAM_DQ),
    .SRAM_CSn(SRAM_CSn),
    .SRAM_WEn(SRAM_WEn),
    .SRAM_OEn(SRAM_OEn),
    .SRAM_A(),
    .SRAM_DQ(SRAM_DQ),
    .usb1_cs(usb1_cs),
    .usb1_clk(),
    .usb1_mosi(),
    .usb1_miso(1'b1),
    .usb1_nint(1'b1),
    .usb2_cs(usb2_cs),
    .usb2_clk(),
    .usb2_mosi(),
    .usb2_miso(1'b1),
    .usb2_nint(1'b1),
    .HDMI_CLK_P(),
    .HDMI_CLK_N(),
    .HDMI_D0_P(),
    .HDMI_D0_N(),
    .HDMI_D1_P(),
    .HDMI_D1_N(),
    .HDMI_D2_P(),
    .HDMI_D2_N(),
    .flash1_cs(flash1_cs),
    .flash1_clk(flash1_clk),
    .flash1_mosi(flash1_io[0]),
    .flash1_miso(flash1_io[1]),
    .flash1_wp_n(flash1_io[2]),
    .flash1_hold_n(flash1_io[3]),
    .flash2_cs(flash2_cs),
    .flash2_clk(flash2_clk),
    .flash2_mosi(flash2_io[0]),
    .flash2_miso(flash2_io[1]),
    .flash2_wp_n(flash2_io[2]),
    .flash2_hold_n(flash2_io[3]),
    .uart_rx(uart_rx_line),
    .uart_tx(uart_tx_line),
    .uart_rts_n(uart_rts_n),
    .uart_dtr_n(uart_dtr_n),
    .sd_cs(sd_cs),
    .sd_clk(),
    .sd_mosi(),
    .sd_miso(1'b1),
    .sd_data2_nc(),
    .sd_data1_nc(1'b1),
    .dipsw(4'b0101),
    .audio_dac_data(audio_dac_data),
    .eth_cs(eth_cs),
    .eth_clk(),
    .eth_mosi(),
    .eth_miso(1'b1),
    .eth_nint(1'b1),
    .led_gpu(led_gpu),
    .led_flash(led_flash),
    .led_usb(led_usb),
    .led_eth(led_eth),
    .led_user(led_user),
    .led_uart_rx(led_uart_rx),
    .led_uart_tx(led_uart_tx),
    .reset_n(reset_n),
    .disp(6'd0),
    .gpio(9'd0)
);
/* verilator lint_on PINCONNECTEMPTY */

// The SDRAM: two chip models on the SDRAM's pins, with the PLL model's clock
sdram_chip #(
    .PAUSE(SDRAM_PAUSE_CYCLES * 10_000)
) sdram_lo (
    .clk(SDRAM_CLK),
    .cke(SDRAM_CKE),
    .cs_n(SDRAM_CSn),
    .ras_n(SDRAM_RASn),
    .cas_n(SDRAM_CASn),
    .we_n(SDRAM_WEn),
    .ba(SDRAM_BA),
    .a(SDRAM_A),
    .dqm(SDRAM_DQM[1:0]),
    .dq(SDRAM_DQ[15:0]),
    .error(sdram_error_lo)
);

sdram_chip #(
    .PAUSE(SDRAM_PAUSE_CYCLES * 10_000)
) sdram_hi (
    .clk(SDRAM_CLK),
    .cke(SDRAM_CKE),
    .cs_n(SDRAM_CSn),
    .ras_n(SDRAM_RASn),
    .cas_n(SDRAM_CASn),
    .we_n(SDRAM_WEn),
    .ba(SDRAM_BA),
    .a(SDRAM_A),
    .dqm(SDRAM_DQM[3:2]),
    .dq(SDRAM_DQ[31:16]),
    .error(sdram_error_hi)
);

// The flashes
flash_chip #(
    .ADDRESS_BITS(16)
) flash1 (
    .cs_n(flash1_cs),
    .clk(flash1_clk),
    .io(flash1_io),
    .error(flash1_error)
);

flash_chip #(
    .ADDRESS_BITS(16)
) flash2 (
    .cs_n(flash2_cs),
    .clk(flash2_clk),
    .io(flash2_io),
    .error(flash2_error)
);

always @(posedge clk)
    if (flash1_error != 4'd0 || flash2_error != 4'd0)
        $fatal(1, "a flash model reports a broken rule in cycle %0d", cycle);

// A reset of the system pauses the refresh check of the chips. A broken rule of the
// SDRAM ends the run with an error.
always @(posedge clk) begin
    if (dut.reset) begin
        sdram_lo.pause_refresh_check;
        sdram_hi.pause_refresh_check;
    end
    if (sdram_error_lo != 5'd0 || sdram_error_hi != 5'd0)
        $fatal(1, "an SDRAM chip model reports a broken rule in cycle %0d", cycle);
end

// The pin events
reg [8*256-1:0] path;
integer events_fd;
integer event_cycle;
integer event_pin;
integer event_value;

initial begin
    if ($value$plusargs("events=%s", path)) begin
        events_fd = $fopen(path, "r");
        if (events_fd == 0)
            $fatal(1, "cannot read %0s", path);
        while ($fscanf(events_fd, "%d %d %d\n", event_cycle, event_pin,
                       event_value) == 3) begin
            while (cycle < event_cycle)
                @(posedge clk);
            case (event_pin)
                0: reset_n    <= event_value[0];
                1: uart_rts_n <= event_value[0];
                2: uart_dtr_n <= event_value[0];
                3: if (event_value[0]) release dut.pll.locked;
                   else force dut.pll.locked = 1'b0;
                default: $fatal(1, "no pin %0d", event_pin);
            endcase
        end
    end
end

// The trace
integer trace_fd = 0;
reg [4:0] traced = 5'bxxxxx;
wire [4:0] watched = {dut.reset, led_user, led_uart_rx, led_uart_tx, led_flash};

always @(posedge clk) begin
    if (trace_fd != 0 && watched !== traced)
        $fwrite(trace_fd, "%0d %b %b %b %b %b\n", cycle, watched[4], watched[3],
                watched[2], watched[1], watched[0]);
    traced <= watched;
end

// The tx line before the first clock edge, as after configuration
reg tx_at_start;
initial #1 tx_at_start = uart_tx_line;

task trace_pins;
    begin
        $fwrite(trace_fd, "pin uart_tx_at_start %b\n", tx_at_start);
        $fwrite(trace_fd, "pin led_gpu %b\n", led_gpu);
        $fwrite(trace_fd, "pin led_flash %b\n", led_flash);
        $fwrite(trace_fd, "pin led_usb %b\n", led_usb);
        $fwrite(trace_fd, "pin led_eth %b\n", led_eth);
        $fwrite(trace_fd, "sdram initialized %b\n",
                sdram_lo.mode_set && sdram_lo.refreshes >= 8 &&
                sdram_hi.mode_set && sdram_hi.refreshes >= 8);
        $fwrite(trace_fd, "pin SRAM_CSn %b\n", SRAM_CSn);
        $fwrite(trace_fd, "pin SRAM_WEn %b\n", SRAM_WEn);
        $fwrite(trace_fd, "pin SRAM_OEn %b\n", SRAM_OEn);
        $fwrite(trace_fd, "pin SRAM_DQ %b\n", SRAM_DQ);
        $fwrite(trace_fd, "pin usb1_cs %b\n", usb1_cs);
        $fwrite(trace_fd, "pin usb2_cs %b\n", usb2_cs);
        $fwrite(trace_fd, "pin eth_cs %b\n", eth_cs);
        $fwrite(trace_fd, "pin sd_cs %b\n", sd_cs);
        $fwrite(trace_fd, "pin flash1_cs %b\n", flash1_cs);
        $fwrite(trace_fd, "pin flash2_cs %b\n", flash2_cs);
        $fwrite(trace_fd, "pin audio_dac_data %b\n", audio_dac_data);
    end
endtask

integer cycles;

initial begin
    if (!$value$plusargs("cycles=%d", cycles))
        $fatal(1, "+cycles=<n> is missing");
    if ($value$plusargs("trace=%s", path))
        trace_fd = $fopen(path, "w");
    if ($value$plusargs("vcd=%s", path)) begin
        $dumpfile(path);
        $dumpvars(1, tb_fpgc);
        $dumpvars(1, dut);
    end
    while (cycle < cycles)
        @(posedge clk);
    if (trace_fd != 0) begin
        trace_pins;
        $fclose(trace_fd);
    end
    $finish(0);
end

endmodule
