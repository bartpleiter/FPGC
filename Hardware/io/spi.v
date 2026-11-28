// SPI controller for one bus, in mode 0
module spi #(
    parameter HALF_PERIOD = 1,  // cycles per half of the SPI clock
    parameter QUAD        = 0   // receives can use all four data lines
) (
    input  wire        clk,
    input  wire        reset,

    // Transfers
    input  wire        send,           // sends send_data
    input  wire        receive,        // receives into received, sends 1s
    input  wire        word,           // with send or receive: 4 bytes, else 1
    input  wire [31:0] send_data,      // the first byte in bits 7:0
    output wire [31:0] received,       // the first byte in bits 7:0
    output wire        busy,

    // Settings
    input  wire        configure,      // takes the two settings below
    input  wire        select_in,
    input  wire        quad_in,
    output reg         select,         // the chip select is low
    output reg         quad,           // receives use IO0 to IO3

    // Pins, IO0 is MOSI and IO1 is MISO on one line
    output reg         spi_clk,
    output reg         cs_n,
    output reg  [3:0]  io_out,
    output reg  [3:0]  io_enable,
    input  wire [3:0]  io_in
);

localparam [7:0] HALF = HALF_PERIOD;

reg [30:0] tx;         // the bits after the one on IO0
reg [31:0] rx;         // received into bit 0
reg [5:0]  clocks;     // SPI clocks left
reg [7:0]  count;      // cycles in this half of the clock
reg        high;       // the SPI clock is high
reg        wide;       // this transfer uses four lines
reg        words;      // this transfer is a word
reg        rose;       // the SPI clock rose at the last edge
reg        capture;    // io_sampled holds the bits of the last rise
reg [3:0]  io_sampled; // in the pins' input registers

wire running   = clocks != 6'd0;
wire half_done = count == HALF - 8'd1;

assign busy = running || rose || capture || send || receive || configure;

// The first byte on the bus is the low byte
assign received = words ? {rx[7:0], rx[15:8], rx[23:16], rx[31:24]} : {24'd0, rx[7:0]};

// One cycle after the rise, the device's bits are in the input registers
always @(posedge clk)
    io_sampled <= io_in;

always @(posedge clk) begin
    if (reset) begin
        clocks    <= 6'd0;
        count     <= 8'd0;
        high      <= 1'b0;
        rose      <= 1'b0;
        capture   <= 1'b0;
        select    <= 1'b0;
        quad      <= 1'b0;
        spi_clk   <= 1'b0;
        cs_n      <= 1'b1;
        io_out    <= 4'b1111;
        io_enable <= 4'b1101;
    end else begin
        rose    <= 1'b0;
        capture <= rose;

        if (configure) begin
            select    <= select_in;
            cs_n      <= !select_in;
            quad      <= QUAD && quad_in;
            io_enable <= QUAD && quad_in ? 4'b0000 : 4'b1101;
        end

        // Start
        if (send || receive) begin
            wide   <= receive && quad;
            words  <= word;
            clocks <= receive && quad ? (word ? 6'd8 : 6'd2) : (word ? 6'd32 : 6'd8);
            count  <= 8'd0;
            {io_out[0], tx} <= receive ? 32'hFFFFFFFF
                                       : {send_data[7:0], send_data[15:8],
                                          send_data[23:16], send_data[31:24]};
        end else if (running) begin
            if (!half_done) begin
                count <= count + 8'd1;
            end else if (!high) begin
                // The rise, where the device takes a bit
                count   <= 8'd0;
                high    <= 1'b1;
                spi_clk <= 1'b1;
                rose    <= 1'b1;
            end else begin
                // The fall, with the next bit
                count   <= 8'd0;
                high    <= 1'b0;
                spi_clk <= 1'b0;
                clocks  <= clocks - 6'd1;
                {io_out[0], tx} <= {tx, 1'b1};
            end
        end

        if (capture)
            rx <= wide ? {rx[27:0], io_sampled} : {rx[30:0], io_sampled[1]};
    end
end

endmodule
