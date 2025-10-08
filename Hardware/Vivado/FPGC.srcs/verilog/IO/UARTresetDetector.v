// UARTresetDetector listens to shifts in received UART bytes,
//  and set magic_detected high if a match is found with a static sequence
// The idea is that this magic_detected signal can be used as a reset, which resets this module too
module UARTresetDetector #(
    parameter MAGIC_LEN = 32,
    parameter [MAGIC_LEN*8-1:0] MAGIC_SEQUENCE = 256'h5C6A7408D53522204F5BE72AFC0F9FCE119BE20DAB4E910E61D73E1F0F99F684
) (
    input wire clk,
    input wire reset,
    
    // UART RX interface
    input wire rx_valid,          // data valid strobe from UART
    input wire [7:0] rx_data,     // received byte
    
    // Output
    output reg magic_detected = 1'b0
);

    reg rx_valid_prev = 1'b0;

    // Shift register to hold last 32 bytes received
    reg [MAGIC_LEN*8-1:0] shift_reg = 32'd0;
    
    always @(posedge clk)
    begin
        if (reset)
        begin
            shift_reg <= 0;
            magic_detected <= 1'b0;
            rx_valid_prev <= 1'b0;
        end
        else
        begin
            rx_valid_prev <= rx_valid;

            if (rx_valid && !rx_valid_prev)
            begin
                // Shift in new byte
                shift_reg <= {shift_reg[MAGIC_LEN*8-9:0], rx_data};
                
                // Check if shifted register matches magic sequence
                if ({shift_reg[MAGIC_LEN*8-9:0], rx_data} == MAGIC_SEQUENCE)
                begin
                    magic_detected <= 1'b1;
                end
                else
                begin
                    magic_detected <= 1'b0;
                end
            end
        end
    end

endmodule
