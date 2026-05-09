/*
 * MemoryUnit
 * Single interface between CPU and slow Memory or IO
 * Optimized for simplicity, not speed, as high speed memory should be directly connected to CPU
 */
module MemoryUnit (
    // ---- System interface ----
    input  wire         clk, // Assumed to be 100MHz
    input  wire         reset,
    output wire         uart_reset,

    // ---- CPU interface (50 MHz domain) ----
    input  wire         start,
    input  wire [31:0]  addr, // Address in CPU words
    input  wire [31:0]  data,
    input  wire         we,
    output reg  [31:0]  q = 32'd0,
    output reg          done = 1'b0,

    // ---- I/O signals ----
    // UART
    input wire          uart_rx,
    output wire         uart_tx,
    output wire         uart_irq,

    // Timer interrupts
    output wire         OST1_int,
    output wire         OST2_int,
    output wire         OST3_int,

    // Activity outputs for LEDs
    output reg          flash_spi_activity = 1'b0,
    output reg          usb_spi_activity = 1'b0,

    // User LED state
    output reg          user_led_state = 1'b0,

    // Boot mode signal
    input wire          boot_mode,

    // SPI0 (Flash 1)
    output wire         SPI0_clk,
    output reg          SPI0_cs = 1'b1,
    output wire         SPI0_mosi,
    input wire          SPI0_miso,

    // SPI1 (Flash 2) -- 4-bit bidirectional bus for QSPIflash.
    // IO0 = MOSI, IO1 = MISO, IO2 = WP_n (1-bit) / IO2 (QSPI),
    // IO3 = HOLD_n (1-bit) / IO3 (QSPI). Tristate is handled at the
    // FPGC top level (or in the testbench) using SPI1_io_oe.
    output wire         SPI1_clk,
    output reg          SPI1_cs = 1'b1,
    output wire [3:0]   SPI1_io_out,
    output wire [3:0]   SPI1_io_oe,
    input  wire [3:0]   SPI1_io_in,

    // SPI2 (USB Host 1)
    output wire         SPI2_clk,
    output reg          SPI2_cs = 1'b1,
    output wire         SPI2_mosi,
    input wire          SPI2_miso,
    input wire          SPI2_nint,

    // SPI5 (SD Card)
    output wire         SPI5_clk,
    output reg          SPI5_cs = 1'b1,
    output wire         SPI5_mosi,
    input wire          SPI5_miso,

    // ---- DMA peer-request ports (driven by DMAengine, see dma-implementation-plan.md §2.5) ----
    // I/O peer port: lets the DMA engine push transactions through MemoryUnit's IO state machine.
    input  wire         iop_start,
    input  wire         iop_we,
    input  wire [31:0]  iop_addr,
    input  wire [31:0]  iop_data,
    output wire         iop_done,
    output wire [31:0]  iop_q,

    // VRAMPX peer port: lets the DMA engine emit byte writes to the pixel framebuffer.
    input  wire         vp_we,
    input  wire [16:0]  vp_addr,
    input  wire [7:0]   vp_data,
    output wire         vramPX_dma_we,
    output wire [16:0]  vramPX_dma_addr,
    output wire [7:0]   vramPX_dma_d,

    // ---- DMA SPI burst port (direct, bypasses MemoryUnit's MMIO state machine) ----
    // The DMA engine uses this port to drive SimpleSPI2 burst-mode FIFOs
    // directly on SPI0/SPI1/SPI4. This avoids going through the per-byte
    // iop_* path, which races against CPU MMIO polls under interrupt
    // pressure (see Docs/plans/dma-bdos-spi1-hang-bug.md).
    //   dma_burst_spi_id   selects the target SPI controller (0,1,4).
    //   dma_burst_select   when 1, routes the engine's dma_burst_* signals
    //                      to the chosen SPI controller in place of the CPU
    //                      MMIO command port. The other controllers' dma
    //                      ports are tied off.
    input  wire [2:0]   dma_burst_spi_id,
    input  wire         dma_burst_select,
    input  wire         dma_burst_we,
    input  wire [7:0]   dma_burst_data,
    input  wire         dma_burst_start,
    input  wire [15:0]  dma_burst_len,
    input  wire         dma_burst_dummy,
    input  wire         dma_burst_re_rx,
    // QSPI Fast Read controls (only meaningful when dma_burst_select=1
    // and the targeted controller is QSPIflash, i.e. spi_id=1).
    input  wire         dma_burst_qspi_read,
    input  wire [23:0]  dma_burst_qspi_addr,
    output wire         dma_burst_tx_full,
    output wire         dma_burst_rx_empty,
    output wire [7:0]   dma_burst_rx_data,
    output wire [7:0]   dma_burst_rx_count,
    output wire         dma_burst_busy,
    output wire         dma_burst_done,

    // ---- DMA register-bus side (connects to DMAengine reg_*) ----
    output reg  [2:0]   dma_reg_addr = 3'd0,
    output reg          dma_reg_we   = 1'b0,
    output reg  [31:0]  dma_reg_data = 32'd0,
    input  wire [31:0]  dma_reg_q,

    // ---- Camera control (connects to camera modules in FPGC.v) ----
    output reg          cam_ctrl_enable    = 1'b0,
    input  wire         cam_frame_done,
    input  wire         cam_current_buf,

    // ---- I2C master (generic) ----
    output reg          i2c_start      = 1'b0,
    output reg          i2c_rw         = 1'b0,
    output reg  [6:0]   i2c_dev_addr   = 7'd0,
    output reg  [7:0]   i2c_reg_addr   = 8'd0,
    output reg  [7:0]   i2c_wr_data    = 8'd0,
    input  wire         i2c_busy,
    input  wire         i2c_ack_err,
    input  wire [7:0]   i2c_rd_data,
    input  wire [4:0]   i2c_dbg_state, // SCCB config sequence complete
    input  wire         cam_vsync_raw,      // Raw VSYNC pin (synchronized)
    input  wire         cam_href_raw,       // Raw HREF pin (synchronized)
    input  wire [2:0]   cam_dbg_state,          // CameraCapture FSM state
    input  wire [16:0]  cam_dbg_frame_pixels,   // Y pixels in last frame
    input  wire [8:0]   cam_dbg_line_count,     // Lines in last frame
    input  wire [11:0]  cam_dbg_cache_lines,    // Cache lines in last frame
    input  wire [7:0]   cam_dbg_partial_drops,  // Partial drops in last frame
    input  wire         sdram_arb_busy,     // SDRAMarbiter busy flag

    // ---- GPU timing (synchronized to clk100 in FPGC.v) ----
    input  wire         gpu_vblank,         // HIGH during vertical blanking
    input  wire [11:0]  gpu_v_count          // Current vertical line counter

    // TODO: GPIO
);

// cam_frame_done_latch is managed entirely in the main FSM below
reg cam_frame_done_latch = 1'b0;

// I2C start-pending flag: set when CPU writes I2C_CMD, cleared when
// i2c_busy rises.  Ensures post-write busy poll always sees busy=1.
reg i2c_start_pending = 1'b0;

// q_hold: latches the return value for STATE_RETURN_Q so it survives
// the default q<=0 assignment that runs every cycle.
reg [31:0] q_hold = 32'd0;

// ---- DMA peer-port plumbing (step 9: iop_* routes SPI0/SPI4 byte MMIO) ----
// The DMA engine drives iop_start/iop_we/iop_addr/iop_data with a single-byte
// transaction targeting either ADDR_SPI0_DATA or ADDR_SPI4_DATA. We route it
// through the same FSM the CPU uses by adding an extra branch in STATE_IDLE
// that fires only when the CPU is not starting a transaction. That gives us
// natural mutual exclusion against the CPU's own SPI MMIO.
//
// Wrinkle: the CPU's `start` is a 1-cycle pulse from MemoryStage.v (gated by
// `mu_io_started` until `mu_done`). If `start` fires while MemoryUnit is busy
// with an iop transaction (state != STATE_IDLE), the pulse would be lost and
// the CPU would deadlock. So we latch any CPU start that we couldn't accept
// immediately and replay it as soon as the FSM returns to IDLE.
reg         iop_done_r = 1'b0;
reg [31:0]  iop_q_r    = 32'd0;
reg         iop_active = 1'b0;
reg [2:0]   iop_spi_id = 3'd0;
assign iop_done = iop_done_r;
assign iop_q    = iop_q_r;

reg         cpu_req_pending = 1'b0;
// VRAMPX DMA pass-through (muxed with the CPU port at the FPGA top-level in step 8).
assign vramPX_dma_we   = vp_we;
assign vramPX_dma_addr = vp_addr;
assign vramPX_dma_d    = vp_data;

// ---- DMA SPI burst port demux ----
// Demux the engine's single dma_burst_* port to per-SPI dma_select strobes.
// Each SimpleSPI2 sees its own dma_select; only the chosen one is active
// during a burst. All other dma_* inputs to SimpleSPI2 are shared (tied to
// the engine's outputs) -- they're only sampled when dma_select=1.
wire dma_sel_spi0 = dma_burst_select && (dma_burst_spi_id == 3'd0);
wire dma_sel_spi1 = dma_burst_select && (dma_burst_spi_id == 3'd1);
wire dma_sel_spi5 = dma_burst_select && (dma_burst_spi_id == 3'd5);

// Per-SPI burst-side wires from SimpleSPI2 (declared here, hooked up at the
// instances below, mux'd back to the engine at the bottom of the file).
wire        SPI0_tx_full_w, SPI0_rx_empty_w, SPI0_busy_w;
wire [7:0]  SPI0_rx_data_w;
wire        SPI1_tx_full_w, SPI1_rx_empty_w, SPI1_busy_w;
wire [7:0]  SPI1_rx_data_w;
wire [7:0]  SPI1_rx_count_w;
wire        SPI5_tx_full_w, SPI5_rx_empty_w, SPI5_busy_w;
wire [7:0]  SPI5_rx_data_w;

// SPI*_done are declared further down alongside their SimpleSPI2 instances;
// the demux for dma_burst_done/busy/tx_full/rx_empty/rx_data lives at the
// bottom of this module after all SPI*_done wires exist.

// ---- IO devices ----
// UART TX
reg uart_tx_start = 1'b0;
wire uart_tx_done;
reg [7:0] uart_tx_data = 8'd0;

UARTtx uart_tx_controller (
    .clk    (clk),
    .reset  (reset),
    .start  (uart_tx_start),
    .data   (uart_tx_data),
    .done   (uart_tx_done),
    .tx     (uart_tx)
);

// UART RX
wire [7:0] uart_rx_q;

UARTrx uart_rx_controller (
    .clk    (clk),
    .reset  (reset),
    .rx     (uart_rx),
    .done   (uart_irq),
    .data   (uart_rx_q)
);

// UART reset is not used — reset is handled via DTR in the top module
assign uart_reset = 1'b0;

// OS timer 1
reg OST1_trigger = 1'b0;
reg OST1_set = 1'b0;
reg [31:0] OST1_value = 32'd0;

OStimer OST1 (
    .clk         (clk),
    .reset       (reset),
    .timer_value (OST1_value),
    .set_value   (OST1_set),
    .trigger     (OST1_trigger),
    .interrupt   (OST1_int)
);

// OS timer 2
reg OST2_trigger = 1'b0;
reg OST2_set = 1'b0;
reg [31:0] OST2_value = 32'd0;

OStimer OST2 (
    .clk         (clk),
    .reset       (reset),
    .timer_value (OST2_value),
    .set_value   (OST2_set),
    .trigger     (OST2_trigger),
    .interrupt   (OST2_int)
);

// OS timer 3
reg OST3_trigger = 1'b0;
reg OST3_set = 1'b0;
reg [31:0] OST3_value = 32'd0;

OStimer OST3 (
    .clk         (clk),
    .reset       (reset),
    .timer_value (OST3_value),
    .set_value   (OST3_set),
    .trigger     (OST3_trigger),
    .interrupt   (OST3_int)
);

// SPI0 (Flash 1) 25 MHz -- via SimpleSPI2 with cmd_skip_fifos=1 so the
// single-byte CPU MMIO + DMA iop_* paths bypass both FIFOs and read/write
// through the persistent last_rx_byte register. Behaves bit-for-bit like
// the original SimpleSPI today; the FIFO path is reserved for a future
// multi-byte burst DMA mode.
reg SPI0_start = 1'b0;
reg [7:0] SPI0_in = 8'd0;
wire [7:0] SPI0_out;
wire SPI0_done;

SimpleSPI2 #(
    .CLKS_PER_HALF_BIT(2),
    .FIFO_DEPTH(32)
) SPI0 (
    .clk             (clk),
    .reset           (reset),
    .cmd_we          (1'b0),          // FIFOs unused on CPU port (skip mode)
    .cmd_data        (SPI0_in),
    .cmd_start_burst (SPI0_start),
    .cmd_burst_len   (16'd1),
    .cmd_dummy       (1'b0),
    .cmd_skip_fifos  (1'b1),
    .tx_full         (SPI0_tx_full_w),
    .rx_empty        (SPI0_rx_empty_w),
    .rx_data         (SPI0_rx_data_w),
    .cmd_re_rx       (1'b0),
    .last_rx_byte    (SPI0_out),
    .busy            (SPI0_busy_w),
    .done            (SPI0_done),
    // DMA burst master
    .dma_select      (dma_sel_spi0),
    .dma_we          (dma_burst_we),
    .dma_data        (dma_burst_data),
    .dma_start_burst (dma_burst_start),
    .dma_burst_len   (dma_burst_len),
    .dma_dummy       (dma_burst_dummy),
    .dma_re_rx       (dma_burst_re_rx),
    .spi_clk         (SPI0_clk),
    .spi_miso        (SPI0_miso),
    .spi_mosi        (SPI0_mosi)
);

// SPI1 (Flash 2) 25 MHz
reg SPI1_start = 1'b0;
reg [7:0] SPI1_in = 8'd0;
wire [7:0] SPI1_out;
wire SPI1_done;

// SPI1 (Flash 2 / BRFS) 25 MHz -- via QSPIflash. Iteration 3 of the QSPI
// migration (Docs/plans/dma-followups.md §A.4.3): the 4-bit bidirectional
// pin interface is now exposed at the MemoryUnit boundary. Tristate is
// handled at the FPGC top level (or by the testbench), so QSPIflash drives
// SPI1_io_out[3:0] gated by SPI1_io_oe[3:0]. In 1-bit mode the OE pattern
// stays at 4'b1101 (drive IO0/IO2/IO3, sample IO1) -- functionally identical
// to the old separate mosi/miso/wp_n/hold_n wiring.
QSPIflash #(
    .CLKS_PER_HALF_BIT(2),
    .FIFO_DEPTH(32)
) SPI1 (
    .clk             (clk),
    .reset           (reset),
    .cmd_we          (1'b0),          // FIFOs unused on CPU port (skip mode)
    .cmd_data        (SPI1_in),
    .cmd_start_burst (SPI1_start),
    .cmd_burst_len   (16'd1),
    .cmd_dummy       (1'b0),
    .cmd_skip_fifos  (1'b1),
    .cmd_qspi_read   (1'b0),          // QSPI Fast Read disabled (iter 2 plumbing)
    .cmd_qspi_addr   (24'd0),
    .tx_full         (SPI1_tx_full_w),
    .rx_empty        (SPI1_rx_empty_w),
    .rx_data         (SPI1_rx_data_w),
    .rx_count_out    (SPI1_rx_count_w),
    .cmd_re_rx       (1'b0),
    .last_rx_byte    (SPI1_out),
    .busy            (SPI1_busy_w),
    .done            (SPI1_done),
    // DMA burst master
    .dma_select      (dma_sel_spi1),
    .dma_we          (dma_burst_we),
    .dma_data        (dma_burst_data),
    .dma_start_burst (dma_burst_start),
    .dma_burst_len   (dma_burst_len),
    .dma_dummy       (dma_burst_dummy),
    .dma_qspi_read   (dma_sel_spi1 ? dma_burst_qspi_read : 1'b0),
    .dma_qspi_addr   (dma_burst_qspi_addr),
    .dma_re_rx       (dma_burst_re_rx),
    .spi_clk         (SPI1_clk),
    .spi_io_out      (SPI1_io_out),
    .spi_io_oe       (SPI1_io_oe),
    .spi_io_in       (SPI1_io_in)
);
// (Pin tristate / wp_n / hold_n tie-offs are handled at the FPGC top
// level by the parent module.)

// SPI2 (USB Host 1) 12.5 MHz
reg SPI2_start = 1'b0;
reg [7:0] SPI2_in = 8'd0;
wire [7:0] SPI2_out;
wire SPI2_done;

SimpleSPI #(
    .CLKS_PER_HALF_BIT(4)
) SPI2 (
    .clk        (clk),
    .reset      (reset),
    .data_in    (SPI2_in),
    .start      (SPI2_start),
    .done       (SPI2_done),
    .data_out   (SPI2_out),
    .spi_clk    (SPI2_clk),
    .spi_miso   (SPI2_miso),
    .spi_mosi   (SPI2_mosi)
);

// SPI3 (USB Host 2) and SPI4 (Ethernet) removed for camera build.
// Only SPI0 (boot flash), SPI1 (BRFS flash), SPI2 (USB keyboard), SPI5 (SD card) remain.

// SPI5 (SD Card) 25 MHz -- via SimpleSPI2 with cmd_skip_fifos=1 (see SPI0
// note above and dma-followups.md §3.1 / §B.5.3). SPI5 is wired into the DMA
// burst port so 512-byte SD block payloads can be moved by DMA.
reg SPI5_start = 1'b0;
reg [7:0] SPI5_in = 8'd0;
wire [7:0] SPI5_out;
wire SPI5_done;

SimpleSPI2 #(
    .CLKS_PER_HALF_BIT(2),
    .FIFO_DEPTH(32)
) SPI5 (
    .clk             (clk),
    .reset           (reset),
    .cmd_we          (1'b0),          // FIFOs unused on CPU port (skip mode)
    .cmd_data        (SPI5_in),
    .cmd_start_burst (SPI5_start),
    .cmd_burst_len   (16'd1),
    .cmd_dummy       (1'b0),
    .cmd_skip_fifos  (1'b1),
    .tx_full         (SPI5_tx_full_w),
    .rx_empty        (SPI5_rx_empty_w),
    .rx_data         (SPI5_rx_data_w),
    .cmd_re_rx       (1'b0),
    .last_rx_byte    (SPI5_out),
    .busy            (SPI5_busy_w),
    .done            (SPI5_done),
    // DMA burst master
    .dma_select      (dma_sel_spi5),
    .dma_we          (dma_burst_we),
    .dma_data        (dma_burst_data),
    .dma_start_burst (dma_burst_start),
    .dma_burst_len   (dma_burst_len),
    .dma_dummy       (dma_burst_dummy),
    .dma_re_rx       (dma_burst_re_rx),
    .spi_clk         (SPI5_clk),
    .spi_miso        (SPI5_miso),
    .spi_mosi        (SPI5_mosi)
);

// Micros counter
wire [31:0] micros;

MicrosCounter micros_counter (
    .clk    (clk),
    .reset  (reset),
    .micros (micros)
);

// ---- Address map ----
localparam
    ADDR_UART_TX         = 32'h1C000000, // UART tx
    ADDR_UART_RX         = 32'h1C000004, // UART rx
    ADDR_TIMER1_VALUE    = 32'h1C000008, // Timer 1 value
    ADDR_TIMER1_START    = 32'h1C00000C, // Timer 1 start
    ADDR_TIMER2_VALUE    = 32'h1C000010, // Timer 2 value
    ADDR_TIMER2_START    = 32'h1C000014, // Timer 2 start
    ADDR_TIMER3_VALUE    = 32'h1C000018, // Timer 3 value
    ADDR_TIMER3_START    = 32'h1C00001C, // Timer 3 start
    ADDR_SPI0_DATA       = 32'h1C000020, // SPI0 data (Flash1)
    ADDR_SPI0_CS         = 32'h1C000024, // SPI0 CS
    ADDR_SPI1_DATA       = 32'h1C000028, // SPI1 data (Flash2)
    ADDR_SPI1_CS         = 32'h1C00002C, // SPI1 CS
    ADDR_SPI2_DATA       = 32'h1C000030, // SPI2 data (USB H1)
    ADDR_SPI2_CS         = 32'h1C000034, // SPI2 CS
    ADDR_SPI2_NINT       = 32'h1C000038, // SPI2 NINT
    // SPI3 (0x3C-0x44) and SPI4 (0x48-0x50) removed — camera build
    ADDR_SPI5_DATA       = 32'h1C000054, // SPI5 data (SD)
    ADDR_SPI5_CS         = 32'h1C000058, // SPI5 CS
    ADDR_GPIO_MODE       = 32'h1C00005C, // GPIO mode
    ADDR_GPIO_STATE      = 32'h1C000060, // GPIO state
    ADDR_BOOT_MODE       = 32'h1C000064, // Boot mode
    ADDR_MICROS          = 32'h1C000068, // Micros
    ADDR_LED_USER        = 32'h1C00006C, // User LED control
    ADDR_DMA_SRC         = 32'h1C000070, // DMA source address (byte)
    ADDR_DMA_DST         = 32'h1C000074, // DMA destination address (byte)
    ADDR_DMA_COUNT       = 32'h1C000078, // DMA byte count
    ADDR_DMA_CTRL        = 32'h1C00007C, // DMA control: [3:0] mode, [4] irq_en, [7:5] sub-target, [31] start
    ADDR_DMA_STATUS      = 32'h1C000080, // DMA status: [0] busy, [1] done, [2] error (sticky, read-clear)
    ADDR_DMA_QSPI_ADDR   = 32'h1C000084, // DMA QSPI Fast Read source address (24-bit byte offset into flash)
    ADDR_CAM_CTRL        = 32'h1C000088, // Camera control: [0] enable
    ADDR_CAM_STATUS      = 32'h1C00008C, // Camera status: [0] frame_done (read-clear), [1] current_buf
    ADDR_I2C_DBG         = 32'h1C000090, // I2C debug: {start_pending, i2c_start, i2c_dbg_state[4:0]}
    ADDR_CAM_BUF0        = 32'h1C000094, // Camera debug 0: [16:0] frame_pixels, [25:17] line_count
    ADDR_CAM_BUF1        = 32'h1C000098, // Camera debug 1: [11:0] cache_lines, [19:12] partial_drops
    ADDR_CAM_DBG         = 32'h1C00009C, // Camera debug 2: [2:0] state, [3] arb_busy
    ADDR_I2C_CMD         = 32'h1C0000A0, // I2C command: write {[23:17] dev_addr, [16] rw, [15:8] reg, [7:0] data}
    ADDR_I2C_DATA        = 32'h1C0000A4, // I2C read data: {24'd0, rd_data[7:0]}
    ADDR_GPU_STATUS      = 32'h1C0000A8, // GPU status: [0] in_vblank, [12:1] v_count
    ADDR_OOB             = 32'h1C0000AC; // All addresses >= this are out of bounds

// ---- State encoding ----
localparam
    STATE_IDLE                   = 5'd0,
    STATE_RETURN_ZERO            = 5'd1,
    STATE_WAIT_UART_TX           = 5'd2,
    STATE_WAIT_UART_RX           = 5'd3,
    STATE_WAIT_SPI0_DATA         = 5'd4,
    STATE_WAIT_SPI0_CS           = 5'd5,
    STATE_WAIT_SPI1_DATA         = 5'd6,
    STATE_WAIT_SPI1_CS           = 5'd7,
    STATE_WAIT_SPI2_DATA         = 5'd8,
    STATE_WAIT_SPI2_CS           = 5'd9,
    STATE_WAIT_SPI2_NINT         = 5'd10,
    // States 11-16 freed (SPI3/SPI4 removed)
    STATE_WAIT_SPI5_DATA         = 5'd17,
    STATE_WAIT_SPI5_CS           = 5'd18,
    STATE_WAIT_BOOT_MODE         = 5'd19,
    STATE_WAIT_MICROS            = 5'd20,
    STATE_RETURN_DMA_REG         = 5'd21, // generic 1-cycle latency read of a DMA register
    STATE_IOP_SPI_WAIT           = 5'd22, // step 9: DMA peer-port SPI transaction in flight
    STATE_RETURN_Q               = 5'd23; // Return pre-loaded q value


reg [4:0] state = 5'd0;
reg wait_done = 1'b0;

// ---- DMA register-bus routing ----
// Step 8: the 5 DMA registers now live inside DMAengine; MemoryUnit just
// translates CPU MMIO accesses to ADDR_DMA_* into reg_addr / reg_we /
// reg_data pulses on the engine bus, and forwards the engine's combinatorial
// reg_q back to the CPU one cycle later via STATE_RETURN_DMA_REG.
// (The local backing-store regs from step 7 are gone.)

always @(posedge clk) begin
    if (reset)
    begin
        state <= STATE_IDLE;
        q <= 32'd0;
        done <= 1'b0;
        wait_done <= 1'b0;

        uart_tx_start <= 1'b0;
        uart_tx_data <= 8'd0;

        flash_spi_activity <= 1'b0;
        usb_spi_activity <= 1'b0;
        user_led_state <= 1'b0;

        OST1_trigger <= 1'b0;
        OST1_set <= 1'b0;
        OST1_value <= 32'd0;

        OST2_trigger <= 1'b0;
        OST2_set <= 1'b0;
        OST2_value <= 32'd0;

        OST3_trigger <= 1'b0;
        OST3_set <= 1'b0;
        OST3_value <= 32'd0;

        SPI0_start <= 1'b0;
        SPI0_in <= 8'd0;
        SPI0_cs <= 1'b1;

        SPI1_start <= 1'b0;
        SPI1_in <= 8'd0;
        SPI1_cs <= 1'b1;

        SPI2_start <= 1'b0;
        SPI2_in <= 8'd0;
        SPI2_cs <= 1'b1;

        SPI5_start <= 1'b0;
        SPI5_in <= 8'd0;
        SPI5_cs <= 1'b1;

        dma_reg_addr <= 3'd0;
        dma_reg_we   <= 1'b0;
        dma_reg_data <= 32'd0;

        iop_done_r <= 1'b0;
        iop_q_r    <= 32'd0;
        iop_active <= 1'b0;
        iop_spi_id <= 3'd0;

        cpu_req_pending <= 1'b0;

        cam_frame_done_latch <= 1'b0;
        i2c_start_pending    <= 1'b0;
        i2c_start            <= 1'b0;
    end else
    begin
        // Default assignments
        done <= 1'b0;
        q <= 32'd0;
        wait_done <= 1'b0;

        // i2c_start: keep asserted until master acknowledges by setting busy.
        // This prevents losing the start pulse if it arrives while the master
        // is still in S_DONE transitioning back to S_IDLE.
        if (i2c_busy)
        begin
            i2c_start         <= 1'b0;
            i2c_start_pending <= 1'b0;
        end

        // Latch cam_frame_done pulses from CameraCapture
        if (cam_frame_done)
            cam_frame_done_latch <= 1'b1;

        // Latch CPU start pulses that arrive while MemoryUnit is busy.
        // CPU's mu_addr/mu_data/mu_we hold across the request (see MemoryStage),
        // so we only need a pending flag.
        if (start && state != STATE_IDLE)
            cpu_req_pending <= 1'b1;        uart_tx_start <= 1'b0;

        flash_spi_activity <= 1'b0;
        usb_spi_activity <= 1'b0;

        OST1_set <= 1'b0;
        OST1_trigger <= 1'b0;
        OST2_set <= 1'b0;
        OST2_trigger <= 1'b0;
        OST3_set <= 1'b0;
        OST3_trigger <= 1'b0;

        // DMA register-bus pulses default low so writes/reads only fire one cycle
        dma_reg_we <= 1'b0;

        // DMA iop_* peer-port pulses default low; iop_active clears when the
        // engine drops iop_start (handshake completion).
        iop_done_r <= 1'b0;
        if (iop_active && !iop_start)
            iop_active <= 1'b0;

        SPI0_start <= 1'b0;
        SPI1_start <= 1'b0;
        SPI2_start <= 1'b0;
        SPI5_start <= 1'b0;

        case (state)
            STATE_IDLE:
            begin
                if (start || cpu_req_pending)
                begin
                    cpu_req_pending <= 1'b0;
                    // ---- UART ----
                    if (addr == ADDR_UART_TX)
                    begin
                        uart_tx_data <= data[7:0];
                        uart_tx_start <= 1'b1;
                        state <= STATE_WAIT_UART_TX;
                    end
                    else if (addr == ADDR_UART_RX)
                    begin
                        state <= STATE_WAIT_UART_RX;
                    end

                    // ---- Timers ----
                    else if (addr == ADDR_TIMER1_VALUE)
                    begin
                        OST1_value <= data;
                        OST1_set <= 1'b1;
                        state <= STATE_RETURN_ZERO;
                    end
                    else if (addr == ADDR_TIMER1_START)
                    begin
                        OST1_trigger <= 1'b1;
                        state <= STATE_RETURN_ZERO;
                    end
                    else if (addr == ADDR_TIMER2_VALUE)
                    begin
                        OST2_value <= data;
                        OST2_set <= 1'b1;
                        state <= STATE_RETURN_ZERO;
                    end
                    else if (addr == ADDR_TIMER2_START)
                    begin
                        OST2_trigger <= 1'b1;
                        state <= STATE_RETURN_ZERO;
                    end
                    else if (addr == ADDR_TIMER3_VALUE)
                    begin
                        OST3_value <= data;
                        OST3_set <= 1'b1;
                        state <= STATE_RETURN_ZERO;
                    end
                    else if (addr == ADDR_TIMER3_START)
                    begin
                        OST3_trigger <= 1'b1;
                        state <= STATE_RETURN_ZERO;
                    end

                    // ---- SPI0 (Flash 1) ----
                    else if (addr == ADDR_SPI0_DATA)
                    begin
                        if (we)
                        begin
                            SPI0_in <= data[7:0];
                            SPI0_start <= 1'b1;
                            flash_spi_activity <= 1'b1;
                            wait_done <= 1'b1;
                        end
                        state <= STATE_WAIT_SPI0_DATA;
                    end
                    else if (addr == ADDR_SPI0_CS)
                    begin
                        if (we)
                        begin
                            SPI0_cs <= data[0];
                        end
                        state <= STATE_WAIT_SPI0_CS;
                    end

                    // ---- SPI1 (Flash 2) ----
                    else if (addr == ADDR_SPI1_DATA)
                    begin
                        if (we)
                        begin
                            SPI1_in <= data[7:0];
                            SPI1_start <= 1'b1;
                            flash_spi_activity <= 1'b1;
                            wait_done <= 1'b1;
                        end
                        state <= STATE_WAIT_SPI1_DATA;
                    end
                    else if (addr == ADDR_SPI1_CS)
                    begin
                        if (we)
                        begin
                            SPI1_cs <= data[0];
                        end
                        state <= STATE_WAIT_SPI1_CS;
                    end

                    // ---- SPI2 (USB Host 1) ----
                    else if (addr == ADDR_SPI2_DATA)
                    begin
                        if (we)
                        begin
                            SPI2_in <= data[7:0];
                            SPI2_start <= 1'b1;
                            usb_spi_activity <= 1'b1;
                            wait_done <= 1'b1;
                        end
                        state <= STATE_WAIT_SPI2_DATA;
                    end
                    else if (addr == ADDR_SPI2_CS)
                    begin
                        if (we)
                        begin
                            SPI2_cs <= data[0];
                        end
                        state <= STATE_WAIT_SPI2_CS;
                    end
                    else if (addr == ADDR_SPI2_NINT)
                    begin
                        state <= STATE_WAIT_SPI2_NINT;
                    end

                    // ---- SPI5 (SD Card) ----
                    else if (addr == ADDR_SPI5_DATA)
                    begin
                        if (we)
                        begin
                            SPI5_in <= data[7:0];
                            SPI5_start <= 1'b1;
                            flash_spi_activity <= 1'b1;
                            wait_done <= 1'b1;
                        end
                        state <= STATE_WAIT_SPI5_DATA;
                    end
                    else if (addr == ADDR_SPI5_CS)
                    begin
                        if (we)
                        begin
                            SPI5_cs <= data[0];
                        end
                        state <= STATE_WAIT_SPI5_CS;
                    end

                    // ---- GPIO ----
                    else if (addr == ADDR_GPIO_MODE)
                    begin
                        // TODO: Implement
                        state <= STATE_RETURN_ZERO;
                    end
                    else if (addr == ADDR_GPIO_STATE)
                    begin
                        // TODO: Implement
                        state <= STATE_RETURN_ZERO;
                    end

                    // ---- Misc ----
                    else if (addr == ADDR_BOOT_MODE)
                    begin
                        state <= STATE_WAIT_BOOT_MODE;
                    end
                    else if (addr == ADDR_MICROS)
                    begin
                        state <= STATE_WAIT_MICROS;
                    end
                    else if (addr == ADDR_LED_USER)
                    begin
                        if (we)
                        begin
                            user_led_state <= data[0];
                        end
                        state <= STATE_RETURN_ZERO;
                    end

                    // ---- DMA registers (routed to DMAengine reg_*) ----
                    else if (addr == ADDR_DMA_SRC)
                    begin
                        dma_reg_addr <= 3'd0;
                        dma_reg_we   <= we;
                        dma_reg_data <= data;
                        state        <= STATE_RETURN_DMA_REG;
                    end
                    else if (addr == ADDR_DMA_DST)
                    begin
                        dma_reg_addr <= 3'd1;
                        dma_reg_we   <= we;
                        dma_reg_data <= data;
                        state        <= STATE_RETURN_DMA_REG;
                    end
                    else if (addr == ADDR_DMA_COUNT)
                    begin
                        dma_reg_addr <= 3'd2;
                        dma_reg_we   <= we;
                        dma_reg_data <= data;
                        state        <= STATE_RETURN_DMA_REG;
                    end
                    else if (addr == ADDR_DMA_CTRL)
                    begin
                        dma_reg_addr <= 3'd3;
                        dma_reg_we   <= we;
                        dma_reg_data <= data;
                        state        <= STATE_RETURN_DMA_REG;
                    end
                    else if (addr == ADDR_DMA_STATUS)
                    begin
                        dma_reg_addr <= 3'd4;
                        dma_reg_we   <= 1'b0; // STATUS is read-only; reading also clears stickies
                        dma_reg_data <= 32'd0;
                        state        <= STATE_RETURN_DMA_REG;
                    end
                    else if (addr == ADDR_DMA_QSPI_ADDR)
                    begin
                        dma_reg_addr <= 3'd5;
                        dma_reg_we   <= we;
                        dma_reg_data <= data;
                        state        <= STATE_RETURN_DMA_REG;
                    end
                    // ---- Camera MMIO ----
                    else if (addr == ADDR_CAM_CTRL)
                    begin
                        if (we) cam_ctrl_enable <= data[0];
                        q_hold <= {31'd0, cam_ctrl_enable};
                        state  <= STATE_RETURN_Q;
                    end
                    else if (addr == ADDR_CAM_STATUS)
                    begin
                        // [0] frame_done_latch (read-clear)
                        // [1] current_buf
                        // [2] always 1 (was cam_configure_done, now software does config)
                        // [3] raw VSYNC pin
                        // [4] raw HREF pin
                        q_hold <= {27'd0, cam_href_raw, cam_vsync_raw, 1'b1,
                                   cam_current_buf, cam_frame_done_latch};
                        // Read-clear the frame_done latch
                        cam_frame_done_latch <= 1'b0;
                        state <= STATE_RETURN_Q;
                    end
                    else if (addr == ADDR_I2C_DBG)
                    begin
                        // [4:0] I2C master FSM state
                        // [5]   i2c_start (held)
                        // [6]   i2c_start_pending
                        // [7]   i2c_busy
                        q_hold <= {24'd0, i2c_busy, i2c_start_pending, i2c_start, i2c_dbg_state};
                        state  <= STATE_RETURN_Q;
                    end
                    else if (addr == ADDR_CAM_BUF0)
                    begin
                        // Camera debug 0 (read-only): [16:0] frame_pixels, [25:17] line_count
                        q_hold <= {6'd0, cam_dbg_line_count, cam_dbg_frame_pixels};
                        state  <= STATE_RETURN_Q;
                    end
                    else if (addr == ADDR_CAM_BUF1)
                    begin
                        // Camera debug 1 (read-only): [11:0] cache_lines, [19:12] partial_drops
                        q_hold <= {12'd0, cam_dbg_partial_drops, cam_dbg_cache_lines};
                        state  <= STATE_RETURN_Q;
                    end
                    else if (addr == ADDR_CAM_DBG)
                    begin
                        // Camera debug 2: [2:0] state, [3] SDRAMarbiter busy
                        q_hold <= {28'd0, sdram_arb_busy, cam_dbg_state};
                        state  <= STATE_RETURN_Q;
                    end
                    else if (addr == ADDR_I2C_CMD)
                    begin
                        if (we) begin
                            i2c_dev_addr <= data[23:17];
                            i2c_rw       <= data[16];
                            i2c_reg_addr <= data[15:8];
                            i2c_wr_data  <= data[7:0];
                            i2c_start    <= 1'b1;
                            i2c_start_pending <= 1'b1;
                        end
                        // Report busy if master is busy OR if a start is pending
                        q_hold <= {30'd0, i2c_ack_err, (i2c_busy | i2c_start_pending)};
                        state  <= STATE_RETURN_Q;
                    end
                    else if (addr == ADDR_I2C_DATA)
                    begin
                        q_hold <= {24'd0, i2c_rd_data};
                        state  <= STATE_RETURN_Q;
                    end
                    else if (addr == ADDR_GPU_STATUS)
                    begin
                        // [0] in_vblank, [12:1] v_count
                        q_hold <= {19'd0, gpu_v_count, gpu_vblank};
                        state  <= STATE_RETURN_Q;
                    end
                    else
                    begin
                        // Out of range or unhandled address
                        state <= STATE_RETURN_ZERO;
                    end
                end
                else if (iop_start && !iop_active && !cpu_req_pending)
                begin
                    // DMA peer-port transaction (SPI0/SPI1 byte data).
                    iop_active <= 1'b1;
                    if (iop_addr == ADDR_SPI0_DATA)
                    begin
                        SPI0_in            <= iop_data[7:0];
                        SPI0_start         <= 1'b1;
                        flash_spi_activity <= 1'b1;
                        iop_spi_id         <= 3'd0;
                        state              <= STATE_IOP_SPI_WAIT;
                    end
                    else if (iop_addr == ADDR_SPI1_DATA)
                    begin
                        SPI1_in            <= iop_data[7:0];
                        SPI1_start         <= 1'b1;
                        flash_spi_activity <= 1'b1;
                        iop_spi_id         <= 3'd1;
                        state              <= STATE_IOP_SPI_WAIT;
                    end
                    else
                    begin
                        // Unsupported iop address: complete with zero data so
                        // the engine doesn't lock up.
                        iop_done_r <= 1'b1;
                        iop_q_r    <= 32'd0;
                    end
                end
            end

            STATE_WAIT_UART_TX:
            begin
                if (uart_tx_done)
                begin
                    done <= 1'b1;
                    state <= STATE_IDLE;
                end
            end

            STATE_WAIT_UART_RX:
            begin
                done <= 1'b1;
                q <= {24'd0, uart_rx_q};
                state <= STATE_IDLE;
            end

            STATE_WAIT_SPI0_DATA:
            begin
                wait_done <= 1'b1;
                if (SPI0_done || !wait_done)
                begin
                    done <= 1'b1;
                    q <= {24'd0, SPI0_out};
                    state <= STATE_IDLE;
                end
            end

            STATE_WAIT_SPI0_CS:
            begin
                done <= 1'b1;
                q <= {31'd0, SPI0_cs};
                state <= STATE_IDLE;
            end

            STATE_WAIT_SPI1_DATA:
            begin
                wait_done <= 1'b1;
                if (SPI1_done || !wait_done)
                begin
                    done <= 1'b1;
                    q <= {24'd0, SPI1_out};
                    state <= STATE_IDLE;
                end
            end

            STATE_WAIT_SPI1_CS:
            begin
                done <= 1'b1;
                q <= {31'd0, SPI1_cs};
                state <= STATE_IDLE;
            end

            STATE_WAIT_SPI2_DATA:
            begin
                wait_done <= 1'b1;
                if (SPI2_done || !wait_done)
                begin
                    done <= 1'b1;
                    q <= {24'd0, SPI2_out};
                    state <= STATE_IDLE;
                end
            end

            STATE_WAIT_SPI2_CS:
            begin
                done <= 1'b1;
                q <= {31'd0, SPI2_cs};
                state <= STATE_IDLE;
            end

            STATE_WAIT_SPI2_NINT:
            begin
                done <= 1'b1;
                q <= {31'd0, SPI2_nint};
                state <= STATE_IDLE;
            end

            STATE_WAIT_SPI5_DATA:
            begin
                wait_done <= 1'b1;
                if (SPI5_done || !wait_done)
                begin
                    done <= 1'b1;
                    q <= {24'd0, SPI5_out};
                    state <= STATE_IDLE;
                end
            end

            STATE_WAIT_SPI5_CS:
            begin
                done <= 1'b1;
                q <= {31'd0, SPI5_cs};
                state <= STATE_IDLE;
            end

            STATE_WAIT_BOOT_MODE:
            begin
                done <= 1'b1;
                q <= {31'd0, boot_mode};
                state <= STATE_IDLE;
            end

            STATE_WAIT_MICROS:
            begin
                done <= 1'b1;
                q <= micros;
                state <= STATE_IDLE;
            end

            STATE_RETURN_ZERO:
            begin
                done <= 1'b1;
                q <= 32'd0;
                state <= STATE_IDLE;
            end

            STATE_RETURN_DMA_REG:
            begin
                done <= 1'b1;
                q <= dma_reg_q;
                state <= STATE_IDLE;
            end

            STATE_RETURN_Q:
            begin
                // q was pre-loaded by the handler; re-assert it to
                // override the default q<=0 that runs each cycle.
                q     <= q_hold;
                done  <= 1'b1;
                state <= STATE_IDLE;
            end

            STATE_IOP_SPI_WAIT:
            begin
                // Wait for the chosen SPI controller to finish the byte and
                // hand the RX byte back to the engine via iop_q.
                if (iop_spi_id == 3'd0)
                begin
                    flash_spi_activity <= 1'b1;
                    if (SPI0_done)
                    begin
                        iop_done_r <= 1'b1;
                        iop_q_r    <= {24'd0, SPI0_out};
                        state      <= STATE_IDLE;
                    end
                end
                else if (iop_spi_id == 3'd1)
                begin
                    flash_spi_activity <= 1'b1;
                    if (SPI1_done)
                    begin
                        iop_done_r <= 1'b1;
                        iop_q_r    <= {24'd0, SPI1_out};
                        state      <= STATE_IDLE;
                    end
                end
                else
                begin
                    // Should never happen
                    iop_done_r <= 1'b1;
                    iop_q_r    <= 32'd0;
                    state      <= STATE_IDLE;
                end
            end

            default:
            begin
                done <= 1'b0;
                q <= 32'd0;
                state <= STATE_IDLE;
            end
        endcase
    end
end

// ---- DMA SPI burst output mux ----
// Pull together the per-SPI burst-side wires into the engine-facing port.
assign dma_burst_tx_full =
    (dma_burst_spi_id == 3'd0) ? SPI0_tx_full_w :
    (dma_burst_spi_id == 3'd1) ? SPI1_tx_full_w :
    (dma_burst_spi_id == 3'd5) ? SPI5_tx_full_w : 1'b1;
assign dma_burst_rx_empty =
    (dma_burst_spi_id == 3'd0) ? SPI0_rx_empty_w :
    (dma_burst_spi_id == 3'd1) ? SPI1_rx_empty_w :
    (dma_burst_spi_id == 3'd5) ? SPI5_rx_empty_w : 1'b1;
assign dma_burst_rx_data =
    (dma_burst_spi_id == 3'd0) ? SPI0_rx_data_w :
    (dma_burst_spi_id == 3'd1) ? SPI1_rx_data_w :
    (dma_burst_spi_id == 3'd5) ? SPI5_rx_data_w : 8'd0;
// Only QSPIflash (SPI1) exposes rx_count; SPI0/SPI5 (SimpleSPI2) report 0.
// The DMA engine only consults this in MODE_SPI2MEM_QSPI which is SPI1-only.
assign dma_burst_rx_count =
    (dma_burst_spi_id == 3'd1) ? SPI1_rx_count_w : 8'd0;
assign dma_burst_busy =
    (dma_burst_spi_id == 3'd0) ? SPI0_busy_w :
    (dma_burst_spi_id == 3'd1) ? SPI1_busy_w :
    (dma_burst_spi_id == 3'd5) ? SPI5_busy_w : 1'b0;
assign dma_burst_done =
    (dma_burst_spi_id == 3'd0) ? SPI0_done :
    (dma_burst_spi_id == 3'd1) ? SPI1_done :
    (dma_burst_spi_id == 3'd5) ? SPI5_done : 1'b0;

endmodule
