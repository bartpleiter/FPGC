// Timer
module timer (
    input  wire        clk,
    input  wire        reset,

    input  wire        start,          // count down from start_count, 0 stops
    input  wire [31:0] start_count,
    input  wire        configure,      // takes the two settings below
    input  wire        periodic_in,
    input  wire        irq_enable_in,
    input  wire        clear,          // clears expired

    output reg  [31:0] count,          // the cycles left
    output reg         periodic,       // start again at 0
    output reg         irq_enable,
    output reg         expired,
    output wire        irq
);

reg [31:0] reload;

wire expire = count == 32'd1;

always @(posedge clk) begin
    if (reset) begin
        count      <= 32'd0;
        reload     <= 32'd0;
        periodic   <= 1'b0;
        irq_enable <= 1'b0;
        expired    <= 1'b0;
    end else begin
        if (start) begin
            count  <= start_count;
            reload <= start_count;
        end else if (expire) begin
            count <= periodic ? reload : 32'd0;
        end else if (count != 32'd0) begin
            count <= count - 32'd1;
        end
        if (configure) begin
            periodic   <= periodic_in;
            irq_enable <= irq_enable_in;
        end
        expired <= expire || (expired && !clear);
    end
end

assign irq = expired && irq_enable;

endmodule
