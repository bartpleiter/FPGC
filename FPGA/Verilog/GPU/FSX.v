/*
* Graphical processor (Frame Synthesizer)
* Generates video from VRAM
*/
module FSX(
    //Clocks
    input clkPixel,
    input clkTMDShalf,
    //input clk14,
    //input clk114,
    input clkMuxOut,

    //HDMI
    output [3:0] TMDS_p,
    output [3:0] TMDS_n,

    //NTSC composite
    //output [7:0] composite,

    //Select output method
    //input selectOutput,

    input halfRes,

    //VRAM32
    output [13:0]       vram32_addr,
    input  [31:0]       vram32_q, 

    //VRAM322
    output [13:0]       vram322_addr,
    input  [31:0]       vram322_q, 

    //VRAM8
    output [13:0]       vram8_addr,
    input  [7:0]        vram8_q,

    //VRAMSPR
    output [13:0]       vramSPR_addr,
    input  [8:0]        vramSPR_q,

    //VRAMpixel
    output [16:0]       vramPX_addr,
    input  [23:0]       vramPX_q,

    //Interrupt signal
    output              frameDrawn
);

wire selectOutput = 1'b1; // always HDMI, as I no longer wish to include NTSC as a tiny HDMI monitor is now used as primary display

// LVDS Converter
wire [3:0] TMDS;

lvds lvdsConverter(
    .datain     (TMDS),
    .dataout    (TMDS_n),
    .dataout_b  (TMDS_p) // Reversed because of a LVDS polarity swap on the V3 PCB
);


wire [11:0] h_count_hdmi;
wire [11:0] v_count_hdmi;

wire hsync_hdmi;
wire vsync_hdmi;
wire csync;
wire blank_hdmi;
wire frameDrawn_hdmi;

TimingGenerator timingGenerator(
    // Clock
    .clkPixel(clkPixel),

    // Position counters
    .h_count(h_count_hdmi),
    .v_count(v_count_hdmi),

    // Video signals
    .hsync(hsync_hdmi),
    .vsync(vsync_hdmi),
    .csync(csync),
    .blank(blank_hdmi),
    
    // Interrupt signal
    .frameDrawn(frameDrawn_hdmi)
);

/*
wire [2:0] r_ntsc;
wire [2:0] g_ntsc;
wire [1:0] b_ntsc;


wire frameDrawn_ntsc;
wire [11:0] h_count_ntsc;
wire [11:0] v_count_ntsc;

wire hsync_ntsc;
wire vsync_ntsc;
wire blank_ntsc;


RGB332toNTSC rgb2ntsc(
    .clk(clk14), //14.318MHz
    .clkColor(clk114), //114.5454MHz
    .r(r_ntsc),
    .g(g_ntsc),
    .b(b_ntsc),
    .hcount(h_count_ntsc),
    .vcount(v_count_ntsc),
    .hs(hsync_ntsc),
    .vs(vsync_ntsc),
    .blank(blank_ntsc),
    .composite(composite), // video output signal
    .frameDrawn(frameDrawn_ntsc) // interrupt signal
);
*/

wire hsync;
wire vsync;
wire blank;
wire [11:0] h_count;
wire [11:0] v_count;

/*
assign frameDrawn   = (selectOutput == 1'b1) ? frameDrawn_hdmi : frameDrawn_ntsc;
assign hsync        = (selectOutput == 1'b1) ? hsync_hdmi : hsync_ntsc;
assign vsync        = (selectOutput == 1'b1) ? vsync_hdmi : ~vsync_ntsc; // ntsc vsync is inverted
assign blank        = (selectOutput == 1'b1) ? blank_hdmi : blank_ntsc;
assign h_count      = (selectOutput == 1'b1) ? h_count_hdmi : h_count_ntsc;
assign v_count      = (selectOutput == 1'b1) ? v_count_hdmi : v_count_ntsc;
*/

assign frameDrawn   = frameDrawn_hdmi;
assign hsync        = hsync_hdmi;
assign vsync        = vsync_hdmi;
assign blank        = blank_hdmi;
assign h_count      = h_count_hdmi;
assign v_count      = v_count_hdmi;



wire [2:0] BGW_r;
wire [2:0] BGW_g;
wire [1:0] BGW_b;


BGWrenderer bgwrenderer(
    // Video I/O
    .clk(clkMuxOut),
    .hs(hsync),
    .vs(vsync),
    .blank(blank),

    .scale2x(selectOutput),
    
    // Output colors
    .r(BGW_r),
    .g(BGW_g),
    .b(BGW_b),

    .h_count(h_count),  // line position in pixels including blanking 
    .v_count(v_count),  // frame position in lines including blanking 

    // VRAM32
    .vram32_addr(vram32_addr),
    .vram32_q(vram32_q), 

    // VRAM8
    .vram8_addr(vram8_addr),
    .vram8_q(vram8_q)
);


wire [7:0] PX_r;
wire [7:0] PX_g;
wire [7:0] PX_b;


PixelEngine pixelEngine(
    // Video I/O
    .clk(clkMuxOut),
    .hs(hsync),
    .vs(vsync),
    .blank(blank),

    .scale2x(selectOutput),
    .halfRes(halfRes),
    
    // Output colors
    .r(PX_r),
    .g(PX_g),
    .b(PX_b),

    .h_count(h_count),  // line position in pixels including blanking 
    .v_count(v_count),  // frame position in lines including blanking 

    // VRAM
    .vram_addr(vramPX_addr),
    .vram_q(vramPX_q)
);


// Give priority to pixel plane if bgw plane is black
wire pxPriority = (BGW_r == 3'd0 && BGW_g == 3'd0 && BGW_b == 2'd0);

wire [7:0] BGW_r_Byte;
wire [7:0] BGW_g_Byte;
wire [7:0] BGW_b_Byte;

assign BGW_r_Byte = (BGW_r == 3'd0) ?  {BGW_r, 5'b00000} : {BGW_r, 5'b11111};
assign BGW_g_Byte = (BGW_g == 3'd0) ?  {BGW_g, 5'b00000} : {BGW_g, 5'b11111};
assign BGW_b_Byte = (BGW_b == 2'd0) ?  {BGW_b, 6'b000000} : {BGW_b, 6'b111111};

wire [7:0] rByte;
wire [7:0] gByte;
wire [7:0] bByte;

assign rByte = (pxPriority) ? PX_r: BGW_r_Byte;
assign gByte = (pxPriority) ? PX_g: BGW_g_Byte;
assign bByte = (pxPriority) ? PX_b : BGW_b_Byte;


// Convert VGA signal to HDMI signals
RGB2HDMI rgb2hdmi(
    .clkTMDS(clkTMDShalf),
    .clkRGB (clkPixel),
    .rRGB   (rByte),
    .gRGB   (gByte),
    .bRGB   (bByte),
    .blk    (blank_hdmi),
    .hs     (hsync_hdmi),
    .vs     (vsync_hdmi), 
    .bTMDS  (TMDS[0]),
    .gTMDS  (TMDS[1]),
    .rTMDS  (TMDS[2]),
    .cTMDS  (TMDS[3])
);


// Image file generator for simulation
integer file;
integer framecounter = 0;

// HDMI
always @(negedge vsync_hdmi)
begin
    if (selectOutput == 1'b1)
    begin
        file = $fopen($sformatf("/home/bart/Documents/FPGA/FPGC6/Verilog/output/frame%0d.ppm", framecounter), "w");
        $fwrite(file, "P3\n");
        $fwrite(file, "640 480\n");
        $fwrite(file, "255\n");
        framecounter = framecounter + 1;
    end
end

always @(posedge clkPixel)
begin
    if (selectOutput == 1'b1)
    begin
        if (~blank_hdmi)
        begin
            $fwrite(file, "%d  %d  %d\n", rByte, gByte, bByte);
        end
    end
end

/*
wire [7:0] rByte_ntsc;
wire [7:0] gByte_ntsc;
wire [7:0] bByte_ntsc;
assign rByte_ntsc = (r_ntsc == 3'd0) ?  {r_ntsc, 5'b00000} : {r_ntsc, 5'b11111};
assign gByte_ntsc = (g_ntsc == 3'd0) ?  {g_ntsc, 5'b00000} : {g_ntsc, 5'b11111};
assign bByte_ntsc = (b_ntsc == 2'd0) ?  {b_ntsc, 6'b000000} : {b_ntsc, 6'b111111};

// NTSC
always @(negedge vsync_ntsc)
begin
    if (selectOutput == 1'b0)
    begin
        file = $fopen($sformatf("/home/bart/Documents/FPGA/FPGC6/Verilog/output/frame%0d.ppm", framecounter), "w");
        $fwrite(file, "P3\n");
        $fwrite(file, "320 240\n");
        $fwrite(file, "255\n");
        framecounter = framecounter + 1;
    end
end

always @(posedge clkPixel)
begin
    if (selectOutput == 1'b0)
    begin
        if (~blank_ntsc)
        begin
            $fwrite(file, "%d  %d  %d\n", rByte_ntsc, gByte_ntsc, bByte_ntsc);
        end
    end
end
*/

endmodule