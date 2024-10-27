/*
* Converts RGB332 video into NTSC composite 240p video
*/
module RGB332toNTSC(
    input clk, //14.318MHz
    input clkColor, //114.5454MHz
    
    input [2:0] r,
    input [2:0] g,
    input [1:0] b,
    
    // counters for rendering
    output wire [11:0] hcount,
	output wire [11:0] vcount,
    output wire blank,
    output wire hs,
    output wire vs,
    output wire frameDrawn,
    
    output wire [7:0] composite // video output signal
);

wire clk14 = clk;
wire clk114 = clkColor;

//-------------------------
//NTSC BASE SIGNAL
//-------------------------

wire [7:0] ntscBase;
wire colorburst;
wire hactive;
wire vactive;

wire active;
assign active = hactive && vactive;
assign blank = ~active;

// Generate NTSC base signal
NTSC ntsc(
.clk        (clk14),
.ntscBase   (ntscBase),
.colorburst (colorburst),
.hactive    (hactive),
.vactive    (vactive),
.hcount     (hcount),
.vcount     (vcount),
.hsync      (hs),
.vsync      (vs),
.frameDrawn (frameDrawn)
);


//-------------------------
//COLOR PHASE
//-------------------------

wire colorPhase;
wire burstPhase; // base phase used for color burst

wire [4:0] phaseSelect;

PhaseGen phasegen(
.clk        (clk114), //114.5454MHz
.select     (phaseSelect), // phase select
.colorPhase (colorPhase), // phase of selected color
.burstPhase (burstPhase) // phase of color burst
);


//-------------------------
//COMBINER
//-------------------------
wire [7:0] luma;
wire [7:0] colorAmplitude;

RGBtoYPhaseAmpl rgb2yphampl(
  .rgb332({r,g,b}), // rrrgggbb
  .luma(luma),
  .phase(phaseSelect),
 .ampl(colorAmplitude)
);

            
// Apply clipping
wire [7:0] ntscPlusColor;
wire [7:0] ntscMinusColor;
wire [7:0] ntscPlusColorClipped;
wire [7:0] ntscMinusColorClipped;
wire ntscPlusColorOverflow;
wire ntscMinusColorOverflow;
assign {ntscPlusColorOverflow, ntscPlusColor} = {1'b0, luma} + {1'b0, colorAmplitude};
assign {ntscMinusColorOverflow, ntscMinusColor} = {1'b0, luma} - {1'b0, colorAmplitude};
assign ntscPlusColorClipped = (ntscPlusColorOverflow) ? 8'd255 : ntscPlusColor;
assign ntscMinusColorClipped = (ntscMinusColorOverflow) ? 8'd0 : ntscMinusColor;

// Combine signal based on burst or color phase
assign composite = (colorburst && burstPhase) ? ntscBase + 8'd36:
    (colorburst && ~burstPhase) ? ntscBase - 8'd36:
    (active && colorPhase) ? ntscPlusColorClipped:
    (active && ~colorPhase) ? ntscMinusColorClipped:
    ntscBase;

endmodule
