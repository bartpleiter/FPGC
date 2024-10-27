/*
* Memory Unit
*/
module MemoryUnit(
    // Clocks
    input           clk,
    input           reset,

    // Bus
    input [26:0]    bus_addr,
    input [31:0]    bus_data,
    input           bus_we,
    input           bus_start,
    output [31:0]   bus_q,
    output reg      bus_done = 1'b0,

    /********
    * MEMORY
    ********/

    //SPI Flash / SPI0
    inout           SPIflash_data, SPIflash_q, SPIflash_wp, SPIflash_hold,
    output          SPIflash_cs,
    output          SPIflash_clk,

    //VRAM32 cpu port
    output [31:0]   VRAM32_cpu_d,
    output [13:0]   VRAM32_cpu_addr,
    output          VRAM32_cpu_we,
    input  [31:0]   VRAM32_cpu_q,

    //VRAM8 cpu port
    output [7:0]    VRAM8_cpu_d,
    output [13:0]   VRAM8_cpu_addr,
    output          VRAM8_cpu_we,
    input  [7:0]    VRAM8_cpu_q,

    //VRAMspr cpu port
    output [8:0]    VRAMspr_cpu_d,
    output [13:0]   VRAMspr_cpu_addr,
    output          VRAMspr_cpu_we,
    input  [8:0]    VRAMspr_cpu_q,

    //VRAMpx cpu port
    output [23:0]   VRAMpx_cpu_d,
    output [16:0]   VRAMpx_cpu_addr,
    output          VRAMpx_cpu_we,
    input  [23:0]   VRAMpx_cpu_q,

    //ROM
    output [8:0]    ROM_addr,
    input  [31:0]   ROM_q,

    /********
    * I/O
    ********/

    //UART0 (Main USB)
    input           UART0_in,
    output          UART0_out,
    output          UART0_rx_interrupt,

    //UART1 (APU) DEPRECATED
    //input           UART1_in,
    //output          UART1_out,
    //output          UART1_rx_interrupt,

    //UART2 (GP)
    input           UART2_in,
    output          UART2_out,
    output          UART2_rx_interrupt,

    //SPI0 (Flash)
    //declared under MEMORY
    output          SPI0_QSPI,

    //SPI1 (USB0/CH376T)
    output          SPI1_clk,
    output reg      SPI1_cs = 1'b1,
    output          SPI1_mosi,
    input           SPI1_miso,
    input           SPI1_nint,

    //SPI2 (USB1/CH376T)
    output          SPI2_clk,
    output reg      SPI2_cs = 1'b1,
    output          SPI2_mosi,
    input           SPI2_miso,
    input           SPI2_nint,

    //SPI3 (W5500)
    output          SPI3_clk,
    output reg      SPI3_cs = 1'b1,
    output          SPI3_mosi,
    input           SPI3_miso,
    input           SPI3_int,

    //SPI4 (EXT/GP)
    output          SPI4_clk,
    output reg      SPI4_cs = 1'b1,
    output          SPI4_mosi,
    input           SPI4_miso,
    input           SPI4_GP,

    //GPIO (Separated GPI and GPO until GPIO module is implemented)
    input [3:0]     GPI,
    output reg [3:0]GPO = 4'd0,

    //OStimers
    output          OST1_int,
    output          OST2_int,
    output          OST3_int,

    //PS/2
    input           PS2_clk, PS2_data,
    output          PS2_int,            //Scan code ready signal

    output reg halfRes = 1'b0,

    //Boot mode
    input           boot_mode

);

// Address select parameters
localparam 
    A_SDRAM = 0,
    A_FLASH = 1,
    A_VRAM32 = 2,
    A_VRAM8 = 3,
    A_VRAMSPR = 4,
    A_ROM = 5,
    A_UART0RX = 6,
    A_UART0TX = 7,
    //A_UART1RX = 8,
    //A_UART1TX = 9,
    A_UART2RX = 10,
    A_UART2TX = 11,
    A_SPI0 = 12,
    A_SPI0CS = 13,
    A_SPI0EN = 14,
    A_SPI1 = 15,
    A_SPI1CS = 16,
    A_SPI1NINT = 17,
    A_SPI2 = 18,
    A_SPI2CS = 19,
    A_SPI2NINT = 20,
    A_SPI3 = 21,
    A_SPI3CS = 22,
    A_SPI3INT = 23,
    A_SPI4 = 24,
    A_SPI4CS = 25,
    A_SPI4GP = 26,
    A_GPIO = 27,
    A_GPIODIR = 28,
    A_TIMER1VAL = 29,
    A_TIMER1CTRL = 30,
    A_TIMER2VAL = 31,
    A_TIMER2CTRL = 32,
    A_TIMER3VAL = 33,
    A_TIMER3CTRL = 34,
    //A_SNESPAD = 35,
    A_PS2 = 36,
    A_BOOTMODE = 37,
    A_VRAMPX = 38,
    A_FPDIVWA = 39,
    A_FPDIVSTART = 40,
    A_IDIVWA = 41,
    A_IDIVSTARTS = 42,
    A_IDIVSTARTU = 43,
    A_IDIVMODS = 44,
    A_IDIVMODU = 45,
    A_HALFRES = 46,
    A_MILLIS = 47;

//------------
//SPI0 (flash) TODO: move this to a separate module
//------------

//SPIreader
wire [23:0] SPIflashReader_addr;            //address of flash (32 bit)
wire        SPIflashReader_start;           //start signal for SPIreader
wire        SPIflashReader_cs;              //cs
wire [31:0] SPIflashReader_q;               //data out
wire        SPIflashReader_initDone;        //initdone of SPIreader
wire        SPIflashReader_recvDone;        //recvdone of SPIreader TODO might change this to busy
wire        SPIflashReader_reset;           //reset SPIreader
wire        SPIflashReader_write;           //output mode of inout pins (high when writing to SPI flash)
wire        SPIflashReader_clk;             //clk for spi flash

wire io0_out, io1_out, io2_out, io3_out;    //d, q wp, hold output
wire io0_in,  io1_in,  io2_in,  io3_in;     //d, q wp, hold input

SPIreader sreader (
.clk        (clk),
.reset      (SPIflashReader_reset),
.cs         (SPIflashReader_cs),
.address    (SPIflashReader_addr),
.instr      (SPIflashReader_q),
.start      (SPIflashReader_start),
.initDone   (SPIflashReader_initDone),
.recvDone   (SPIflashReader_recvDone),
.write      (SPIflashReader_write),
.spi_clk    (SPIflashReader_clk),
.io0_out    (io0_out),
.io1_out    (io1_out),
.io2_out    (io2_out),
.io3_out    (io3_out),
.io0_in     (io0_in),
.io1_in     (io1_in),
.io2_in     (io2_in),
.io3_in     (io3_in)
);

//SPI0 (flash)
wire SPI0_clk;
wire SPI0_mosi;
reg  SPI0_cs = 1'b1;
reg  SPI0_enable = 1'b0; //high enables SPI0 and disables SPIreader
wire SPI0_start;
wire [7:0] SPI0_in;
wire [7:0] SPI0_out;
wire SPI0_done;

assign SPI0_QSPI = ~SPI0_enable;

SimpleSPI #(
.CLKS_PER_HALF_BIT(1))
SPI0(
.clk        (clk),
.reset      (reset),
.in_byte    (SPI0_in),
.start      (SPI0_start),
.done       (SPI0_done),
.out_byte   (SPI0_out),
.spi_clk    (SPI0_clk),
.miso       (SPIflash_q),
.mosi       (SPI0_mosi)
);

//Tri-state signals
wire SPIcombined_d, SPIcombined_q, SPIcombined_wp, SPIcombined_hold, SPIcombined_OutputEnable;

assign SPIflash_clk             = (SPI0_enable) ? SPI0_clk  : SPIflashReader_clk;
assign SPIflash_cs              = (SPI0_enable) ? SPI0_cs   : SPIflashReader_cs;
assign SPIflashReader_reset     = (SPI0_enable) ? 1'b1      : reset;

assign SPIcombined_d            = (SPI0_enable) ? SPI0_mosi : io0_out;
assign SPIcombined_q            = (SPI0_enable) ? 1'bz      : io1_out;
assign SPIcombined_wp           = (SPI0_enable) ? 1'b1      : io2_out;
assign SPIcombined_hold         = (SPI0_enable) ? 1'b1      : io3_out;
assign SPIcombined_OutputEnable = (SPI0_enable) ? 1'b1      : SPIflashReader_write;

assign SPIflash_data = (SPIcombined_OutputEnable) ? SPIcombined_d    : 1'bz;
assign SPIflash_q    = (SPIcombined_OutputEnable) ? SPIcombined_q    : 1'bz;
assign SPIflash_wp   = (SPIcombined_OutputEnable) ? SPIcombined_wp   : 1'bz;
assign SPIflash_hold = (SPIcombined_OutputEnable) ? SPIcombined_hold : 1'bz;

assign io0_in = (~SPIcombined_OutputEnable) ? SPIflash_data : 1'bz;
assign io1_in = (~SPIcombined_OutputEnable) ? SPIflash_q    : 1'bz;
assign io2_in = (~SPIcombined_OutputEnable) ? SPIflash_wp   : 1'bz;
assign io3_in = (~SPIcombined_OutputEnable) ? SPIflash_hold : 1'bz;


//------------
//UART0
//------------
wire UART0_r_Tx_DV, UART0_w_Tx_Done;
wire [7:0] UART0_r_Tx_Byte;

UARTtx UART0_tx(
.i_Clock    (clk),
.reset      (reset),
.i_Tx_DV    (UART0_r_Tx_DV),
.i_Tx_Byte  (UART0_r_Tx_Byte),
.o_Tx_Active(),
.o_Tx_Serial(UART0_out),
.o_Tx_Done  (UART0_w_Tx_Done)
);

wire [7:0] UART0_w_Rx_Byte;

UARTrx UART0_rx(
.i_Clock    (clk),
.reset      (reset),
.i_Rx_Serial(UART0_in),
.o_Rx_DV    (UART0_rx_interrupt),
.o_Rx_Byte  (UART0_w_Rx_Byte)
);


//------------
//UART1
//------------
/*
wire UART1_r_Tx_DV, UART1_w_Tx_Done;
wire [7:0] UART1_r_Tx_Byte;

UARTtx UART1_tx(
.i_Clock    (clk),
.reset      (reset),
.i_Tx_DV    (UART1_r_Tx_DV),
.i_Tx_Byte  (UART1_r_Tx_Byte),
.o_Tx_Active(),
.o_Tx_Serial(UART1_out),
.o_Tx_Done  (UART1_w_Tx_Done)
);

wire [7:0] UART1_w_Rx_Byte;

UARTrx UART1_rx(
.i_Clock    (clk),
.reset      (reset),
.i_Rx_Serial(UART1_in),
.o_Rx_DV    (UART1_rx_interrupt),
.o_Rx_Byte  (UART1_w_Rx_Byte)
);
*/


//------------
//UART2
//------------
wire UART2_r_Tx_DV, UART2_w_Tx_Done;
wire [7:0] UART2_r_Tx_Byte;

UARTtx UART2_tx(
.i_Clock    (clk),
.reset      (reset),
.i_Tx_DV    (UART2_r_Tx_DV),
.i_Tx_Byte  (UART2_r_Tx_Byte),
.o_Tx_Active(),
.o_Tx_Serial(UART2_out),
.o_Tx_Done  (UART2_w_Tx_Done)
);

wire [7:0] UART2_w_Rx_Byte;

UARTrx UART2_rx(
.i_Clock    (clk),
.reset      (reset),
.i_Rx_Serial(UART2_in),
.o_Rx_DV    (UART2_rx_interrupt),
.o_Rx_Byte  (UART2_w_Rx_Byte)
);




//------------
//SPI1 (CH376T bottom)
//------------
wire SPI1_start;
wire [7:0] SPI1_in;
wire [7:0] SPI1_out;
wire SPI1_done;

SimpleSPI #(
.CLKS_PER_HALF_BIT(2))
SPI1(
.clk        (clk),
.reset      (reset),
.in_byte    (SPI1_in),
.start      (SPI1_start),
.done       (SPI1_done),
.out_byte   (SPI1_out),
.spi_clk    (SPI1_clk),
.miso       (SPI1_miso),
.mosi       (SPI1_mosi)
);


//------------
//SPI2 (CH376T top)
//------------
wire SPI2_start;
wire [7:0] SPI2_in;
wire [7:0] SPI2_out;
wire SPI2_done;

SimpleSPI #(
.CLKS_PER_HALF_BIT(2))
SPI2(
.clk        (clk),
.reset      (reset),
.in_byte    (SPI2_in),
.start      (SPI2_start),
.done       (SPI2_done),
.out_byte   (SPI2_out),
.spi_clk    (SPI2_clk),
.miso       (SPI2_miso),
.mosi       (SPI2_mosi)
);


//------------
//SPI3 (W5500)
//------------
wire SPI3_start;
wire [7:0] SPI3_in;
wire [7:0] SPI3_out;
wire SPI3_done;

SimpleSPI #(
.CLKS_PER_HALF_BIT(1))
SPI3(
.clk        (clk),
.reset      (reset),
.in_byte    (SPI3_in),
.start      (SPI3_start),
.done       (SPI3_done),
.out_byte   (SPI3_out),
.spi_clk    (SPI3_clk),
.miso       (SPI3_miso),
.mosi       (SPI3_mosi)
);


//------------
//SPI4 (EXT/GP)
//------------
wire SPI4_start;
wire [7:0] SPI4_in;
wire [7:0] SPI4_out;
wire SPI4_done;

SimpleSPI #(
.CLKS_PER_HALF_BIT(2))
SPI4(
.clk        (clk),
.reset      (reset),
.in_byte    (SPI4_in),
.start      (SPI4_start),
.done       (SPI4_done),
.out_byte   (SPI4_out),
.spi_clk    (SPI4_clk),
.miso       (SPI4_miso),
.mosi       (SPI4_mosi)
);




//------------
//GPIO
//------------
// TODO: To be implemented




//------------
//OS timer 1
//------------
wire OST1_trigger, OST1_set;
wire [31:0] OST1_value;

OStimer OST1(
.clk        (clk),
.reset      (reset),
.timerValue (OST1_value),
.setValue   (OST1_set),
.trigger    (OST1_trigger),
.interrupt  (OST1_int)
);


//------------
//OS timer 2
//------------
wire OST2_trigger, OST2_set;
wire [31:0] OST2_value;

OStimer OST2(
.clk        (clk),
.reset      (reset),
.timerValue (OST2_value),
.setValue   (OST2_set),
.trigger    (OST2_trigger),
.interrupt  (OST2_int)
);


//------------
//OS timer 3
//------------
wire OST3_trigger, OST3_set;
wire [31:0] OST3_value;

OStimer OST3(
.clk        (clk),
.reset      (reset),
.timerValue (OST3_value),
.setValue   (OST3_set),
.trigger    (OST3_trigger),
.interrupt  (OST3_int)
);


//------------
//Millis counter
//------------
wire [31:0] millis;

MillisCounter millisCounter(
.clk        (clk),
.reset      (reset),
.millis     (millis)
);

//------------
//SNES controller
//------------
/*
wire [15:0] SNES_state;
wire SNES_done;
wire SNES_start;

NESpadReader npr (
.clk(clk),
.reset(reset),
.nesc(SNES_clk),
.nesl(SNES_latch),
.nesd(SNES_data),
.nesState(SNES_state),
.frame(SNES_start),
.done(SNES_done)
);*/




//------------
//PS/2 keyboard
//------------
wire [7:0] PS2_scanCode;

Keyboard PS2Keyboard (
.clk            (clk),
.reset          (reset),
.ps2d           (PS2_data),
.ps2c           (PS2_clk),
.rx_done_tick   (PS2_int),
.rx_data        (PS2_scanCode)
);


wire [31:0] fpdiv_input;
wire fpdiv_write_a;
wire fpdiv_busy;
wire fpdiv_start;

wire [31:0] fpdiv_val;


FPDivider fpdivider(
.clk        (clk), 
.rst        (reset),
.start      (fpdiv_start),  // start calculation
.write_a    (fpdiv_write_a),
.busy       (fpdiv_busy),   // calculation in progress
//.done       (fpdiv_done),   // calculation is complete (high for one tick)
//.valid      (valid),  // result is valid
//.dbz        (dbz),    // divide by zero
//.ovf        (ovf),    // overflow
.a_in       (bus_data),   // dividend (numerator)
.b          (bus_data),   // divisor (denominator)
.val        (fpdiv_val)  // result value: quotient
);



wire [31:0] idiv_input;
wire idiv_write_a;
wire idiv_ready;
wire idiv_start;
wire idiv_signed;

wire [31:0] idiv_q;
wire [31:0] idiv_r;


IDivider idivider(
.clk        (clk), 
.rst        (reset),
.start      (idiv_start),  // start calculation
.write_a    (idiv_write_a),
.ready      (idiv_ready),   // !calculation in progress
.flush      (1'b0),
.signed_ope (idiv_signed), // signed or unsiged
.a          (bus_data),   // dividend (numerator)
.b          (bus_data),   // divisor (denominator)
.quotient   (idiv_q),  // result value: quotient
.remainder  (idiv_r)
);


reg [31:0] bus_d_reg = 32'd0;

//----
//MEMORY
//----

//SPI FLASH MEMORY
assign SPIflashReader_addr  = bus_addr - 27'h800000;
assign SPIflashReader_start = bus_addr >= 27'h800000 && bus_addr < 27'hC00000 && bus_start;

//VRAM32
assign VRAM32_cpu_addr      = bus_addr - 27'hC00000;
assign VRAM32_cpu_d         = bus_d_reg;
assign VRAM32_cpu_we        = bus_addr >= 27'hC00000 && bus_addr < 27'hC00420 && bus_we;

//VRAM8
assign VRAM8_cpu_addr       = bus_addr - 27'hC00420;
assign VRAM8_cpu_d          = bus_data;
assign VRAM8_cpu_we         = bus_addr >= 27'hC00420 && bus_addr < 27'hC02422 && bus_we;

//VRAMspr
assign VRAMspr_cpu_addr     = bus_addr - 27'hC02422;
assign VRAMspr_cpu_d        = bus_data;
assign VRAMspr_cpu_we       = bus_addr >= 27'hC02422 && bus_addr < 27'hC02522 && bus_we;

//VRAMpx
assign VRAMpx_cpu_addr     = bus_addr - 27'hD00000;
assign VRAMpx_cpu_d        = bus_data;
assign VRAMpx_cpu_we       = bus_addr >= 27'hD00000 && bus_addr < 27'hD12C00 && bus_we;

//ROM
assign ROM_addr             = bus_addr - 27'hC02522;


//----
//I/O
//----

//UART
assign UART0_r_Tx_DV    = bus_addr == 27'hC02723 && bus_we && bus_start;
assign UART0_r_Tx_Byte  = bus_data;


//assign UART1_r_Tx_DV    = bus_addr == 27'hC02725 && bus_we && bus_start;
//assign UART1_r_Tx_Byte  = bus_data;

assign UART2_r_Tx_DV    = bus_addr == 27'hC02727 && bus_we && bus_start;
assign UART2_r_Tx_Byte  = bus_data;

//SPI
assign SPI0_in          = bus_data;
assign SPI0_start       = bus_addr == 27'hC02728 && bus_we && bus_start;

assign SPI1_in          = bus_data;
assign SPI1_start       = bus_addr == 27'hC0272B && bus_we && bus_start;

assign SPI2_in          = bus_data;
assign SPI2_start       = bus_addr == 27'hC0272E && bus_we && bus_start;

assign SPI3_in          = bus_data;
assign SPI3_start       = bus_addr == 27'hC02731 && bus_we && bus_start;

assign SPI4_in          = bus_data;
assign SPI4_start       = bus_addr == 27'hC02734 && bus_we && bus_start;

//OS Timers
assign OST1_value       = bus_data;
assign OST1_set         = (bus_addr == 27'hC02739 && bus_we);
assign OST1_trigger     = (bus_addr == 27'hC0273A && bus_we);

assign OST2_value       = bus_data;
assign OST2_set         = (bus_addr == 27'hC0273B && bus_we);
assign OST2_trigger     = (bus_addr == 27'hC0273C && bus_we);

assign OST3_value       = bus_data;
assign OST3_set         = (bus_addr == 27'hC0273D && bus_we);
assign OST3_trigger     = (bus_addr == 27'hC0273E && bus_we);

//SNES
//assign SNES_start       = bus_addr == 27'hC0273F && bus_start;

//Divider
assign fpdiv_write_a      = (bus_addr == 27'hC02742 && bus_we);
assign fpdiv_start        = (bus_addr == 27'hC02743 && bus_we);

assign idiv_write_a      = (bus_addr == 27'hC02744 && bus_we);
assign idiv_start        = (
                            (bus_addr == 27'hC02745 ||
                                bus_addr == 27'hC02746 || 
                                bus_addr == 27'hC02747 || 
                                bus_addr == 27'hC02748
                            ) 
                            && bus_we
                           );
assign idiv_signed       = ((bus_addr == 27'hC02745 || bus_addr == 27'hC02747) && bus_we);


reg [5:0] a_sel;

// Address selection
always @(bus_addr)
begin
    a_sel = 6'd0;
    if (bus_addr < 27'h800000) a_sel = A_SDRAM;
    if (bus_addr >= 27'h800000 && bus_addr < 27'hC00000) a_sel = A_FLASH;
    if (bus_addr >= 27'hC00000 && bus_addr < 27'hC00420) a_sel = A_VRAM32;
    if (bus_addr >= 27'hC00420 && bus_addr < 27'hC02422) a_sel = A_VRAM8;
    if (bus_addr >= 27'hC02422 && bus_addr < 27'hC02522) a_sel = A_VRAMSPR;
    if (bus_addr >= 27'hC02522 && bus_addr < 27'hC02722) a_sel = A_ROM;
    if (bus_addr == 27'hC02722) a_sel = A_UART0RX;
    if (bus_addr == 27'hC02723) a_sel = A_UART0TX;
    //if (bus_addr == 27'hC02724) a_sel = A_UART1RX;
    //if (bus_addr == 27'hC02725) a_sel = A_UART1TX;
    if (bus_addr == 27'hC02726) a_sel = A_UART2RX;
    if (bus_addr == 27'hC02727) a_sel = A_UART2TX;
    if (bus_addr == 27'hC02728) a_sel = A_SPI0;
    if (bus_addr == 27'hC02729) a_sel = A_SPI0CS;
    if (bus_addr == 27'hC0272A) a_sel = A_SPI0EN;
    if (bus_addr == 27'hC0272B) a_sel = A_SPI1;
    if (bus_addr == 27'hC0272C) a_sel = A_SPI1CS;
    if (bus_addr == 27'hC0272D) a_sel = A_SPI1NINT;
    if (bus_addr == 27'hC0272E) a_sel = A_SPI2;
    if (bus_addr == 27'hC0272F) a_sel = A_SPI2CS;
    if (bus_addr == 27'hC02730) a_sel = A_SPI2NINT;
    if (bus_addr == 27'hC02731) a_sel = A_SPI3;
    if (bus_addr == 27'hC02732) a_sel = A_SPI3CS;
    if (bus_addr == 27'hC02733) a_sel = A_SPI3INT;
    if (bus_addr == 27'hC02734) a_sel = A_SPI4;
    if (bus_addr == 27'hC02735) a_sel = A_SPI4CS;
    if (bus_addr == 27'hC02736) a_sel = A_SPI4GP;
    if (bus_addr == 27'hC02737) a_sel = A_GPIO;
    if (bus_addr == 27'hC02738) a_sel = A_GPIODIR;
    if (bus_addr == 27'hC02739) a_sel = A_TIMER1VAL;
    if (bus_addr == 27'hC0273A) a_sel = A_TIMER1CTRL;
    if (bus_addr == 27'hC0273B) a_sel = A_TIMER2VAL;
    if (bus_addr == 27'hC0273C) a_sel = A_TIMER2CTRL;
    if (bus_addr == 27'hC0273D) a_sel = A_TIMER3VAL;
    if (bus_addr == 27'hC0273E) a_sel = A_TIMER3CTRL;
    //if (bus_addr == 27'hC0273F) a_sel = A_SNESPAD;
    if (bus_addr == 27'hC02740) a_sel = A_PS2;
    if (bus_addr == 27'hC02741) a_sel = A_BOOTMODE;
    if (bus_addr == 27'hC02742) a_sel = A_FPDIVWA;
    if (bus_addr == 27'hC02743) a_sel = A_FPDIVSTART;
    if (bus_addr == 27'hC02744) a_sel = A_IDIVWA;
    if (bus_addr == 27'hC02745) a_sel = A_IDIVSTARTS;
    if (bus_addr == 27'hC02746) a_sel = A_IDIVSTARTU;
    if (bus_addr == 27'hC02747) a_sel = A_IDIVMODS;
    if (bus_addr == 27'hC02748) a_sel = A_IDIVMODU;
    if (bus_addr == 27'hC02749) a_sel = A_HALFRES;
    if (bus_addr == 27'hC0274A) a_sel = A_MILLIS;
    if (bus_addr >= 27'hD00000 && bus_addr < 27'hD12C00) a_sel = A_VRAMPX;
end

reg [31:0] bus_q_wire;
reg [31:0] bus_q_wire_reg = 32'd0;
always @(*)
begin
    case (a_sel)
        A_SDRAM:        bus_q_wire = 32'd0; //sd_q; sdram is removed now!
        A_FLASH:        bus_q_wire = SPIflashReader_q;
        A_VRAM32:       bus_q_wire = VRAM32_cpu_q;
        A_VRAM8:        bus_q_wire = VRAM8_cpu_q;
        A_VRAMSPR:      bus_q_wire = VRAMspr_cpu_q;
        A_ROM:          bus_q_wire = ROM_q;
        A_UART0RX:      bus_q_wire = UART0_w_Rx_Byte;
        //A_UART0TX:      bus_q_wire = 
        //A_UART1RX:      bus_q_wire = UART1_w_Rx_Byte;
        //A_UART1TX:      bus_q_wire = 
        A_UART2RX:      bus_q_wire = UART2_w_Rx_Byte;
        //A_UART2TX:      bus_q_wire = 
        A_SPI0:         bus_q_wire = SPI0_out;
        A_SPI0CS:       bus_q_wire = SPI0_cs;
        A_SPI0EN:       bus_q_wire = SPI0_enable;
        A_SPI1:         bus_q_wire = SPI1_out;
        A_SPI1CS:       bus_q_wire = SPI1_cs;
        A_SPI1NINT:     bus_q_wire = SPI1_nint;
        A_SPI2:         bus_q_wire = SPI2_out;
        A_SPI2CS:       bus_q_wire = SPI2_cs;
        A_SPI2NINT:     bus_q_wire = SPI2_nint;
        A_SPI3:         bus_q_wire = SPI3_out;
        A_SPI3CS:       bus_q_wire = SPI3_cs;
        A_SPI3INT:      bus_q_wire = SPI3_int;
        A_SPI4:         bus_q_wire = SPI4_out;
        A_SPI4CS:       bus_q_wire = SPI4_cs;
        A_SPI4GP:       bus_q_wire = SPI4_GP;
        A_GPIO:         bus_q_wire = {24'd0, GPO, GPI};
        //A_GPIODIR:      bus_q_wire = 
        //A_TIMER1VAL:    bus_q_wire = 
        //A_TIMER1CTRL:   bus_q_wire = 
        //A_TIMER2VAL:    bus_q_wire = 
        //A_TIMER2CTRL:   bus_q_wire = 
        //A_TIMER3VAL:    bus_q_wire = 
        //A_TIMER3CTRL:   bus_q_wire = 
        //A_SNESPAD:      bus_q_wire = {16'd0, SNES_state};
        A_PS2:          bus_q_wire = {24'd0, PS2_scanCode};
        A_BOOTMODE:     bus_q_wire = {31'd0, boot_mode};
        A_VRAMPX:       bus_q_wire = VRAMpx_cpu_q;
        A_FPDIVSTART:   bus_q_wire = fpdiv_val;
        A_IDIVSTARTS:   bus_q_wire = idiv_q;
        A_IDIVSTARTU:   bus_q_wire = idiv_q;
        A_IDIVMODS:     bus_q_wire = idiv_r;
        A_IDIVMODU:     bus_q_wire = idiv_r;
        A_MILLIS:       bus_q_wire = millis;
        default:        bus_q_wire = 32'd0;
    endcase
end

always @(posedge clk)
begin
    if (reset)
    begin
        bus_q_wire_reg <= 32'd0;
        bus_d_reg <= 32'd0;
    end
    else
    begin
        bus_d_reg <= bus_data; // latch for copy instructions to SRAM/regs

        // latch output
        if (bus_done || bus_done_next || SPIflashReader_recvDone) // TODO: Should probably add more ready statements here
            bus_q_wire_reg <= bus_q_wire;
    end
end

reg bus_done_next = 1'b0;

assign bus_q =      (a_sel == A_ROM) ? ROM_q: // safe because ROM cannot be the destination of a copy instruction
                    bus_q_wire_reg;


always @(posedge clk)
begin
    if (reset)
    begin
        GPO         <= 4'd0;
        SPI0_enable <= 1'b0;
        bus_done <= 1'b0;
        bus_done_next <= 1'b0;
        SPI0_cs     <= 1'b1;
        SPI1_cs     <= 1'b1;
        SPI2_cs     <= 1'b1;
        SPI3_cs     <= 1'b1;
        SPI4_cs     <= 1'b1;
        //TODO: add reset
        
    end
    else
    begin

        if (bus_done_next)
        begin
            bus_done_next <= 1'b0;
            bus_done <= 1'b1;
        end
        else
        begin
            bus_done <= 1'b0;
        end

        if (bus_start)
        begin
            case (a_sel)
                A_SDRAM:
                begin
                    bus_done <= 1'b0; // this is to make sure bus_done from MU will never be used when SDRAM access
                end
                A_FLASH:
                begin
                    if (SPIflashReader_recvDone || SPI0_enable)
                        bus_done <= 1'b1;
                end

                A_UART0TX:
                begin
                    if (UART0_w_Tx_Done)
                        bus_done <= 1'b1;
                end

                /*
                A_UART1TX:
                begin
                    if (UART1_w_Tx_Done)
                        bus_done <= 1'b1;
                end
                */

                A_UART2TX:
                begin
                    if (UART2_w_Tx_Done)
                        bus_done <= 1'b1;
                end

                A_SPI0:
                begin
                    if (SPI0_done)
                        if (!bus_done_next) bus_done_next <= 1'b1;
                end

                A_SPI0CS:
                begin
                    if (bus_we)
                    begin
                        SPI0_cs <= bus_data[0];
                    end
                    if (!bus_done_next) bus_done_next <= 1'b1;
                end

                A_SPI0EN:
                begin
                    if (bus_we)
                    begin
                        SPI0_enable <= bus_data[0];
                    end
                    if (!bus_done_next) bus_done_next <= 1'b1;
                end

                A_SPI1:
                begin
                    if (SPI1_done)
                        if (!bus_done_next) bus_done_next <= 1'b1;
                end

                A_SPI1CS:
                begin
                    if (bus_we)
                    begin
                        SPI1_cs <= bus_data[0];
                    end
                    if (!bus_done_next) bus_done_next <= 1'b1;
                end

                A_SPI2:
                begin
                    if (SPI2_done)
                        if (!bus_done_next) bus_done_next <= 1'b1;
                end

                A_SPI2CS:
                begin
                    if (bus_we)
                    begin
                        SPI2_cs <= bus_data[0];
                    end
                    if (!bus_done_next) bus_done_next <= 1'b1;
                end

                A_SPI3:
                begin
                    if (SPI3_done)
                        if (!bus_done_next) bus_done_next <= 1'b1;
                end

                A_SPI3CS:
                begin
                    if (bus_we)
                    begin
                        SPI3_cs <= bus_data[0];
                    end
                    if (!bus_done_next) bus_done_next <= 1'b1;
                end

                A_SPI4:
                begin
                    if (SPI4_done)
                        if (!bus_done_next) bus_done_next <= 1'b1;
                end

                A_SPI4CS:
                begin
                    if (bus_we)
                    begin
                        SPI4_cs <= bus_data[0];
                    end
                    if (!bus_done_next) bus_done_next <= 1'b1;
                end

                A_GPIO:
                begin
                    if (bus_we)
                    begin
                        GPO <= bus_data[7:4];
                    end
                        if (!bus_done_next) bus_done_next <= 1'b1;
                end

                /*
                A_SNESPAD:
                begin
                    if (SNES_done)
                        bus_done <= 1'b1;
                end
                */

                A_VRAM8, A_VRAM32, A_VRAMSPR, A_VRAMPX:
                begin
                    if (bus_we)
                        bus_done <= 1'b1;
                    else
                        if (!bus_done_next) bus_done_next <= 1'b1;
                end

                A_ROM:
                begin
                    bus_done <= 1'b1;
                end

                A_FPDIVSTART:
                begin
                    if (!fpdiv_busy)
                        if (!bus_done_next) bus_done_next <= 1'b1;
                end

                A_IDIVSTARTS, A_IDIVSTARTU, A_IDIVMODS, A_IDIVMODU:
                begin
                    if (idiv_ready)
                        if (!bus_done_next) bus_done_next <= 1'b1;
                end

                A_HALFRES:
                begin
                    if (bus_we)
                    begin
                        halfRes <= bus_data[0];
                    end
                        if (!bus_done_next) bus_done_next <= 1'b1;
                end

                default:
                begin
                    if (!bus_done_next) bus_done_next <= 1'b1;
                end

            endcase

                
        end

    end

end

endmodule