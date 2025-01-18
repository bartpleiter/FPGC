/*
* Data Memory
* Types of memory to be accessed:
* 1. ROM [read only] (single cycle)
* 2. L1 Data Cache [read/write] (2 100MHz cycles or more depening on busy state of SDRAM controller or cache)
* 3. VRAM (32, 32-2, 8, px) [read/write] (single cycle)
* 4. Memory Unit (MU) [read/write] (>=2 cycles)
*/

module DataMem(
    input wire          clk, clk100, reset,
    input wire  [26:0]  pc,
    input wire  [26:0]  addr,
    input wire          we,
    input wire          re,
    input wire  [31:0]  data,
    output wire [31:0]  q,
    output              busy,

    // ROM
    output [8:0]    bus_d_rom_addr,
    input [31:0]    bus_d_rom_q,

    // L1 Data Cache
    output [23:0]   bus_l1d_addr,
    output          bus_l1d_start,
    output [31:0]   bus_l1d_data,
    output          bus_l1d_we,
    input [31:0]    bus_l1d_q,
    input           bus_l1d_done,
    input           bus_l1d_ready,

    // VRAM32 cpu port
    output [10:0]   VRAM32_cpu_addr,
    output [31:0]   VRAM32_cpu_d,
    output          VRAM32_cpu_we,
    input  [31:0]   VRAM32_cpu_q,

    // VRAM8 cpu port
    output [13:0]   VRAM8_cpu_addr,
    output [7:0]    VRAM8_cpu_d,
    output          VRAM8_cpu_we,
    input  [7:0]    VRAM8_cpu_q,

    // VRAMspr cpu port
    output [7:0]    VRAMspr_cpu_addr,
    output [8:0]    VRAMspr_cpu_d,
    output          VRAMspr_cpu_we,
    input  [8:0]    VRAMspr_cpu_q,

    // VRAMpx cpu port
    output [16:0]   VRAMpx_cpu_addr,
    output [7:0]    VRAMpx_cpu_d,
    output          VRAMpx_cpu_we,
    input  [7:0]    VRAMpx_cpu_q,

    // Memory Unit
    output [26:0]   bus_mu_addr,
    output          bus_mu_start,
    output [31:0]   bus_mu_data,
    output          bus_mu_we,
    input [31:0]    bus_mu_q,
    input           bus_mu_done,
    input           bus_mu_ready,

    input wire      clear, hold
);

// Memory address ranges
localparam SDRAM_START = 32'h0000000;
localparam SDRAM_END = 32'h1000000;

localparam ROM_START = 32'h1000000;
localparam ROM_END = 32'h1100000;

localparam VRAM32_START = 32'h1100000;
localparam VRAM32_END = 32'h1200000;

localparam VRAM8_START = 32'h1200000;
localparam VRAM8_END = 32'h1300000;

localparam VRAMspr_START = 32'h1300000;
localparam VRAMspr_END = 32'h1400000;

localparam VRAMpx_START = 32'h1400000;
localparam VRAMpx_END = 32'h2000000;

localparam MU_START = 32'h2000000;

// Address in range selection
wire in_range_l1d = addr < SDRAM_END;
wire in_range_rom = addr >= ROM_START && addr < ROM_END;
wire in_range_vram32 = addr >= VRAM32_START && addr < VRAM32_END;
wire in_range_vram8 = addr >= VRAM8_START && addr < VRAM8_END;
wire in_range_vramspr = addr >= VRAMspr_START && addr < VRAMspr_END;
wire in_range_vrampx = addr >= VRAMpx_START && addr < VRAMpx_END;
wire in_range_mu = addr >= MU_START;

wire in_range_single_cycle = (addr >= ROM_START && addr < MU_START);

// Forward address to all memories
assign bus_d_rom_addr = addr;
assign bus_l1d_addr = addr;
assign VRAM32_cpu_addr = addr;
assign VRAM8_cpu_addr = addr;
assign VRAMspr_cpu_addr = addr;
assign VRAMpx_cpu_addr = addr;
assign bus_mu_addr = addr;

// Forward data to all memories
assign bus_l1d_data = data;
assign VRAM32_cpu_d = data;
assign VRAM8_cpu_d = data;
assign VRAMspr_cpu_d = data;
assign VRAMpx_cpu_d = data;
assign bus_mu_data = data;

// Forward write enable if transaction requires start signal
assign bus_l1d_we = we;
assign bus_mu_we = we;
// Forward write enable if transaction is in range for single cycle memories without start signal
assign VRAM32_cpu_we = (in_range_vram32) ? we : 1'b0;
assign VRAM8_cpu_we = (in_range_vram8) ? we : 1'b0;
assign VRAMspr_cpu_we = (in_range_vramspr) ? we : 1'b0;
assign VRAMpx_cpu_we = (in_range_vrampx) ? we : 1'b0;

wire [31:0] q_wire = (q_override_l1d) ? q_override_l1d_data:
          (q_override_mu) ? q_override_mu_data:
          (in_range_l1d) ? bus_l1d_q : 
          (in_range_rom) ? bus_d_rom_q : 
          (in_range_vram32) ? VRAM32_cpu_q : 
          (in_range_vram8) ? VRAM8_cpu_q : 
          (in_range_vramspr) ? VRAMspr_cpu_q : 
          (in_range_vrampx) ? VRAMpx_cpu_q : 
          bus_mu_q;

assign q = q_wire;

// localparam STATE_IDLE = 3'b000;
// localparam STATE_WAIT = 3'b001;
// reg [2:0] state = STATE_IDLE;

wire multicycle_bus_done = (bus_l1d_done || bus_mu_done);

reg we_prev = 1'b0;
reg re_prev = 1'b0;
reg [26:0] pc_prev = 27'h0;

wire rw_strobe = (we && !we_prev) || (re && !re_prev) || ((we || re) && (pc != pc_prev));

reg multicycle_bus_done_latch = 1'b0;
reg bus_l1d_done_latch = 1'b0;
reg bus_mu_done_latch = 1'b0;

assign bus_l1d_start = (in_range_l1d && rw_strobe);

assign busy = (!in_range_single_cycle && (we || re) && !(multicycle_bus_done || multicycle_bus_done_latch) && !bus_l1d_start);

reg [31:0] q_l1d_latch = 32'd0;
reg [31:0] q_mu_latch = 32'd0;


wire q_override_l1d = bus_l1d_done || bus_l1d_done_latch;
wire q_override_mu = bus_mu_done || bus_mu_done_latch;
wire [31:0] q_override_l1d_data = bus_l1d_q || q_l1d_latch;
wire [31:0] q_override_mu_data = bus_mu_q || q_mu_latch;

always @(posedge clk100)
begin
    if (reset)
    begin
        we_prev <= 1'b0;
        re_prev <= 1'b0;
        pc_prev <= 27'h0;
        multicycle_bus_done_latch <= 1'b0;
        bus_l1d_done_latch <= 1'b0;
        bus_mu_done_latch <= 1'b0;
        q_l1d_latch <= 32'd0;
        q_mu_latch <= 32'd0;
    end
    else
    begin
        we_prev <= we;
        re_prev <= re;
        multicycle_bus_done_latch <= multicycle_bus_done;
        bus_l1d_done_latch <= bus_l1d_done;
        bus_mu_done_latch <= bus_mu_done;
        pc_prev <= pc;
        q_l1d_latch <= bus_l1d_q;
        q_mu_latch <= bus_mu_q;
    end
end
endmodule

