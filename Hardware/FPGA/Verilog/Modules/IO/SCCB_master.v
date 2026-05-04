// SCCB (Serial Camera Control Bus) Master — write-only, 3-phase transaction
// Based on OV7670-Verilog reference, adapted for 100 MHz system clock.
// Drives open-drain SIOC/SIOD via output-enable signals.

`timescale 1ns / 1ps

module SCCB_master #(
    parameter CLK_FREQ  = 100_000_000,
    parameter SCCB_FREQ = 100_000
)(
    input  wire       clk,
    input  wire       reset,
    input  wire       start,       // Pulse to begin 3-phase write
    input  wire [7:0] address,     // Register address
    input  wire [7:0] data,        // Register data
    output reg        ready,       // High when idle
    output reg        sioc_oe,     // 1 = pull SIOC low, 0 = release (high-Z)
    output reg        siod_oe      // 1 = pull SIOD low, 0 = release (high-Z)
);

    localparam CAMERA_ADDR = 8'h42;  // OV7670 write address

    // Quarter-period in clock cycles
    localparam QUARTER = CLK_FREQ / (4 * SCCB_FREQ);
    localparam HALF    = CLK_FREQ / (2 * SCCB_FREQ);
    localparam DONE_DELAY = 2 * CLK_FREQ / SCCB_FREQ;

    // FSM states
    localparam S_IDLE         = 4'd0;
    localparam S_START        = 4'd1;   // START condition: SIOD low while SIOC high
    localparam S_LOAD_BYTE    = 4'd2;
    localparam S_TX_1         = 4'd3;   // SIOC low
    localparam S_TX_2         = 4'd4;   // Set SIOD data bit
    localparam S_TX_3         = 4'd5;   // SIOC high (slave samples)
    localparam S_TX_4         = 4'd6;   // Shift/check end of byte
    localparam S_STOP_1       = 4'd7;   // SIOC low
    localparam S_STOP_2       = 4'd8;   // SIOD low
    localparam S_STOP_3       = 4'd9;   // SIOC high
    localparam S_STOP_4       = 4'd10;  // SIOD high (STOP condition)
    localparam S_DONE         = 4'd11;  // Inter-transaction delay
    localparam S_TIMER        = 4'd12;  // Countdown subroutine

    reg [3:0]  state        = S_IDLE;
    reg [3:0]  return_state = S_IDLE;
    reg [31:0] timer        = 0;
    reg [7:0]  latched_addr;
    reg [7:0]  latched_data;
    reg [1:0]  byte_counter = 0;   // 0=cam_addr, 1=reg_addr, 2=reg_data, 3=done
    reg [7:0]  tx_byte      = 0;
    reg [3:0]  bit_index    = 0;   // 0-8 (8 = ACK bit)

    always @(posedge clk or posedge reset) begin
        if (reset) begin
            state     <= S_IDLE;
            ready     <= 1'b1;
            sioc_oe   <= 1'b0;
            siod_oe   <= 1'b0;
            timer     <= 0;
        end else begin
            case (state)

            S_IDLE: begin
                ready <= 1'b1;
                if (start) begin
                    latched_addr <= address;
                    latched_data <= data;
                    byte_counter <= 2'd0;
                    bit_index    <= 4'd0;
                    ready        <= 1'b0;
                    // START: SIOD goes low while SIOC is high
                    sioc_oe <= 1'b0;  // SIOC released (high)
                    siod_oe <= 1'b1;  // SIOD pulled low
                    timer        <= QUARTER;
                    return_state <= S_LOAD_BYTE;
                    state        <= S_TIMER;
                end
            end

            S_LOAD_BYTE: begin
                if (byte_counter == 2'd3) begin
                    // All 3 bytes sent, generate STOP
                    state <= S_STOP_1;
                end else begin
                    bit_index <= 4'd0;
                    case (byte_counter)
                        2'd0: tx_byte <= CAMERA_ADDR;
                        2'd1: tx_byte <= latched_addr;
                        2'd2: tx_byte <= latched_data;
                        default: tx_byte <= 8'd0;
                    endcase
                    byte_counter <= byte_counter + 1'b1;
                    state <= S_TX_1;
                end
            end

            S_TX_1: begin
                // SIOC low
                sioc_oe      <= 1'b1;
                timer        <= QUARTER;
                return_state <= S_TX_2;
                state        <= S_TIMER;
            end

            S_TX_2: begin
                // Set SIOD to data bit (or release for ACK)
                siod_oe      <= (bit_index == 4'd8) ? 1'b0 : ~tx_byte[7];
                timer        <= QUARTER;
                return_state <= S_TX_3;
                state        <= S_TIMER;
            end

            S_TX_3: begin
                // SIOC high — slave samples SIOD
                sioc_oe      <= 1'b0;
                timer        <= HALF;
                return_state <= S_TX_4;
                state        <= S_TIMER;
            end

            S_TX_4: begin
                if (bit_index == 4'd8) begin
                    // ACK bit done, load next byte
                    state <= S_LOAD_BYTE;
                end else begin
                    tx_byte   <= tx_byte << 1;
                    bit_index <= bit_index + 1'b1;
                    state     <= S_TX_1;
                end
            end

            // STOP sequence
            S_STOP_1: begin
                sioc_oe      <= 1'b1;  // SIOC low
                timer        <= QUARTER;
                return_state <= S_STOP_2;
                state        <= S_TIMER;
            end

            S_STOP_2: begin
                siod_oe      <= 1'b1;  // SIOD low
                timer        <= QUARTER;
                return_state <= S_STOP_3;
                state        <= S_TIMER;
            end

            S_STOP_3: begin
                sioc_oe      <= 1'b0;  // SIOC high
                timer        <= QUARTER;
                return_state <= S_STOP_4;
                state        <= S_TIMER;
            end

            S_STOP_4: begin
                siod_oe      <= 1'b0;  // SIOD high — STOP condition
                timer        <= QUARTER;
                return_state <= S_DONE;
                state        <= S_TIMER;
            end

            S_DONE: begin
                // Inter-transaction delay
                timer        <= DONE_DELAY;
                return_state <= S_IDLE;
                state        <= S_TIMER;
            end

            S_TIMER: begin
                if (timer == 0)
                    state <= return_state;
                else
                    timer <= timer - 1'b1;
            end

            default: state <= S_IDLE;
            endcase
        end
    end

endmodule
