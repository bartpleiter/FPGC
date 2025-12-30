/*
 * InterruptController with Clock Domain Crossing (CDC) handling
 * Gives priority to lower numbered interrupts
 * Outputs a single interrupt signal and ID to the CPU
 * When multiple interrupts happen at the same time, the lower priority ones are queued
 *
 * On interrupt, CPU should set intDisabled high, save the PC and jump to ADDR 1
 * and restore everything on reti by setting intDisabled to low and jumping to the saved PC
 */
module InterruptController #(
    parameter NUM_INTERRUPTS = 8 // Changing this requires changing the priority encoder logic below
) (
    input wire                       clk,
    input wire                       reset,

    input wire [NUM_INTERRUPTS-1:0]  interrupts,  // int1=bit0, int2=bit1, etc.
    input wire                       intDisabled,

    output reg                       intCPU = 1'b0,
    output reg [7:0]                 intID = 8'd0
);

// CDC: Two-stage synchronizer for each interrupt input
// This prevents metastability from propagating
(* ASYNC_REG = "TRUE" *) reg [NUM_INTERRUPTS-1:0] int_sync1 = {NUM_INTERRUPTS{1'b0}};
(* ASYNC_REG = "TRUE" *) reg [NUM_INTERRUPTS-1:0] int_sync2 = {NUM_INTERRUPTS{1'b0}};

reg [NUM_INTERRUPTS-1:0] int_prev = {NUM_INTERRUPTS{1'b0}};
reg [NUM_INTERRUPTS-1:0] int_triggered = {NUM_INTERRUPTS{1'b0}};

integer i;

// First always block: Synchronization chain
// This MUST be separate to ensure proper synchronizer implementation
always @(posedge clk) 
begin
    if (reset)
    begin
        int_sync1 <= {NUM_INTERRUPTS{1'b0}};
        int_sync2 <= {NUM_INTERRUPTS{1'b0}};
    end
    else
    begin
        int_sync1 <= interrupts;      // First stage (may go metastable)
        int_sync2 <= int_sync1;        // Second stage (metastability resolved)
    end
end

// Second always block: Edge detection and interrupt handling
// Now operating on fully synchronized signals
always @(posedge clk) 
begin
    if (reset)
    begin
        int_prev <= {NUM_INTERRUPTS{1'b0}};
        int_triggered <= {NUM_INTERRUPTS{1'b0}};
        intCPU <= 1'b0;
        intID <= 8'd0;
    end
    else
    begin
        // Edge detection on SYNCHRONIZED signals
        int_prev <= int_sync2;
        
        // Detect rising edges
        for (i = 0; i < NUM_INTERRUPTS; i = i + 1)
        begin
            if (int_sync2[i] && !int_prev[i])
                int_triggered[i] <= 1'b1;
        end
        
        // Handle interrupt requests with priority encoding
        if (!intDisabled && !intCPU)
        begin
            // Priority encoder: lowest index has highest priority
            if (int_triggered[0])
            begin
                int_triggered[0] <= 1'b0;
                intCPU <= 1'b1;
                intID <= 8'd1; // Interrupt IDs start at 1
            end
            else if (int_triggered[1])
            begin
                int_triggered[1] <= 1'b0;
                intCPU <= 1'b1;
                intID <= 8'd2;
            end
            else if (int_triggered[2])
            begin
                int_triggered[2] <= 1'b0;
                intCPU <= 1'b1;
                intID <= 8'd3;
            end
            else if (int_triggered[3])
            begin
                int_triggered[3] <= 1'b0;
                intCPU <= 1'b1;
                intID <= 8'd4;
            end
            else if (int_triggered[4])
            begin
                int_triggered[4] <= 1'b0;
                intCPU <= 1'b1;
                intID <= 8'd5;
            end
            else if (int_triggered[5])
            begin
                int_triggered[5] <= 1'b0;
                intCPU <= 1'b1;
                intID <= 8'd6;
            end
            else if (int_triggered[6])
            begin
                int_triggered[6] <= 1'b0;
                intCPU <= 1'b1;
                intID <= 8'd7;
            end
            else if (int_triggered[7])
            begin
                int_triggered[7] <= 1'b0;
                intCPU <= 1'b1;
                intID <= 8'd8;
            end
        end
        
        // Clear intCPU when interrupts are disabled
        if (intDisabled)
        begin
            intCPU <= 1'b0;
        end
    end
end

endmodule
