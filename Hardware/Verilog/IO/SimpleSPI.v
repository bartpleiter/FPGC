module SimpleSPI
#(parameter CLKS_PER_HALF_BIT = 1)
(
    // Control/Data Signals,
    input       reset,
    input       clk,

    // TX (MOSI) Signals
    input [7:0] in_byte,
    input       start,

    // RX (MISO) Signals
    output      done,
    output reg [7:0] out_byte = 8'd0,

    // SPI Interface
    output reg  spi_clk = 1'b0,
    input       miso,
    output      mosi
);

reg o_SPI_MOSI = 1'b0;

reg [$clog2(CLKS_PER_HALF_BIT*2)-1:0] r_SPI_clk_Count = 0;
reg r_SPI_clk = 1'b0;
reg [4:0] r_SPI_clk_Edges = 1'b0;
reg r_Leading_Edge = 1'b0;
reg r_Trailing_Edge = 1'b0;
reg       r_TX_DV = 1'b0;
reg [7:0] r_TX_Byte = 1'b0;

reg [2:0] r_RX_Bit_Count = 1'b0;
reg [2:0] r_TX_Bit_Count = 1'b0;

reg o_TX_Ready = 1'b0;


assign mosi = (o_TX_Ready) ? 1'bz : o_SPI_MOSI;
assign done = o_TX_Ready;


// Purpose: Generate SPI Clock correct number of times when DV pulse comes
always @(posedge clk)
begin
    if (reset)
    begin
        o_TX_Ready      <= 1'b0;
        r_SPI_clk_Edges <= 1'b0;
        r_Leading_Edge  <= 1'b0;
        r_Trailing_Edge <= 1'b0;
        r_SPI_clk       <= 1'b0; // assign default state to idle state
        r_SPI_clk_Count <= 1'b0;
        r_TX_Byte <= 8'h00;
        r_TX_DV   <= 1'b0;
    end
    else
    begin

        // Default assignments
        r_Leading_Edge  <= 1'b0;
        r_Trailing_Edge <= 1'b0;

        r_TX_DV <= start;

        if (r_SPI_clk_Edges == 0)
            begin
            if (start)
            begin
                r_TX_Byte <= in_byte;
                o_TX_Ready      <= 1'b0;
                r_SPI_clk_Edges <= 16;  // Total # edges in one byte ALWAYS 16
            end
            else
            begin
                o_TX_Ready <= 1'b1;
            end
        end
        else
        begin
            o_TX_Ready <= 1'b0;

            if (r_SPI_clk_Count == CLKS_PER_HALF_BIT*2-1)
            begin
                r_SPI_clk_Edges <= r_SPI_clk_Edges - 1'b1;
                r_Trailing_Edge <= 1'b1;
                r_SPI_clk_Count <= 0;
                r_SPI_clk       <= ~r_SPI_clk;
            end
            else if (r_SPI_clk_Count == CLKS_PER_HALF_BIT-1)
            begin
                r_SPI_clk_Edges <= r_SPI_clk_Edges - 1'b1;
                r_Leading_Edge  <= 1'b1;
                r_SPI_clk_Count <= r_SPI_clk_Count + 1'b1;
                r_SPI_clk       <= ~r_SPI_clk;
            end
            else
            begin
                r_SPI_clk_Count <= r_SPI_clk_Count + 1'b1;
            end
        end  
    end // else: !if(reset)
end // always @ (posedge clk)


// Purpose: Generate MOSI data
// Works with both CPHA=0 and CPHA=1
always @(posedge clk)
begin
    if (reset)
    begin
        o_SPI_MOSI     <= 1'b0;
        r_TX_Bit_Count <= 3'b111; // send MSb first
    end
    else
    begin
        // If ready is high, reset bit counts to default
        if (o_TX_Ready)
        begin
            r_TX_Bit_Count <= 3'b111;
        end
        // Catch the case where we start transaction and CPHA = 0
        else if (r_SPI_clk_Edges == 5'd16)
        begin
            o_SPI_MOSI     <= r_TX_Byte[7];
            r_TX_Bit_Count <= 3'b110;
        end
        else
        if (r_Trailing_Edge)
        begin
            r_TX_Bit_Count <= r_TX_Bit_Count - 1'b1;
            o_SPI_MOSI     <= r_TX_Byte[r_TX_Bit_Count];
        end
    end
end


// Purpose: Read in MISO data.
always @(posedge clk)
begin
    if (reset)
    begin
        out_byte      <= 8'h00;
        r_RX_Bit_Count <= 3'b111;
    end
    else
    begin
        if (o_TX_Ready) // Check if ready is high, if so reset bit count to default
        begin
            r_RX_Bit_Count <= 3'b111;
        end
        else if (r_Leading_Edge)
        begin
            out_byte[r_RX_Bit_Count] <= miso;  // Sample data
            r_RX_Bit_Count            <= r_RX_Bit_Count - 1'b1;
        end
    end
end


// Purpose: Add clock delay to signals for alignment.
always @(posedge clk)
begin
    if (reset)
    begin
        spi_clk  <= 1'd0;
    end
    else
    begin
        spi_clk <= r_SPI_clk;
    end // else: !if(reset)
end // always @ (posedge clk)


endmodule // SPI_Master