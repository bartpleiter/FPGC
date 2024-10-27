/*
* NTSC encoder
*/
module NTSC(
    input               clk, //14.31818MHz
    output wire [7:0]   ntscBase,
    output wire         colorburst,
    output wire 		hsync,
    output wire         vsync,
    output wire 		hactive,
    output wire 		vactive,
    output reg [11:0]   hcount,
    output reg [11:0] 	vcount,
    output wire         frameDrawn
);

//assign hcountDiv2 = hcount >> 1; // divide by 2 to get 320 pixels instead of 640

localparam HLINECYCLES      = 910;  // hline: 910 cycles for 63.555us (227.5 color burst cycles), 0.3V
localparam VLINES           = 263;  // frame: 263 lines per frame, all the same, for 240P
localparam HSYNCCYCLES      = 67;   // hsync: first 67 cycles of line, 0V
localparam VSYNCCYCLES      = 421;  // vsync: first line: 421 cycles, apparently no hsync needed, 0V
localparam HACTIVESTART     = 196;  // 0.35V Black to 1V White
localparam HACTIVEDURATION  = 640;  // 
localparam VACTIVESTART     = 40;   // 
localparam VACTIVEDURATION  = 200;  // 
localparam COLORBURSTSTART  = 70;   // start a bit after hsync, 4.7+ (2.2/2) = 5.8, but a bit earlier, because extra cycle
localparam COLORBURSTDURATION = 60; // 10 color burst cycles = 10*4 = 36


//wire hsync;
//wire vsync;
assign hsync    = hcount < HSYNCCYCLES;
assign vsync 	= (vcount == 12'd0) && (hcount < VSYNCCYCLES);
assign hactive 	= (hcount >= HACTIVESTART) && (hcount < (HACTIVESTART + HACTIVEDURATION));
assign vactive 	= (vcount >= VACTIVESTART) && (vcount < (VACTIVESTART + VACTIVEDURATION));
assign colorburst = !vsync && (hcount >= COLORBURSTSTART) && (hcount < (COLORBURSTSTART + COLORBURSTDURATION));


assign ntscBase =   (hsync || vsync) ? 8'd0: // low on sync
                    //(hactive && vactive && vcount > 8'd200) ? 8'b11011001: // 255-38 for color modulation
                    //(hactive) ? 8'd89: // officially: 8'd89 black level -> 0.35v
                    8'd72; // blank level -> 0.3v (77)

//assign colorActive = vactive && hcount >= HACTIVESTART + 64 && hcount < HACTIVESTART + 576;

always @(posedge clk)
begin
    if (hcount != HLINECYCLES - 1)
        hcount <= hcount + 1'b1;
    else 
    begin
        hcount <= 0;
        if (vcount != VLINES - 1)
            vcount <= vcount + 1'b1;
        else
            vcount <= 0;
    end
end

initial
begin
    hcount = 12'd0;
    vcount = 12'd0;
end

assign frameDrawn = (vcount == 0 && hcount < 12'd32);

endmodule
