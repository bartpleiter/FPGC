/*
 * Generates video timings and counters
 * Note that changing the V_POL from 0 requires updating the BGWrenderer.v
 */
module TimingGenerator #(
    parameter H_RES           = 640, // Horizontal resolution (pixels)
    parameter V_RES           = 480, // Vertical resolution (lines)
    parameter H_FP            = 16,  // Horizontal front porch
    parameter H_SYNC          = 96,  // Horizontal sync
    parameter H_BP            = 48,  // Horizontal back porch
    parameter V_FP            = 10,  // Vertical front porch
    parameter V_SYNC          = 2,   // Vertical sync
    parameter V_BP            = 33,  // Vertical back porch
    parameter H_POL           = 0,   // Vertical sync polarity
    parameter V_POL           = 0,   // Vertical sync polarity
    parameter INTERRUPT_TICKS = 32   // Number of cycles to keep interrupt signal high
) (
    input wire        clkPixel,
    output reg [11:0] h_count = 12'd0, // Frame position in lines including blanking
    output reg [11:0] v_count = 12'd0, // Frame position in lines including blanking
    output wire       hsync,
    output wire       vsync,
    output wire       blank,
    output wire       frameDrawn
);

                                     // Horizontal: sync, active, and pixels
localparam HS_STA = H_FP - 1;        // Sync start (first pixel is 0) 15
localparam HS_END = HS_STA + H_SYNC; // Sync end 111
localparam HA_STA = HS_END + H_BP;   // Active start 149
localparam HA_END = HA_STA + H_RES;  // Active end 789
localparam LINE   = HA_END;          // Line pixels

                                     // Vertical: sync, active, and pixels
localparam VS_STA = V_FP - 1;        // Sync start (first line is 0) 9
localparam VS_END = VS_STA + V_SYNC; // Sync end 11
localparam VA_STA = VS_END + V_BP;   // Active start 44
localparam VA_END = VA_STA + V_RES;  // Active end 524
localparam FRAME  = VA_END;          // Frame lines

always @(posedge clkPixel)
begin
    // If end of line
    if (h_count == LINE)
    begin
        h_count <= 12'd0;
        v_count <= (v_count == FRAME) ? 12'd0 : v_count + 12'd1;
    end
    else
        h_count <= h_count + 12'd1;
end

assign hsync = (h_count > HS_STA && h_count <= HS_END) ^ H_POL;
assign vsync = (v_count > VS_STA && v_count <= VS_END) ^ V_POL;

assign blank = ~(h_count > HA_STA && h_count <= HA_END && v_count > VA_STA && v_count <= VA_END);

// Interrupt signal for CPU, high for INTERRUPT_TICKS ticks when last frame is drawn
assign frameDrawn = (v_count == 0 && h_count < INTERRUPT_TICKS);

endmodule
