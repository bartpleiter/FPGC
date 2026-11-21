// UART receiver
module uart_rx #(
    parameter CLOCKS_PER_BIT = 100
) (
    input  wire       clk,
    input  wire       reset,

    input  wire       rx,

    output reg        valid,          // high for one cycle
    output reg  [7:0] data,
    output reg        framing_error
);

localparam [15:0] BIT  = CLOCKS_PER_BIT;
localparam [15:0] HALF = CLOCKS_PER_BIT / 2;
localparam [15:0] BYTE = CLOCKS_PER_BIT * 10;

localparam [2:0] WAIT  = 3'd0;
localparam [2:0] IDLE  = 3'd1;
localparam [2:0] START = 3'd2;
localparam [2:0] DATA  = 3'd3;
localparam [2:0] STOP  = 3'd4;

reg        rx_sync;
reg        line;
reg [2:0]  state;
reg [15:0] clocks;
reg [2:0]  bits;

always @(posedge clk) begin
    rx_sync <= rx;
    line    <= rx_sync;
end

always @(posedge clk) begin
    valid         <= 1'b0;
    framing_error <= 1'b0;
    clocks        <= clocks + 16'd1;
    if (reset) begin
        state  <= WAIT;
        clocks <= 16'd0;
        bits   <= 3'd0;
        data   <= 8'd0;
    end else case (state)
        WAIT:
            if (!line)
                clocks <= 16'd0;
            else if (clocks == BYTE - 16'd1)
                state <= IDLE;
        IDLE: begin
            clocks <= 16'd0;
            if (!line)
                state <= START;
        end
        START:
            if (clocks == HALF - 16'd1) begin
                clocks <= 16'd0;
                bits   <= 3'd0;
                state  <= line ? IDLE : DATA;
            end
        DATA:
            if (clocks == BIT - 16'd1) begin
                clocks <= 16'd0;
                data   <= {line, data[7:1]};
                bits   <= bits + 3'd1;
                if (bits == 3'd7)
                    state <= STOP;
            end
        STOP:
            if (clocks == BIT - 16'd1) begin
                clocks <= 16'd0;
                if (line) begin
                    valid <= 1'b1;
                    state <= IDLE;
                end else begin
                    framing_error <= 1'b1;
                    state         <= WAIT;
                end
            end
        default:
            state <= WAIT;
    endcase
end

endmodule
