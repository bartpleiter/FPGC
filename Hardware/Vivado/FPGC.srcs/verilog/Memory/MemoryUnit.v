/*
 * MemoryUnit (MU)
 * Single interface between CPU and slow Memory or IO
 * Optimized for simplicity, not speed, as high speed memory should be directly connected to CPU
 */
module MemoryUnit(
    //========================
    // System interface
    //========================
    input  wire         clk, // Assumed to be 50MHz
    input  wire         reset,

    //========================
    // CPU interface (50 MHz domain)
    //========================
    input  wire         start,
    input  wire [31:0]  addr, // Address in CPU words
    input  wire [31:0]  data,
    input  wire         we,
    output reg  [31:0]  q = 32'd0,
    output reg          done = 1'b0,

    //========================
    // I/O signals
    //========================
    // UART
    input wire          uart_rx,
    output wire         uart_tx,
    output wire         uart_irq,

    // Timer interrupts
    output wire         OST1_int,
    output wire         OST2_int,
    output wire         OST3_int,

    // Boot mode signal
    input wire          boot_mode,

    // SPI0 (Flash 1)
    output              SPI0_clk,
    output reg          SPI0_cs = 1'b1,
    output              SPI0_mosi,
    input               SPI0_miso,

    // SPI1 (Flash 2)
    output              SPI1_clk,
    output reg          SPI1_cs = 1'b1,
    output              SPI1_mosi,
    input               SPI1_miso,

    // SPI2 (USB Host 1)
    output              SPI2_clk,
    output reg          SPI2_cs = 1'b1,
    output              SPI2_mosi,
    input               SPI2_miso,

    // SPI3 (USB Host 2)
    output              SPI3_clk,
    output reg          SPI3_cs = 1'b1,
    output              SPI3_mosi,
    input               SPI3_miso,

    // SPI4 (Ethernet)
    output              SPI4_clk,
    output reg          SPI4_cs = 1'b1,
    output              SPI4_mosi,
    input               SPI4_miso,

    // SPI5 (SD Card)
    output              SPI5_clk,
    output reg          SPI5_cs = 1'b1,
    output              SPI5_mosi,
    input               SPI5_miso

    // TODO: GPIO

);

//========================
// IO Devices
//========================
// UART TX
reg uart_tx_start = 1'b0;
wire uart_tx_done;
reg [7:0] uart_tx_data = 8'd0;

UARTtx uart_tx_controller(
.i_Clock    (clk),
.reset      (reset),
.i_Tx_DV    (uart_tx_start),
.i_Tx_Byte  (uart_tx_data),
.o_Tx_Active(),
.o_Tx_Serial(uart_tx),
.o_Tx_Done  (uart_tx_done)
);

// UART RX
wire [7:0] uart_rx_q;

UARTrx uart_rx_controller(
.i_Clock    (clk),
.reset      (reset),
.i_Rx_Serial(uart_rx),
.o_Rx_DV    (uart_irq),
.o_Rx_Byte  (uart_rx_q)
);

// OS timer 1
reg OST1_trigger = 1'b0;
reg OST1_set = 1'b0;
reg [31:0] OST1_value = 32'd0;

OStimer OST1(
.clk        (clk),
.reset      (reset),
.timerValue (OST1_value),
.setValue   (OST1_set),
.trigger    (OST1_trigger),
.interrupt  (OST1_int)
);

// OS timer 2
reg OST2_trigger = 1'b0;
reg OST2_set = 1'b0;
reg [31:0] OST2_value = 32'd0;

OStimer OST2(
.clk        (clk),
.reset      (reset),
.timerValue (OST2_value),
.setValue   (OST2_set),
.trigger    (OST2_trigger),
.interrupt  (OST2_int)
);

// OS timer 3
reg OST3_trigger = 1'b0;
reg OST3_set = 1'b0;
reg [31:0] OST3_value = 32'd0;

OStimer OST3(
.clk        (clk),
.reset      (reset),
.timerValue (OST3_value),
.setValue   (OST3_set),
.trigger    (OST3_trigger),
.interrupt  (OST3_int)
);

// SPI0 (Flash 1) 25 MHz
reg SPI0_start = 1'b0;
reg [7:0] SPI0_in = 8'd0;
wire [7:0] SPI0_out;
wire SPI0_done;

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
.miso       (SPI0_miso),
.mosi       (SPI0_mosi)
);

// SPI1 (Flash 2) 25 MHz
reg SPI1_start = 1'b0;
reg [7:0] SPI1_in = 8'd0;
wire [7:0] SPI1_out;
wire SPI1_done;

SimpleSPI #(
.CLKS_PER_HALF_BIT(1))
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

// SPI2 (USB Host 1) 12.5 MHz
reg SPI2_start = 1'b0;
reg [7:0] SPI2_in = 8'd0;
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

// SPI3 (USB Host 2) 12.5 MHz
reg SPI3_start = 1'b0;
reg [7:0] SPI3_in = 8'd0;
wire [7:0] SPI3_out;
wire SPI3_done;

SimpleSPI #(
.CLKS_PER_HALF_BIT(2))
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

// SPI4 (Ethernet) 12.5 MHz
reg SPI4_start = 1'b0;
reg [7:0] SPI4_in = 8'd0;
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

// SPI5 (SD Card) 25 MHz
reg SPI5_start = 1'b0;
reg [7:0] SPI5_in = 8'd0;
wire [7:0] SPI5_out;
wire SPI5_done;

SimpleSPI #(
.CLKS_PER_HALF_BIT(1))
SPI5(
.clk        (clk),
.reset      (reset),
.in_byte    (SPI5_in),
.start      (SPI5_start),
.done       (SPI5_done),
.out_byte   (SPI5_out),
.spi_clk    (SPI5_clk),
.miso       (SPI5_miso),
.mosi       (SPI5_mosi)
);

// Micros counter
wire [31:0] micros;

MicrosCounter microsCounter(
.clk        (clk),
.reset      (reset),
.micros     (micros)
);

//========================
// Memory Unit Logic
//========================
// Address mappings (from memory map)
localparam ADDR_UART_TX         = 32'h7000000; // UART tx
localparam ADDR_UART_RX         = 32'h7000001; // UART rx
localparam ADDR_TIMER1_VALUE    = 32'h7000002; // Timer 1 value
localparam ADDR_TIMER1_START    = 32'h7000003; // Timer 1 start
localparam ADDR_TIMER2_VALUE    = 32'h7000004; // Timer 2 value
localparam ADDR_TIMER2_START    = 32'h7000005; // Timer 2 start
localparam ADDR_TIMER3_VALUE    = 32'h7000006; // Timer 3 value
localparam ADDR_TIMER3_START    = 32'h7000007; // Timer 3 start
localparam ADDR_SPI0_DATA       = 32'h7000008; // SPI0 data (Flash1)
localparam ADDR_SPI0_CS         = 32'h7000009; // SPI0 CS
localparam ADDR_SPI1_DATA       = 32'h700000A; // SPI1 data (Flash2)
localparam ADDR_SPI1_CS         = 32'h700000B; // SPI1 CS
localparam ADDR_SPI2_DATA       = 32'h700000C; // SPI2 data (USB H1)
localparam ADDR_SPI2_CS         = 32'h700000D; // SPI2 CS
localparam ADDR_RESERVED_0E     = 32'h700000E; // Reserved
localparam ADDR_SPI3_DATA       = 32'h700000F; // SPI3 data (USB H2)
localparam ADDR_SPI3_CS         = 32'h7000010; // SPI3 CS
localparam ADDR_RESERVED_11     = 32'h7000011; // Reserved
localparam ADDR_SPI4_DATA       = 32'h7000012; // SPI4 data (Ethernet)
localparam ADDR_SPI4_CS         = 32'h7000013; // SPI4 CS
localparam ADDR_RESERVED_14     = 32'h7000014; // Reserved
localparam ADDR_SPI5_DATA       = 32'h7000015; // SPI5 data (SD)
localparam ADDR_SPI5_CS         = 32'h7000016; // SPI5 CS
localparam ADDR_GPIO_MODE       = 32'h7000017; // GPIO mode
localparam ADDR_GPIO_STATE      = 32'h7000018; // GPIO state
localparam ADDR_BOOT_MODE       = 32'h7000019; // Boot mode
localparam ADDR_FPGA_TEMP       = 32'h700001A; // FPGA temp
localparam ADDR_MICROS          = 32'h700001B; // Micros

localparam ADDR_OOB             = 32'h700001C; // All addresses >= this are out of bounds and return a constant

// State machine states
localparam STATE_IDLE                   = 8'd0;
localparam STATE_RETURN_ZERO            = 8'd1;
localparam STATE_WAIT_UART_TX           = 8'd2;
localparam STATE_WAIT_UART_RX           = 8'd3;
localparam STATE_WAIT_SPI0_DATA         = 8'd4;
localparam STATE_WAIT_SPI0_CS           = 8'd5;
localparam STATE_WAIT_SPI1_DATA         = 8'd6;
localparam STATE_WAIT_SPI1_CS           = 8'd7;
localparam STATE_WAIT_SPI2_DATA         = 8'd8;
localparam STATE_WAIT_SPI2_CS           = 8'd9;
localparam STATE_WAIT_SPI3_DATA         = 8'd10;
localparam STATE_WAIT_SPI3_CS           = 8'd11;
localparam STATE_WAIT_SPI4_DATA         = 8'd12;
localparam STATE_WAIT_SPI4_CS           = 8'd13;
localparam STATE_WAIT_SPI5_DATA         = 8'd14;
localparam STATE_WAIT_SPI5_CS           = 8'd15;
localparam STATE_WAIT_BOOT_MODE         = 8'd16;
localparam STATE_WAIT_FPGA_TEMP         = 8'd17;
localparam STATE_WAIT_MICROS            = 8'd18;


reg [7:0] state = 8'd0;

always @(posedge clk) begin
    if (reset)
    begin
        state <= STATE_IDLE;
        q = 32'd0;
        done <= 1'b0;

        uart_tx_start <= 1'b0;
        uart_tx_data <= 8'd0;

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

        SPI3_start <= 1'b0;
        SPI3_in <= 8'd0;
        SPI3_cs <= 1'b1;

        SPI4_start <= 1'b0;
        SPI4_in <= 8'd0;
        SPI4_cs <= 1'b1;

        SPI5_start <= 1'b0;
        SPI5_in <= 8'd0;
        SPI5_cs <= 1'b1;
    end else
    begin
        // Default assignments
        done <= 1'b0;
        q <= 32'd0;

        uart_tx_start <= 1'b0;

        OST1_set <= 1'b0;
        OST1_trigger <= 1'b0;
        OST2_set <= 1'b0;
        OST2_trigger <= 1'b0;
        OST3_set <= 1'b0;
        OST3_trigger <= 1'b0;

        SPI0_start <= 1'b0;
        SPI1_start <= 1'b0;
        SPI2_start <= 1'b0;
        SPI3_start <= 1'b0;
        SPI4_start <= 1'b0;
        SPI5_start <= 1'b0;

        case (state)
            STATE_IDLE:
            begin
                if (start)
                begin
                    // UART TX
                    if (addr == ADDR_UART_TX)
                    begin
                        uart_tx_data <= data[7:0];
                        uart_tx_start <= 1'b1;
                        state <= STATE_WAIT_UART_TX;
                    end

                    // UART RX
                    if (addr == ADDR_UART_RX)
                    begin
                        state <= STATE_WAIT_UART_RX;
                    end

                    // Timer1
                    if (addr == ADDR_TIMER1_VALUE)
                    begin
                        OST1_value <= data;
                        OST1_set <= 1'b1;
                        state <= STATE_RETURN_ZERO;
                    end

                    if (addr == ADDR_TIMER1_START)
                    begin
                        OST1_trigger <= 1'b1;
                        state <= STATE_RETURN_ZERO;
                    end

                    // Timer2
                    if (addr == ADDR_TIMER2_VALUE)
                    begin
                        OST2_value <= data;
                        OST2_set <= 1'b1;
                        state <= STATE_RETURN_ZERO;
                    end

                    if (addr == ADDR_TIMER2_START)
                    begin
                        OST2_trigger <= 1'b1;
                        state <= STATE_RETURN_ZERO;
                    end

                    // Timer3
                    if (addr == ADDR_TIMER3_VALUE)
                    begin
                        OST3_value <= data;
                        OST3_set <= 1'b1;
                        state <= STATE_RETURN_ZERO;
                    end

                    if (addr == ADDR_TIMER3_START)
                    begin
                        OST3_trigger <= 1'b1;
                        state <= STATE_RETURN_ZERO;
                    end

                    // SPI0
                    if (addr == ADDR_SPI0_DATA)
                    begin
                        SPI0_in <= data[7:0];
                        SPI0_start <= 1'b1;
                        state <= STATE_WAIT_SPI0_DATA;
                    end

                    if (addr == ADDR_SPI0_CS)
                    begin
                        if (we)
                        begin
                            SPI0_cs <= data[0];
                        end
                        state <= STATE_WAIT_SPI0_CS;
                    end

                    // SPI1
                    if (addr == ADDR_SPI1_DATA)
                    begin
                        SPI1_in <= data[7:0];
                        SPI1_start <= 1'b1;
                        state <= STATE_WAIT_SPI1_DATA;
                    end

                    if (addr == ADDR_SPI1_CS)
                    begin
                        if (we)
                        begin
                            SPI1_cs <= data[0];
                        end
                        state <= STATE_WAIT_SPI1_CS;
                    end

                    // SPI2
                    if (addr == ADDR_SPI2_DATA)
                    begin
                        SPI2_in <= data[7:0];
                        SPI2_start <= 1'b1;
                        state <= STATE_WAIT_SPI2_DATA;
                    end

                    if (addr == ADDR_SPI2_CS)
                    begin
                        if (we)
                        begin
                            SPI2_cs <= data[0];
                        end
                        state <= STATE_WAIT_SPI2_CS;
                    end

                    // SPI3
                    if (addr == ADDR_SPI3_DATA)
                    begin
                        SPI3_in <= data[7:0];
                        SPI3_start <= 1'b1;
                        state <= STATE_WAIT_SPI3_DATA;
                    end

                    if (addr == ADDR_SPI3_CS)
                    begin
                        if (we)
                        begin
                            SPI3_cs <= data[0];
                        end
                        state <= STATE_WAIT_SPI3_CS;
                    end

                    // SPI4
                    if (addr == ADDR_SPI4_DATA)
                    begin
                        SPI4_in <= data[7:0];
                        SPI4_start <= 1'b1;
                        state <= STATE_WAIT_SPI4_DATA;
                    end

                    if (addr == ADDR_SPI4_CS)
                    begin
                        if (we)
                        begin
                            SPI4_cs <= data[0];
                        end
                        state <= STATE_WAIT_SPI4_CS;
                    end

                    // SPI5
                    if (addr == ADDR_SPI5_DATA)
                    begin
                        SPI5_in <= data[7:0];
                        SPI5_start <= 1'b1;
                        state <= STATE_WAIT_SPI5_DATA;
                    end

                    if (addr == ADDR_SPI5_CS)
                    begin
                        if (we)
                        begin
                            SPI5_cs <= data[0];
                        end
                        state <= STATE_WAIT_SPI5_CS;
                    end

                    // GPIO
                    if (addr == ADDR_GPIO_MODE)
                    begin
                        // TODO: Implement
                        state <= STATE_RETURN_ZERO;
                    end

                    if (addr == ADDR_GPIO_STATE)
                    begin
                        // TODO: Implement
                        state <= STATE_RETURN_ZERO;
                    end

                    // Boot mode
                    if (addr == ADDR_BOOT_MODE)
                    begin
                        state <= STATE_WAIT_BOOT_MODE;
                    end

                    if (addr >= ADDR_OOB)
                    begin
                        // Out of range
                        state <= STATE_RETURN_ZERO;
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
                if (SPI0_done)
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
                if (SPI1_done)
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
                if (SPI2_done)
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

            STATE_WAIT_SPI3_DATA:
            begin
                if (SPI3_done)
                begin
                    done <= 1'b1;
                    q <= {24'd0, SPI3_out};
                    state <= STATE_IDLE;
                end
            end

            STATE_WAIT_SPI3_CS:
            begin
                done <= 1'b1;
                q <= {31'd0, SPI3_cs};
                state <= STATE_IDLE;
            end

            STATE_WAIT_SPI4_DATA:
            begin
                if (SPI4_done)
                begin
                    done <= 1'b1;
                    q <= {24'd0, SPI4_out};
                    state <= STATE_IDLE;
                end
            end

            STATE_WAIT_SPI4_CS:
            begin
                done <= 1'b1;
                q <= {31'd0, SPI4_cs};
                state <= STATE_IDLE;
            end

            STATE_WAIT_SPI5_DATA:
            begin
                if (SPI5_done)
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

            STATE_WAIT_FPGA_TEMP:
            begin
                done <= 1'b1;
                q <= 32'd0; // TODO: Implement FPGA temperature reading
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

            default:
            begin
                done <= 1'b0;
                q <= 32'd0;
                state <= STATE_IDLE;
            end
        endcase
    end
end


endmodule
