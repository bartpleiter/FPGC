// CameraSubArbiter — Multiplexes DMA engine and CameraCapture into a single
// SDRAM arbiter DMA port. Round-robin with camera priority (streaming deadlines).
//
// Protocol: single-cycle `start` pulse → wait for `done` pulse.
// When both masters request simultaneously, camera wins (to prevent FIFO overflow).

`timescale 1ns / 1ps

module CameraSubArbiter (
    input  wire         clk,
    input  wire         reset,

    // --- DMA engine port (requester) ---
    input  wire [20:0]  dma_addr,
    input  wire [255:0] dma_data,
    input  wire         dma_we,
    input  wire         dma_start,
    output reg          dma_done,
    output wire [255:0] dma_q,

    // --- Camera port (requester, write-only) ---
    input  wire [20:0]  cam_addr,
    input  wire [255:0] cam_data,
    input  wire         cam_we,
    input  wire         cam_start,
    output reg          cam_done,

    // --- SDRAM arbiter DMA port (downstream) ---
    output reg  [20:0]  sd_addr,
    output reg  [255:0] sd_data,
    output reg           sd_we,
    output reg           sd_start,
    input  wire          sd_done,
    input  wire [255:0]  sd_q
);

    // Read data goes only to DMA (camera is write-only)
    assign dma_q = sd_q;

    // FSM
    localparam S_IDLE    = 2'd0;
    localparam S_DMA     = 2'd1;  // DMA transaction in flight
    localparam S_CAMERA  = 2'd2;  // Camera transaction in flight

    reg [1:0] state    = S_IDLE;
    reg       last_was_cam = 1'b0;  // For round-robin fairness

    always @(posedge clk or posedge reset) begin
        if (reset) begin
            state      <= S_IDLE;
            sd_start   <= 1'b0;
            dma_done   <= 1'b0;
            cam_done   <= 1'b0;
            last_was_cam <= 1'b0;
        end else begin
            // Defaults
            sd_start <= 1'b0;
            dma_done <= 1'b0;
            cam_done <= 1'b0;

            case (state)
            S_IDLE: begin
                if (cam_start && dma_start) begin
                    // Both requesting — camera has priority (streaming deadline)
                    // unless last was camera (fairness)
                    if (last_was_cam) begin
                        sd_addr  <= dma_addr;
                        sd_data  <= dma_data;
                        sd_we    <= dma_we;
                        sd_start <= 1'b1;
                        state    <= S_DMA;
                    end else begin
                        sd_addr  <= cam_addr;
                        sd_data  <= cam_data;
                        sd_we    <= cam_we;
                        sd_start <= 1'b1;
                        state    <= S_CAMERA;
                    end
                end else if (cam_start) begin
                    sd_addr  <= cam_addr;
                    sd_data  <= cam_data;
                    sd_we    <= cam_we;
                    sd_start <= 1'b1;
                    state    <= S_CAMERA;
                end else if (dma_start) begin
                    sd_addr  <= dma_addr;
                    sd_data  <= dma_data;
                    sd_we    <= dma_we;
                    sd_start <= 1'b1;
                    state    <= S_DMA;
                end
            end

            S_DMA: begin
                if (sd_done) begin
                    dma_done     <= 1'b1;
                    last_was_cam <= 1'b0;
                    state        <= S_IDLE;
                end
            end

            S_CAMERA: begin
                if (sd_done) begin
                    cam_done     <= 1'b1;
                    last_was_cam <= 1'b1;
                    state        <= S_IDLE;
                end
            end
            endcase
        end
    end

endmodule
