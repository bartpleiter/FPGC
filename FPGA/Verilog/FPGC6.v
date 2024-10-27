/*
* Top level design of the FPGC6
*/
module FPGC6(
    input           clk, //50MHz
    input           clk_SDRAM, //100MHz
    input           nreset,

    //HDMI
    output wire [3:0] TMDS_p,
    output wire [3:0] TMDS_n,

    //SDRAM
    output          SDRAM_CLK,
    output          SDRAM_CSn,
    output          SDRAM_WEn,
    output          SDRAM_CASn,
    output          SDRAM_RASn,
    output          SDRAM_CKE,
    output [12:0]   SDRAM_A,
    output [1:0]    SDRAM_BA,
    output [3:0]    SDRAM_DQM,
    inout  [31:0]   SDRAM_DQ,

    //SPI0 flash
    output          SPI0_clk,
    output          SPI0_cs,
    inout           SPI0_data,
    inout           SPI0_q,
    inout           SPI0_wp,
    inout           SPI0_hold,
     
    //SPI1 CH376 bottom
    output          SPI1_clk,
    output          SPI1_cs,
    output          SPI1_mosi,
    input           SPI1_miso,
    input           SPI1_nint,
    output          SPI1_rst,
     
    //SPI2 CH376 top
    output          SPI2_clk,
    output          SPI2_cs,
    output          SPI2_mosi,
    input           SPI2_miso,
    input           SPI2_nint,
    output          SPI2_rst,
     
    //SPI3 W5500
    output          SPI3_clk,
    output          SPI3_cs,
    output          SPI3_mosi,
    input           SPI3_miso,
    input           SPI3_int,
    output          SPI3_nrst,
     
    //SPI4 GP
    output          SPI4_clk,
    output          SPI4_cs,
    output          SPI4_mosi,
    input           SPI4_miso,
    input           SPI4_gp,
     
    //UART0
    input           UART0_in,
    output          UART0_out,
    input           UART0_dtr,
     
    //UART1 (currently unused because no UART midi synth anymore)
    //input           UART1_in,
    //output          UART1_out,
     
    //UART2
    input           UART2_in,
    output          UART2_out,
     
    //PS/2
    input           PS2_clk, PS2_data,
     
    //Led for debugging
    output          led,
     
    //GPIO
    input [3:0]     GPI,
    output [3:0]    GPO,
     
    //DIP switch
    input [3:0]     DIPS,

    //I2S audio
    output          I2S_SDIN, I2S_SCLK, I2S_LRCLK, I2S_MCLK,
    
    //Status leds
    output          led_Booted, led_Eth, led_Flash, led_USB0, led_USB1, led_PS2, led_HDMI, led_QSPI, led_GPU, led_I2S
);


// TMP FIXES FOR NEW PCB
assign I2S_SDIN = 1'b0;
assign I2S_SCLK = 1'b0;
assign I2S_LRCLK = 1'b0;
assign I2S_MCLK = 1'b0;


//-------------------CLK-------------------------
//In hardware a PLL should be used here
// to create the clk and crt_clk 
//assign crt_clk = clk; //'fix' for simulation

//Run VGA at CRT speed
//assign vga_clk = crt_clk;

wire clkTMDShalf;   // TMDS clock (pre-DDR), 5x pixel clock
wire clkPixel;  // Pixel clock

wire clk14; // NTSC clock
wire clk114; // NTSC color clock

wire clkMuxOut; // HDMI or NTSC clock, depending on selectOutput

// everything at clk speed for simulation
assign clkTMDShalf = clk;
assign clkPixel = clk;
assign clk14 = clk;
assign clk114 = clk;
assign clkMuxOut = clk;

//Run SDRAM at 100MHz
assign SDRAM_CLK = clk_SDRAM;


//--------------------Reset&Stabilizers-----------------------
//Reset signals
wire nreset_stable, UART0_dtr_stable, reset;

//Dip switch
wire boot_mode_stable;

//GPU: High when frame just rendered (needs to be stabilized)
wire frameDrawn, frameDrawn_stable;

//Stabilized SPI interrupt signals
wire SPI1_nint_stable, SPI2_nint_stable, SPI3_int_stable, SPI4_gp_stable; 

MultiStabilizer multistabilizer (
.clk(clk),
.u0(nreset),
.s0(nreset_stable),
.u1(UART0_dtr),
.s1(UART0_dtr_stable),
.u2(SPI1_nint),
.s2(SPI1_nint_stable),
.u3(SPI2_nint),
.s3(SPI2_nint_stable),
.u4(SPI3_int),
.s4(SPI3_int_stable),
.u5(SPI4_gp),
.s5(SPI4_gp_stable),
.u6(frameDrawn),
.s6(frameDrawn_stable),
.u7(DIPS[0]),
.s7(boot_mode_stable)
);

//Indicator for opened Serial port
assign led = UART0_dtr_stable;

//DRT to reset pulse
wire dtrRst;

DtrReset dtrReset (
.clk(clk),
.dtr(UART0_dtr_stable),
.dtrRst(dtrRst)
);

assign reset = (~nreset_stable) || dtrRst;

//External reset outputs
assign SPI1_rst = reset;
assign SPI2_rst = reset;
assign SPI3_nrst = ~reset;


//---------------------------VRAM32---------------------------------
//VRAM32 I/O
wire        vram32_gpu_clk;
wire [13:0] vram32_gpu_addr;
wire [31:0] vram32_gpu_d;
wire        vram32_gpu_we;
wire [31:0] vram32_gpu_q;

wire        vram32_cpu_clk;
wire [13:0] vram32_cpu_addr;
wire [31:0] vram32_cpu_d;
wire        vram32_cpu_we; 
wire [31:0] vram32_cpu_q;

//because FSX will not write to VRAM
assign vram32_gpu_we    = 1'b0;
assign vram32_gpu_d     = 32'd0;

VRAM #(
.WIDTH(32),
.WORDS(1056),
.ADDR_BITS(14),
.LIST("/home/bart/Documents/FPGA/FPGC6/Verilog/memory/vram32.list")
)   vram32(
//CPU port
.cpu_clk    (clk),
.cpu_d      (vram32_cpu_d),
.cpu_addr   (vram32_cpu_addr),
.cpu_we     (vram32_cpu_we),
.cpu_q      (vram32_cpu_q),

//GPU port
.gpu_clk    (clkMuxOut),
.gpu_d      (vram32_gpu_d),
.gpu_addr   (vram32_gpu_addr),
.gpu_we     (vram32_gpu_we),
.gpu_q      (vram32_gpu_q)
);


//---------------------------VRAM322--------------------------------
//VRAM322 I/O
wire        vram322_gpu_clk;
wire [13:0] vram322_gpu_addr;
wire [31:0] vram322_gpu_d;
wire        vram322_gpu_we;
wire [31:0] vram322_gpu_q;

//because FSX will not write to VRAM
assign vram322_gpu_we    = 1'b0;
assign vram322_gpu_d     = 32'd0;

VRAM #(
.WIDTH(32),
.WORDS(1056),
.ADDR_BITS(14),
.LIST("/home/bart/Documents/FPGA/FPGC6/Verilog/memory/vram32.list")
)   vram322(
//CPU port
.cpu_clk    (clk),
.cpu_d      (vram32_cpu_d),
.cpu_addr   (vram32_cpu_addr),
.cpu_we     (vram32_cpu_we),
.cpu_q      (),

//GPU port
.gpu_clk    (clkMuxOut),
.gpu_d      (vram322_gpu_d),
.gpu_addr   (vram322_gpu_addr),
.gpu_we     (vram322_gpu_we),
.gpu_q      (vram322_gpu_q)
);


//--------------------------VRAM8--------------------------------
//VRAM8 I/O
wire        vram8_gpu_clk;
wire [13:0] vram8_gpu_addr;
wire [7:0]  vram8_gpu_d;
wire        vram8_gpu_we;
wire [7:0]  vram8_gpu_q;

wire        vram8_cpu_clk;
wire [13:0] vram8_cpu_addr;
wire [7:0]  vram8_cpu_d;
wire        vram8_cpu_we;
wire [7:0]  vram8_cpu_q;

//because FSX will not write to VRAM
assign vram8_gpu_we     = 1'b0;
assign vram8_gpu_d      = 8'd0;

VRAM #(
.WIDTH(8),
.WORDS(8194),
.ADDR_BITS(14),
.LIST("/home/bart/Documents/FPGA/FPGC6/Verilog/memory/vram8.list")
)   vram8(
//CPU port
.cpu_clk    (clk),
.cpu_d      (vram8_cpu_d),
.cpu_addr   (vram8_cpu_addr),
.cpu_we     (vram8_cpu_we),
.cpu_q      (vram8_cpu_q),

//GPU port
.gpu_clk    (clkMuxOut),
.gpu_d      (vram8_gpu_d),
.gpu_addr   (vram8_gpu_addr),
.gpu_we     (vram8_gpu_we),
.gpu_q      (vram8_gpu_q)
);


//--------------------------VRAMSPR--------------------------------
//VRAMSPR I/O
wire        vramSPR_gpu_clk;
wire [13:0] vramSPR_gpu_addr;
wire [8:0]  vramSPR_gpu_d;
wire        vramSPR_gpu_we;
wire [8:0]  vramSPR_gpu_q;

wire        vramSPR_cpu_clk;
wire [13:0] vramSPR_cpu_addr;
wire [8:0]  vramSPR_cpu_d;
wire        vramSPR_cpu_we;
wire [8:0]  vramSPR_cpu_q;

//because FSX will not write to VRAM
assign vramSPR_gpu_we     = 1'b0;
assign vramSPR_gpu_d      = 9'd0;

VRAM #(
.WIDTH(9),
.WORDS(256),
.ADDR_BITS(14),
.LIST("/home/bart/Documents/FPGA/FPGC6/Verilog/memory/vramSPR.list")
)   vramSPR(
//CPU port
.cpu_clk    (clk),
.cpu_d      (vramSPR_cpu_d),
.cpu_addr   (vramSPR_cpu_addr),
.cpu_we     (vramSPR_cpu_we),
.cpu_q      (vramSPR_cpu_q),

//GPU port
.gpu_clk    (clkMuxOut),
.gpu_d      (vramSPR_gpu_d),
.gpu_addr   (vramSPR_gpu_addr),
.gpu_we     (vramSPR_gpu_we),
.gpu_q      (vramSPR_gpu_q)
);


//--------------------------VRAMPX--------------------------------
//VRAMPX I/O
wire        vramPX_gpu_clk;
wire [16:0] vramPX_gpu_addr;
wire [23:0]  vramPX_gpu_d;
wire        vramPX_gpu_we;
wire [23:0]  vramPX_gpu_q;

wire        vramPX_cpu_clk;
wire [16:0] vramPX_cpu_addr;
wire [23:0]  vramPX_cpu_d;
wire        vramPX_cpu_we;
wire [23:0]  vramPX_cpu_q;

// FSX will not write to VRAM
assign vramPX_gpu_we     = 1'b0;
assign vramPX_gpu_d      = 24'd0;

VRAM #(
.WIDTH(24),
.WORDS(76800),
.ADDR_BITS(17),
.LIST("memory/vramPX.list")
) vramPX(
// CPU port
.cpu_clk    (clk),
.cpu_d      (vramPX_cpu_d),
.cpu_addr   (vramPX_cpu_addr),
.cpu_we     (vramPX_cpu_we),
.cpu_q      (vramPX_cpu_q),

// GPU port
.gpu_clk    (clkMuxOut),
.gpu_d      (vramPX_gpu_d),
.gpu_addr   (vramPX_gpu_addr),
.gpu_we     (vramPX_gpu_we),
.gpu_q      (vramPX_gpu_q)
);


//-------------------ROM-------------------------
//ROM I/O
wire [8:0] rom_addr;
wire [31:0] rom_q;


ROM rom(
.clk            (clk),
.reset          (reset),
.address        (rom_addr),
.q              (rom_q)
);


//----------------SDRAM Controller------------------
// inputs
wire [23:0]      sdc_addr;  // address to write or to start reading from
wire [31:0]      sdc_data;  // data to write
wire             sdc_we;    // write enable
wire             sdc_start; // start trigger

// outputs
wire [31:0]     sdc_q;      // memory output
wire            sdc_done;   // output ready

SDRAMcontroller sdramcontroller(
// clock/reset inputs
.clk        (clk_SDRAM),
.reset      (reset),

// interface inputs
.sdc_addr   (sdc_addr),
.sdc_data   (sdc_data),
.sdc_we     (sdc_we),
.sdc_start  (sdc_start),

// interface outputs
.sdc_q      (sdc_q),
.sdc_done   (sdc_done),

// SDRAM signals
.SDRAM_CKE  (SDRAM_CKE),
.SDRAM_CSn  (SDRAM_CSn),
.SDRAM_WEn  (SDRAM_WEn),
.SDRAM_CASn (SDRAM_CASn),
.SDRAM_RASn (SDRAM_RASn),
.SDRAM_A    (SDRAM_A),
.SDRAM_BA   (SDRAM_BA),
.SDRAM_DQM  (SDRAM_DQM),
.SDRAM_DQ   (SDRAM_DQ)
);


//-----------------------FSX-------------------------
//FSX I/O
wire [7:0]  composite; // NTSC composite video signal
reg         selectOutput = 1'b1; // 1 -> HDMI, 0 -> Composite
wire        halfRes;

FSX fsx(
//Clocks
.clkPixel       (clkPixel),
.clkTMDShalf    (clkTMDShalf),
//.clk14          (clk14),
//.clk114         (clk114),
.clkMuxOut      (clkMuxOut),


//HDMI
.TMDS_p         (TMDS_p),
.TMDS_n         (TMDS_n),

//NTSC composite
//.composite      (composite),

//Select output method
//.selectOutput   (selectOutput),

.halfRes(halfRes),

//VRAM32
.vram32_addr    (vram32_gpu_addr),
.vram32_q       (vram32_gpu_q),

//VRAM32
.vram322_addr   (vram322_gpu_addr),
.vram322_q      (vram322_gpu_q),

//VRAM8
.vram8_addr     (vram8_gpu_addr),
.vram8_q        (vram8_gpu_q),

//VRAMSPR
.vramSPR_addr   (vramSPR_gpu_addr),
.vramSPR_q      (vramSPR_gpu_q),

//VRAMPX
.vramPX_addr   (vramPX_gpu_addr),
.vramPX_q      (vramPX_gpu_q),

//Interrupt signal
.frameDrawn     (frameDrawn)
);


//----------------Memory Unit--------------------
//Memory Unit I/O

//Bus
wire [26:0] bus_addr;
wire [31:0] bus_data;
wire        bus_we;
wire        bus_start;
wire [31:0] bus_q;
wire        bus_done;

//Interrupt signals
wire        OST1_int, OST2_int, OST3_int;
wire        UART0_rx_int, UART2_rx_int;
wire        PS2_int;
wire        SPI0_QSPI;

MemoryUnit mu(
//clock
.clk            (clk),
.reset          (reset),

//CPU connection (Bus)
.bus_addr       (bus_addr),
.bus_data       (bus_data),
.bus_we         (bus_we),
.bus_start      (bus_start),
.bus_q          (bus_q),
.bus_done       (bus_done),

/********
* MEMORY
********/

//SPI Flash / SPI0
.SPIflash_data  (SPI0_data), 
.SPIflash_q     (SPI0_q), 
.SPIflash_wp    (SPI0_wp), 
.SPIflash_hold  (SPI0_hold),
.SPIflash_cs    (SPI0_cs), 
.SPIflash_clk   (SPI0_clk),

//VRAM32 cpu port
.VRAM32_cpu_d       (vram32_cpu_d),
.VRAM32_cpu_addr    (vram32_cpu_addr), 
.VRAM32_cpu_we      (vram32_cpu_we),
.VRAM32_cpu_q       (vram32_cpu_q),

//VRAM8 cpu port
.VRAM8_cpu_d        (vram8_cpu_d),
.VRAM8_cpu_addr     (vram8_cpu_addr), 
.VRAM8_cpu_we       (vram8_cpu_we),
.VRAM8_cpu_q        (vram8_cpu_q),

//VRAMspr cpu port
.VRAMspr_cpu_d      (vramSPR_cpu_d),
.VRAMspr_cpu_addr   (vramSPR_cpu_addr), 
.VRAMspr_cpu_we     (vramSPR_cpu_we),
.VRAMspr_cpu_q      (vramSPR_cpu_q),

// VRAMpx cpu port
.VRAMpx_cpu_d      (vramPX_cpu_d),
.VRAMpx_cpu_addr   (vramPX_cpu_addr),
.VRAMpx_cpu_we     (vramPX_cpu_we),
.VRAMpx_cpu_q      (vramPX_cpu_q),

//ROM
.ROM_addr           (rom_addr),
.ROM_q              (rom_q),

/********
* I/O
********/

//UART0 (Main USB)
.UART0_in           (UART0_in),
.UART0_out          (UART0_out),
.UART0_rx_interrupt (UART0_rx_int),

//UART1 (APU)
/*.UART1_in           (),
.UART1_out          (),
.UART1_rx_interrupt (),
*/

//UART2 (GP)
.UART2_in           (UART2_in),
.UART2_out          (UART2_out),
.UART2_rx_interrupt (UART2_rx_int),

//SPI0 (Flash)
//declared under MEMORY
.SPI0_QSPI      (SPI0_QSPI),

//SPI1 (USB0/CH376T)
.SPI1_clk       (SPI1_clk),
.SPI1_cs        (SPI1_cs),
.SPI1_mosi      (SPI1_mosi),
.SPI1_miso      (SPI1_miso),
.SPI1_nint      (SPI1_nint_stable),

//SPI2 (USB1/CH376T)
.SPI2_clk       (SPI2_clk),
.SPI2_cs        (SPI2_cs),
.SPI2_mosi      (SPI2_mosi),
.SPI2_miso      (SPI2_miso),
.SPI2_nint      (SPI2_nint_stable),

//SPI3 (W5500)
.SPI3_clk       (SPI3_clk),
.SPI3_cs        (SPI3_cs),
.SPI3_mosi      (SPI3_mosi),
.SPI3_miso      (SPI3_miso),
.SPI3_int       (SPI3_int_stable),

//SPI4 (EXT/GP)
.SPI4_clk       (SPI4_clk),
.SPI4_cs        (SPI4_cs),
.SPI4_mosi      (SPI4_mosi),
.SPI4_miso      (SPI4_miso),
.SPI4_GP        (SPI4_gp_stable),

//GPIO (Separated GPI and GPO until GPIO module is implemented)
.GPI        (GPI[3:0]),
.GPO        (GPO[3:0]),

//OStimers
.OST1_int   (OST1_int),
.OST2_int   (OST2_int),
.OST3_int   (OST3_int),

//SNESpad
/*
.SNES_clk   (),
.SNES_latch (),
.SNES_data  (),
*/

//PS/2
.PS2_clk    (PS2_clk),
.PS2_data   (PS2_data),
.PS2_int    (PS2_int), //Scan code ready signal

.halfRes(halfRes),

//Boot mode
.boot_mode  (boot_mode_stable)
);


//------------L2 Cache--------------
//CPU bus
wire [23:0]      l2_addr;  // address to write or to start reading from
wire [31:0]      l2_data;  // data to write
wire             l2_we;    // write enable
wire             l2_start; // start trigger
wire [31:0]      l2_q;     // memory output
wire             l2_done;  // output ready

L2cache l2cache(
.clk            (clk_SDRAM),
.reset          (reset),

// CPU bus
.l2_addr       (l2_addr),
.l2_data       (l2_data),
.l2_we         (l2_we),
.l2_start      (l2_start),
.l2_q          (l2_q),
.l2_done       (l2_done),

// sdram bus
.sdc_addr       (sdc_addr),
.sdc_data       (sdc_data),
.sdc_we         (sdc_we),
.sdc_start      (sdc_start),
.sdc_q          (sdc_q),
.sdc_done       (sdc_done)
);


//---------------CPU----------------
//CPU I/O
wire [26:0] PC;

CPU cpu(
.clk            (clk),
.reset          (reset),

// bus
.bus_addr       (bus_addr),
.bus_data       (bus_data),
.bus_we         (bus_we),
.bus_start      (bus_start),
.bus_q          (bus_q),
.bus_done       (bus_done),

// sdram bus
.sdc_addr       (l2_addr),
.sdc_data       (l2_data),
.sdc_we         (l2_we),
.sdc_start      (l2_start),
.sdc_q          (l2_q),
.sdc_done       (l2_done),

.int1           (OST1_int),            //OStimer1
.int2           (OST2_int),            //OStimer2
.int3           (UART0_rx_int),        //UART0 rx (MAIN)
.int4           (frameDrawn_stable),   //GPU Frame Drawn
.int5           (OST3_int),            //OStimer3
.int6           (PS2_int),             //PS/2 scancode ready
.int7           (1'b0),                //UART1 rx (APU)
.int8           (UART2_rx_int),        //UART2 rx (EXT)

.PC             (PC)
);


//-----------STATUS LEDS-----------
assign led_Booted = (PC >= 27'hC02522 | reset);
assign led_HDMI = (~selectOutput | reset);
assign led_QSPI = (~SPI0_QSPI | reset);

LEDvisualizer #(.MIN_CLK(100000))
LEDvisUSB0
(
.clk(clk),
.reset(reset),
.activity(~SPI1_cs),
.LED(led_USB0)
);

LEDvisualizer #(.MIN_CLK(100000))
LEDvisUSB1
(
.clk(clk),
.reset(reset),
.activity(~SPI2_cs),
.LED(led_USB1)
);

LEDvisualizer #(.MIN_CLK(100000))
LEDvisEth
(
.clk(clk),
.reset(reset),
.activity(~SPI3_cs),
.LED(led_Eth)
);

LEDvisualizer #(.MIN_CLK(100000))
LEDvisPS2
(
.clk(clk),
.reset(reset),
.activity(PS2_int),
.LED(led_PS2)
);

LEDvisualizer #(.MIN_CLK(100000))
LEDvisFlash
(
.clk(clk),
.reset(reset),
.activity(~SPI0_cs),
.LED(led_Flash)
);

LEDvisualizer #(.MIN_CLK(100000))
LEDvisGPU
(
.clk(clk),
.reset(reset),
.activity(vram32_cpu_we|vram8_cpu_we|vramSPR_cpu_we|vramPX_cpu_we),
.LED(led_GPU)
);

LEDvisualizer #(.MIN_CLK(100000))
LEDvisI2S
(
.clk(clk),
.reset(reset),
.activity(I2S_SDIN),
.LED(led_I2S)
);

endmodule