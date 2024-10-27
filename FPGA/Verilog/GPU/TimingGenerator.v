/*
* Generates video timings and counters
*/
module TimingGenerator
#(
    parameter H_RES   = 640,        // horizontal resolution (pixels)
    parameter V_RES   = 480,        // vertical resolution (lines)
    parameter H_FP    = 16,         // horizontal front porch
    parameter H_SYNC  = 96,         // horizontal sync
    parameter H_BP    = 48,         // horizontal back porch
    parameter V_FP    = 10,         // vertical front porch
    parameter V_SYNC  = 2,          // vertical sync
    parameter V_BP    = 33,         // vertical back porch
    parameter H_POL   = 0,          // horizontal sync polarity (0:neg, 1:pos)
    parameter V_POL   = 0,          // vertical sync polarity
    parameter INTERRUPT_TICKS = 32  // number of cycles the interrupt should be high
)
/* 4xWide 320x240
#(
    parameter H_RES   = 1706,       // horizontal resolution (pixels)
    parameter V_RES   = 240,        // vertical resolution (lines)
    parameter H_FP    = 40,         // horizontal front porch
    parameter H_SYNC  = 168,        // horizontal sync
    parameter H_BP    = 208,        // horizontal back porch
    parameter V_FP    = 3,          // vertical front porch
    parameter V_SYNC  = 10,         // vertical sync
    parameter V_BP    = 6,          // vertical back porch
    parameter H_POL   = 0,          // horizontal sync polarity (0:neg, 1:pos)
    parameter V_POL   = 0,          // vertical sync polarity
    parameter INTERRUPT_TICKS = 32  // number of cycles the interrupt should be high
)*/
(
    input clkPixel,

    // Position counters
    output reg [11:0] h_count = 12'd0,  // line position in pixels including blanking 
    output reg [11:0] v_count = 12'd0,  // frame position in lines including blanking 

    output wire hsync,
    output wire vsync,
    output wire csync,
    output wire blank,
    
    //Interrupt signal
    output frameDrawn
);

// Horizontal: sync, active, and pixels
localparam HS_STA = H_FP - 1;           // sync start (first pixel is 0)    15
localparam HS_END = HS_STA + H_SYNC;    // sync end                         111
localparam HA_STA = HS_END + H_BP;      // active start                     149
localparam HA_END = HA_STA + H_RES;     // active end                       789
localparam LINE   = HA_END;             // line pixels

// Vertical: sync, active, and pixels
localparam VS_STA = V_FP - 1;           // sync start (first line is 0)     9
localparam VS_END = VS_STA + V_SYNC;    // sync end                         11
localparam VA_STA = VS_END + V_BP;      // active start                     44
localparam VA_END = VA_STA + V_RES;     // active end                       524
localparam FRAME  = VA_END;             // frame lines


always @ (posedge clkPixel)
begin
    if (h_count == LINE)  // end of line
    begin
         h_count <= 12'd0;
            v_count <= (v_count == FRAME) ? 12'd0 : v_count + 12'd1;
    end
    else 
         h_count <= h_count + 12'd1;
end


assign hsync = H_POL ? (h_count > HS_STA && h_count <= HS_END):
                ~(h_count > HS_STA && h_count <= HS_END);

assign vsync = V_POL ? (v_count > VS_STA && v_count <= VS_END):
                ~(v_count > VS_STA && v_count <= VS_END);

assign csync = ~(hsync ^ vsync);

assign blank = ~(h_count > HA_STA && h_count <= HA_END && v_count > VA_STA && v_count <= VA_END);
     
// Interrupt signal for CPU, high for some ticks when last frame is drawn
assign frameDrawn = (v_count == 0 && h_count < INTERRUPT_TICKS);
 

endmodule