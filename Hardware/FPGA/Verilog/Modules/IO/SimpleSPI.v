/*
 * SimpleSPI
 * Simple SPI master module (Mode 0: CPOL=0, CPHA=0)
 * Transmits and receives 8 bits simultaneously (MSB first)
 * When transfer is complete, done will be driven high for one clock cycle
 *
 * Set Parameter CLKS_PER_HALF_BIT as follows:
 * SPI_CLK_FREQ = clk_freq / (2 * CLKS_PER_HALF_BIT)
 * Example: 50 MHz Clock, CLKS_PER_HALF_BIT=1 -> SPI_CLK = 25 MHz
 * Example: 50 MHz Clock, CLKS_PER_HALF_BIT=2 -> SPI_CLK = 12.5 MHz
 */
module SimpleSPI #(
    parameter CLKS_PER_HALF_BIT = 1
) (
    //========================
    // System interface
    //========================
    input  wire         clk,
    input  wire         reset,

    //========================
    // Control interface
    //========================
    input  wire         start,          // Start transfer (active high pulse)
    input  wire [7:0]   data_in,        // Byte to transmit
    output reg          done = 1'b0,    // Transfer complete (high for one cycle)
    output reg  [7:0]   data_out = 8'd0,// Received byte

    //========================
    // SPI interface
    //========================
    output reg          spi_clk = 1'b0, // SPI clock (idle low for Mode 0)
    input  wire         spi_miso,       // Master In Slave Out
    output reg          spi_mosi = 1'b0 // Master Out Slave In
);

//========================
// State Machine
//========================
localparam
    STATE_IDLE      = 2'd0,
    STATE_TRANSFER  = 2'd1,
    STATE_DONE      = 2'd2;

reg [1:0] state = STATE_IDLE;

//========================
// Internal Registers
//========================
// Counter for clock edges: 16 edges per byte (8 rising + 8 falling)
reg [4:0] edge_count = 5'd0;

// Clock divider counter
localparam CLK_COUNT_WIDTH = (CLKS_PER_HALF_BIT > 1) ? $clog2(CLKS_PER_HALF_BIT*2) : 1;
reg [CLK_COUNT_WIDTH-1:0] clk_count = 0;

// Internal SPI clock (delayed by one cycle for output)
reg spi_clk_internal = 1'b0;

// Shift registers
reg [7:0] tx_shift = 8'd0;
reg [2:0] tx_bit_idx = 3'd7;    // TX bit index (counts down from 7)
reg [2:0] rx_bit_idx = 3'd7;    // RX bit index (counts down from 7)

// Edge detection
reg leading_edge = 1'b0;        // Rising edge of SPI clock
reg trailing_edge = 1'b0;       // Falling edge of SPI clock

always @(posedge clk)
begin
    if (reset)
    begin
        state <= STATE_IDLE;
        edge_count <= 5'd0;
        clk_count <= 0;
        spi_clk_internal <= 1'b0;
        spi_clk <= 1'b0;
        spi_mosi <= 1'b0;
        tx_shift <= 8'd0;
        tx_bit_idx <= 3'd7;
        rx_bit_idx <= 3'd7;
        data_out <= 8'd0;
        done <= 1'b0;
        leading_edge <= 1'b0;
        trailing_edge <= 1'b0;
    end
    else
    begin
        // Default assignments
        done <= 1'b0;
        leading_edge <= 1'b0;
        trailing_edge <= 1'b0;

        case (state)
            STATE_IDLE:
            begin
                spi_clk_internal <= 1'b0;
                spi_clk <= 1'b0;
                clk_count <= 0;
                tx_bit_idx <= 3'd7;
                rx_bit_idx <= 3'd7;
                edge_count <= 5'd0;

                if (start)
                begin
                    tx_shift <= data_in;
                    spi_mosi <= data_in[7];     // Set up MSB first (CPHA=0)
                    tx_bit_idx <= 3'd6;          // Next bit to send
                    edge_count <= 5'd16;         // 16 clock edges total
                    state <= STATE_TRANSFER;
                end
            end

            STATE_TRANSFER:
            begin
                // Generate SPI clock edges with timing
                if (clk_count == CLKS_PER_HALF_BIT * 2 - 1)
                begin
                    // Trailing edge (falling for Mode 0)
                    clk_count <= 0;
                    spi_clk_internal <= ~spi_clk_internal;
                    trailing_edge <= 1'b1;
                    edge_count <= edge_count - 1'b1;
                end
                else if (clk_count == CLKS_PER_HALF_BIT - 1)
                begin
                    // Leading edge (rising for Mode 0)
                    clk_count <= clk_count + 1'b1;
                    spi_clk_internal <= ~spi_clk_internal;
                    leading_edge <= 1'b1;
                    edge_count <= edge_count - 1'b1;
                end
                else
                begin
                    clk_count <= clk_count + 1'b1;
                end

                // Check if transfer complete
                if (edge_count == 0)
                begin
                    state <= STATE_DONE;
                end
            end

            STATE_DONE:
            begin
                spi_clk_internal <= 1'b0;
                spi_clk <= 1'b0;
                done <= 1'b1;
                
                // Wait for start to go low before returning to idle
                if (!start)
                begin
                    state <= STATE_IDLE;
                end
            end

            default:
            begin
                state <= STATE_IDLE;
            end
        endcase

        // TX: shift out on trailing edge (CPHA=0: data changes on falling edge)
        if (trailing_edge)
        begin
            spi_mosi <= tx_shift[tx_bit_idx];
            tx_bit_idx <= tx_bit_idx - 1'b1;
        end

        // RX: sample on leading edge (CPHA=0: data sampled on rising edge)
        if (leading_edge)
        begin
            data_out[rx_bit_idx] <= spi_miso;
            rx_bit_idx <= rx_bit_idx - 1'b1;
        end

        // Delay SPI clock output by one cycle for alignment
        spi_clk <= spi_clk_internal;
    end
end

endmodule
