/*
* Interrupt controller
* On interrupt, CPU should save the PC and jump to ADDR 1
*  and restore everything on reti
*/

module IntController(
    input clk, reset,
    input int1, int2, int3, int4, int5, int6, int7, int8, int9, int10,

    input intDisabled,
    output reg intCPU = 1'b0,
    output reg [7:0] intID = 8'd0

);

reg int1_prev = 1'b0;
reg int2_prev = 1'b0;
reg int3_prev = 1'b0;
reg int4_prev = 1'b0;
reg int5_prev = 1'b0;
reg int6_prev = 1'b0;
reg int7_prev = 1'b0;
reg int8_prev = 1'b0;
reg int9_prev = 1'b0;
reg int10_prev = 1'b0;

reg int1_triggered = 1'b0;
reg int2_triggered = 1'b0;
reg int3_triggered = 1'b0;
reg int4_triggered = 1'b0;
reg int5_triggered = 1'b0;
reg int6_triggered = 1'b0;
reg int7_triggered = 1'b0;
reg int8_triggered = 1'b0;
reg int9_triggered = 1'b0;
reg int10_triggered = 1'b0;

always @(posedge clk) 
begin
    if (reset)
    begin
        int1_prev <= 1'b0;
        int2_prev <= 1'b0;
        int3_prev <= 1'b0;
        int4_prev <= 1'b0;
        int5_prev <= 1'b0;
        int6_prev <= 1'b0;
        int7_prev <= 1'b0;
        int8_prev <= 1'b0;
        int9_prev <= 1'b0;
        int10_prev <= 1'b0;

        int1_triggered <= 1'b0;
        int2_triggered <= 1'b0;
        int3_triggered <= 1'b0;
        int4_triggered <= 1'b0;
        int5_triggered <= 1'b0;
        int6_triggered <= 1'b0;
        int7_triggered <= 1'b0;
        int8_triggered <= 1'b0;
        int9_triggered <= 1'b0;
        int10_triggered <= 1'b0;

        intCPU <= 1'b0;
        intID <= 8'd0;
    end
    else
    begin
        int1_prev <= int1;
        int2_prev <= int2;
        int3_prev <= int3;
        int4_prev <= int4;
        int5_prev <= int5;
        int6_prev <= int6;
        int7_prev <= int7;
        int8_prev <= int8;
        int9_prev <= int9;
        int10_prev <= int10;

        if (int1 && !int1_prev)
            int1_triggered <= 1'b1;

        if (int2 && !int2_prev)
            int2_triggered <= 1'b1;

        if (int3 && !int3_prev)
            int3_triggered <= 1'b1;

        if (int4 && !int4_prev)
            int4_triggered <= 1'b1;

        if (int5 && !int5_prev)
            int5_triggered <= 1'b1;

        if (int6 && !int6_prev)
            int6_triggered <= 1'b1;

        if (int7 && !int7_prev)
            int7_triggered <= 1'b1;

        if (int8 && !int8_prev)
            int8_triggered <= 1'b1;

        if (int9 && !int9_prev)
            int9_triggered <= 1'b1;

        if (int10 && !int10_prev)
            int10_triggered <= 1'b1;

        // trigger new interrupt if available with priority to lowest number
        if (!intDisabled && !intCPU)
        begin
            if (int1_triggered)
            begin
                int1_triggered <= 1'b0;
                intCPU <= 1'b1;
                intID = 8'd1;
            end
            else if (int2_triggered)
            begin
                int2_triggered <= 1'b0;
                intCPU <= 1'b1;
                intID = 8'd2;
            end
            else if (int3_triggered)
            begin
                int3_triggered <= 1'b0;
                intCPU <= 1'b1;
                intID = 8'd3;
            end
            else if (int4_triggered)
            begin
                int4_triggered <= 1'b0;
                intCPU <= 1'b1;
                intID = 8'd4;
            end
            else if (int5_triggered)
            begin
                int5_triggered <= 1'b0;
                intCPU <= 1'b1;
                intID = 8'd5;
            end
            else if (int6_triggered)
            begin
                int6_triggered <= 1'b0;
                intCPU <= 1'b1;
                intID = 8'd6;
            end
            else if (int7_triggered)
            begin
                int7_triggered <= 1'b0;
                intCPU <= 1'b1;
                intID = 8'd7;
            end
            else if (int8_triggered)
            begin
                int8_triggered <= 1'b0;
                intCPU <= 1'b1;
                intID = 8'd8;
            end
            else if (int9_triggered)
            begin
                int9_triggered <= 1'b0;
                intCPU <= 1'b1;
                intID = 8'd9;
            end
            else if (int10_triggered)
            begin
                int10_triggered <= 1'b0;
                intCPU <= 1'b1;
                intID = 8'd10;
            end
        end

        if (intDisabled)
        begin
            intCPU <= 1'b0;
        end
    end
end

endmodule