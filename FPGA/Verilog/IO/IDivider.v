/*
* 32-bit multicycle signed or unsigned integer divider
*/

module IDivider #(
    parameter DATA_WIDTH = 32
) (
    input clk,
    input rst,
    input [DATA_WIDTH-1:0] a,
    input [DATA_WIDTH-1:0] b,
    input signed_ope,
    input write_a,
    input start,
    input flush,
    output reg [DATA_WIDTH-1:0] quotient,
    output reg [DATA_WIDTH-1:0] remainder,
    output ready
);

  reg start_prev = 0;

  reg [DATA_WIDTH-1:0] dividend = 0;
  reg [DATA_WIDTH-1:0] divisor = 0;



  
  localparam COUNT_WIDTH = $clog2(DATA_WIDTH + 1);

  reg r_ready = 0;
  reg r_signed_ope = 0;
  reg [COUNT_WIDTH-1:0] r_count = 0;
  reg [DATA_WIDTH-1:0] r_quotient = 0;
  wire w_dividend_sign;
  reg r_dividend_sign = 0;
  wire remainder_sign;
  reg [DATA_WIDTH:0] r_remainder = 0;
  reg [DATA_WIDTH-1:0] r_divisor = 0;
  wire [DATA_WIDTH:0] divisor_ext;
  wire divisor_sign;
  wire [DATA_WIDTH:0] rem_quo;
  wire                diff_sign;
  wire [DATA_WIDTH:0] sub_add;

  assign ready = r_ready;

  assign divisor_sign = r_divisor[DATA_WIDTH-1] & r_signed_ope;
  assign divisor_ext = {divisor_sign, r_divisor};
  assign remainder_sign = r_remainder[DATA_WIDTH];

  assign rem_quo = {r_remainder[DATA_WIDTH-1:0], r_quotient[DATA_WIDTH-1]};
  assign diff_sign = remainder_sign ^ divisor_sign;
  assign sub_add = diff_sign ? rem_quo + divisor_ext :
                               rem_quo - divisor_ext;

  // after process
  always @(*) begin
    quotient  = (r_quotient << 1) | 1;
    remainder = r_remainder[DATA_WIDTH-1:0];

    if (r_remainder == 0) begin
      // do nothing
    end else if (r_remainder == divisor_ext) begin
      quotient  = quotient + 1;
      remainder = remainder - r_divisor;
    end else if (r_remainder == -divisor_ext) begin
      quotient  = quotient - 1;
      remainder = remainder + r_divisor;
    end else if (remainder_sign ^ r_dividend_sign) begin
      if (diff_sign) begin
        quotient  = quotient - 1;
        remainder = remainder + r_divisor;
      end else begin
        quotient  = quotient + 1;
        remainder = remainder - r_divisor;
      end
    end
  end

  assign w_dividend_sign = dividend[DATA_WIDTH-1] & signed_ope;

  always @(posedge clk)
  begin
      start_prev <= start;
      if (write_a)
      begin
        dividend <= a;
      end
      if (start && !start_prev)
      begin
          divisor <= b;
      end
  end

  always @(posedge clk) begin
    if (rst) begin
      r_quotient      <= 0;
      r_dividend_sign <= 0;
      r_remainder     <= 0;
      r_divisor       <= 0;
      r_count         <= 0;
      r_ready         <= 1'b1;
      r_signed_ope    <= 1'b0;
    end else begin
      if (flush) begin
        r_count         <= 0;
        r_ready         <= 1'b1;
      end else if (start && !start_prev) // use b for this first cycle, as divisor are latched during this cycle
      begin
        // RISC-V's div by 0 spec
        if (b == 0) begin
            r_quotient  <= 1;
            r_remainder <= {w_dividend_sign, dividend};
        end else begin
            r_quotient  <= dividend;
            r_remainder <= {(DATA_WIDTH+1){w_dividend_sign}};
            r_ready     <= 1'b0;
        end
        r_count         <= 0;
        r_dividend_sign <= w_dividend_sign;
        r_divisor       <= b;
        r_signed_ope    <= signed_ope;
      end else if (~ready) begin
        r_quotient  <= {r_quotient[DATA_WIDTH-2:0], ~diff_sign};
        r_remainder <= sub_add[DATA_WIDTH:0];
        r_count     <= r_count + 1;
        if (r_count == DATA_WIDTH - 1) begin
          r_ready <= 1'b1;
        end
      end
    end
  end
endmodule