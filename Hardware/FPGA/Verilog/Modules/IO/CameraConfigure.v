// Camera Configure — ROM-driven SCCB configuration sequencer
// Reads {reg_addr, reg_data} entries from OV7670_config_rom and sends them
// via SCCB_master. Special entries: 0xFFF0 = 10ms delay, 0xFFFF = end.

`timescale 1ns / 1ps

module CameraConfigure #(
    parameter CLK_FREQ = 100_000_000
)(
    input  wire clk,
    input  wire reset,
    input  wire start,       // Pulse to begin configuration sequence
    output reg  done,        // Pulses high when all registers have been written
    output wire sioc,        // SCCB clock (directly drive or open-drain)
    output wire siod         // SCCB data  (directly drive or open-drain)
);

    // --- Config ROM ---
    reg  [7:0]  rom_addr;
    wire [15:0] rom_data;

    OV7670_config_rom rom (
        .clk  (clk),
        .addr (rom_addr),
        .dout (rom_data)
    );

    // --- SCCB master ---
    reg        sccb_start;
    reg  [7:0] sccb_addr;
    reg  [7:0] sccb_data;
    wire       sccb_ready;
    wire       sccb_sioc_oe;
    wire       sccb_siod_oe;

    SCCB_master #(.CLK_FREQ(CLK_FREQ)) sccb (
        .clk     (clk),
        .reset   (reset),
        .start   (sccb_start),
        .address (sccb_addr),
        .data    (sccb_data),
        .ready   (sccb_ready),
        .sioc_oe (sccb_sioc_oe),
        .siod_oe (sccb_siod_oe)
    );

    // Open-drain bus driving
    assign sioc = sccb_sioc_oe ? 1'b0 : 1'bz;
    assign siod = sccb_siod_oe ? 1'b0 : 1'bz;

    // --- Sequencer FSM ---
    localparam S_IDLE     = 2'd0;
    localparam S_SEND     = 2'd1;
    localparam S_DONE     = 2'd2;
    localparam S_TIMER    = 2'd3;

    reg [1:0]  state        = S_IDLE;
    reg [1:0]  return_state = S_IDLE;
    reg [31:0] timer        = 0;

    localparam DELAY_10MS = CLK_FREQ / 100;  // 10ms in clock cycles

    always @(posedge clk or posedge reset) begin
        if (reset) begin
            state      <= S_IDLE;
            rom_addr   <= 8'd0;
            done       <= 1'b0;
            sccb_start <= 1'b0;
            timer      <= 0;
        end else begin
            sccb_start <= 1'b0;  // default: deassert

            case (state)
            S_IDLE: begin
                done <= 1'b0;
                if (start) begin
                    rom_addr <= 8'd0;
                    state    <= S_TIMER;
                    // One-cycle delay for ROM to output first entry
                    timer        <= 1;
                    return_state <= S_SEND;
                end
            end

            S_SEND: begin
                case (rom_data)
                    16'hFFFF: begin
                        // End of ROM
                        state <= S_DONE;
                    end
                    16'hFFF0: begin
                        // 10ms delay
                        timer        <= DELAY_10MS;
                        return_state <= S_SEND;
                        rom_addr     <= rom_addr + 1'b1;
                        state        <= S_TIMER;
                    end
                    default: begin
                        if (sccb_ready) begin
                            sccb_addr  <= rom_data[15:8];
                            sccb_data  <= rom_data[7:0];
                            sccb_start <= 1'b1;
                            rom_addr   <= rom_addr + 1'b1;
                            // Small delay for SCCB ready to deassert
                            timer        <= 1;
                            return_state <= S_SEND;
                            state        <= S_TIMER;
                        end
                    end
                endcase
            end

            S_DONE: begin
                done  <= 1'b1;
                // Stay in S_DONE — done remains latched high permanently
            end

            S_TIMER: begin
                if (timer == 0)
                    state <= return_state;
                else
                    timer <= timer - 1'b1;
            end
            endcase
        end
    end

endmodule
