/*
 * SPIMaster
 * Dedicated high-speed SPI master for ILI9341 display
 *
 * Optimized for continuous back-to-back byte streaming.
 * Mode 0 (CPOL=0, CPHA=0): clock idle low, data sampled on rising edge.
 * MSB first. 50 MHz SPI clock from 100 MHz system clock (÷2).
 *
 * The D/C pin is latched at the start of each byte to avoid glitches
 * mid-transfer. CS is directly controlled by the caller.
 */
module SPIMaster (
    input  wire        clk,        // 100 MHz system clock
    input  wire        reset,

    // Data interface
    input  wire [7:0]  tx_data,    // Byte to transmit
    input  wire        tx_valid,   // Data available (pulse to load)
    output wire        tx_ready,   // Ready to accept next byte

    // Control
    input  wire        dc_value,   // D/C pin value for this byte

    // SPI output
    output wire        spi_clk,    // 50 MHz SPI clock
    output wire        spi_mosi,   // Data out (directly from shift register MSB)
    output reg         spi_dc      // Data/Command pin
);

    // State machine
    localparam IDLE = 1'b0,
               SHIFT = 1'b1;

    reg        state = IDLE;
    reg [7:0]  shift_reg = 8'd0;
    reg [3:0]  bit_cnt = 4'd0;   // Counts 0..15 (16 half-clocks per byte)
    reg        spi_clk_reg = 1'b0;

    // SPI clock output: only toggle during transmission
    assign spi_clk = spi_clk_reg;

    // MOSI: MSB of shift register
    assign spi_mosi = shift_reg[7];

    // Ready when idle
    assign tx_ready = (state == IDLE);

    always @(posedge clk) begin
        if (reset) begin
            state <= IDLE;
            shift_reg <= 8'd0;
            bit_cnt <= 4'd0;
            spi_clk_reg <= 1'b0;
            spi_dc <= 1'b0;
        end else begin
            case (state)
                IDLE: begin
                    spi_clk_reg <= 1'b0;
                    if (tx_valid) begin
                        shift_reg <= tx_data;
                        spi_dc <= dc_value;
                        bit_cnt <= 4'd0;
                        state <= SHIFT;
                    end
                end

                SHIFT: begin
                    bit_cnt <= bit_cnt + 4'd1;

                    if (bit_cnt[0] == 1'b0) begin
                        // Even count: raise clock (data is sampled by display)
                        spi_clk_reg <= 1'b1;
                    end else begin
                        // Odd count: lower clock + shift data
                        spi_clk_reg <= 1'b0;
                        if (bit_cnt == 4'd15) begin
                            // Last falling edge — byte complete
                            state <= IDLE;
                        end else begin
                            // Shift next bit out
                            shift_reg <= {shift_reg[6:0], 1'b0};
                        end
                    end
                end
            endcase
        end
    end

endmodule
