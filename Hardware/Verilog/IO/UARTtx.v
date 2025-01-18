//////////////////////////////////////////////////////////////////////
// File Downloaded from http://www.nandland.com
//////////////////////////////////////////////////////////////////////
// This file contains the UART Transmitter. This transmitter is able
// to transmit 8 bits of serial data, one start bit, one stop bit,
// and no parity bit. When transmit is complete o_Tx_done will be
// driven high for one clock cycle.
//
// Set Parameter CLKS_PER_BIT as follows:
// CLKS_PER_BIT = (Frequency of i_Clock)/(Frequency of UART)
// Example: 25 MHz Clock, 115200 baud UART
// (25000000)/(115200) = 217

module UARTtx(
    input       i_Clock,
    input       reset,
    input       i_Tx_DV,
    input [7:0] i_Tx_Byte,
    output      o_Tx_Active,
    output  reg o_Tx_Serial,
    output  o_Tx_Done
    );

localparam CLKS_PER_BIT   = 50; //1MBaud
localparam s_IDLE = 3'b000;
localparam s_TX_START_BIT = 3'b001;
localparam s_TX_DATA_BITS = 3'b010;
localparam s_TX_STOP_BIT = 3'b011;
localparam s_CLEANUP = 3'b100;


reg [2:0] r_SM_Main;
reg [8:0] r_Clock_Count;
reg [2:0] r_Bit_Index;
reg [7:0] r_Tx_Data;
reg r_Tx_Done;
reg r_Tx_Active;

initial
begin
    r_SM_Main = 3'd0;
    r_Clock_Count = 9'd0;
    r_Bit_Index = 3'd0;
    r_Tx_Data = 8'd0;
    r_Tx_Done = 1'b0;
    r_Tx_Active = 1'b0;
    o_Tx_Serial = 1'b1;
end

/*
always @(posedge i_Clock)
begin
    if (reset)
    begin
        o_Tx_Done_l <= 1'b0;
    end
    else
    begin
        o_Tx_Done_l <= o_Tx_Done;
    end
end
*/

always @(posedge i_Clock)
begin
    if (reset)
    begin
        r_SM_Main <= 3'd0;
        r_Clock_Count <= 9'd0;
        r_Bit_Index <= 3'd0;
        r_Tx_Data <= 8'd0;
        r_Tx_Done <= 1'b0;
        r_Tx_Active <= 1'b0;
        o_Tx_Serial <= 1'b1;
    end
    else
    begin
        case (r_SM_Main)
        s_IDLE :
        begin
            o_Tx_Serial <= 1'b1; // Drive Line High for Idle
            r_Tx_Done <= 1'b0;
            r_Clock_Count <= 0;
            r_Bit_Index <= 0;

            if (i_Tx_DV == 1'b1)
            begin
                r_Tx_Active <= 1'b1;
                r_Tx_Data <= i_Tx_Byte;
                r_SM_Main <= s_TX_START_BIT;
            end
            else
                r_SM_Main <= s_IDLE;
        end // case: s_IDLE


        // Send out Start Bit. Start bit = 0
        s_TX_START_BIT :
        begin
            o_Tx_Serial <= 1'b0;

            // Wait CLKS_PER_BIT-1 clock cycles for start bit to finish
            if (r_Clock_Count < CLKS_PER_BIT-1)
                begin
                r_Clock_Count <= r_Clock_Count + 1;
                r_SM_Main <= s_TX_START_BIT;
            end
            else
            begin
                r_Clock_Count <= 0;
                r_SM_Main <= s_TX_DATA_BITS;
            end
        end // case: s_TX_START_BIT


        // Wait CLKS_PER_BIT-1 clock cycles for data bits to finish 
        s_TX_DATA_BITS :
        begin
            o_Tx_Serial <= r_Tx_Data[r_Bit_Index];

            if (r_Clock_Count < CLKS_PER_BIT-1)
            begin
                r_Clock_Count <= r_Clock_Count + 1;
                r_SM_Main <= s_TX_DATA_BITS;
            end
            else
            begin
                r_Clock_Count <= 0;

                // Check if we have sent out all bits
                if (r_Bit_Index < 7)
                begin
                    r_Bit_Index <= r_Bit_Index + 1;
                    r_SM_Main <= s_TX_DATA_BITS;
                end
                else
                begin
                    r_Bit_Index <= 0;
                    r_SM_Main <= s_TX_STOP_BIT;
                end
            end
        end // case: s_TX_DATA_BITS


        // Send out Stop bit. Stop bit = 1
        s_TX_STOP_BIT :
        begin
            o_Tx_Serial <= 1'b1;

            // Wait CLKS_PER_BIT-1 clock cycles for Stop bit to finish
            if (r_Clock_Count < CLKS_PER_BIT-1)
            begin
                r_Clock_Count <= r_Clock_Count + 1;
                r_SM_Main <= s_TX_STOP_BIT;
            end
            else
            begin
                r_Tx_Done <= 1'b1;
                r_Clock_Count <= 0;
                r_SM_Main <= s_CLEANUP;
                r_Tx_Active <= 1'b0;
            end
        end // case: s_Tx_STOP_BIT


        // Stay here 1 clock
        s_CLEANUP :
        begin
            r_Tx_Done <= 1'b1;

            if (!i_Tx_DV) r_SM_Main <= s_IDLE;
            else r_SM_Main <= s_CLEANUP;
        end


        default :
            r_SM_Main <= s_IDLE;

        endcase
    end
end

assign o_Tx_Active = r_Tx_Active;
assign o_Tx_Done = r_Tx_Done;

endmodule