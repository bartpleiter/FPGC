/*
 * MIG7 Arbiter
 * Arbitrates between L1i and L1d cache controllers for MIG7 memory access
 * Uses round-robin arbitration
 */

module MIG7Arbiter (
    input wire clk,
    input wire reset,
    
    // MIG7 interface (to memory controller)
    input wire init_calib_complete,
    output reg [28:0] app_addr,
    output reg [2:0] app_cmd,
    output reg app_en,
    input wire app_rdy,
    
    output reg [255:0] app_wdf_data,
    output reg app_wdf_end,
    output reg [31:0] app_wdf_mask,
    output reg app_wdf_wren,
    input wire app_wdf_rdy,
    
    input wire [255:0] app_rd_data,
    input wire app_rd_data_end,
    input wire app_rd_data_valid,
    
    // L1i cache controller interface
    input wire [28:0] l1i_app_addr,
    input wire [2:0] l1i_app_cmd,
    input wire l1i_app_en,
    output wire l1i_app_rdy,
    
    input wire [255:0] l1i_app_wdf_data,
    input wire l1i_app_wdf_end,
    input wire [31:0] l1i_app_wdf_mask,
    input wire l1i_app_wdf_wren,
    output wire l1i_app_wdf_rdy,
    
    output reg [255:0] l1i_app_rd_data,
    output reg l1i_app_rd_data_end,
    output reg l1i_app_rd_data_valid,
    
    // L1d cache controller interface
    input wire [28:0] l1d_app_addr,
    input wire [2:0] l1d_app_cmd,
    input wire l1d_app_en,
    output wire l1d_app_rdy,
    
    input wire [255:0] l1d_app_wdf_data,
    input wire l1d_app_wdf_end,
    input wire [31:0] l1d_app_wdf_mask,
    input wire l1d_app_wdf_wren,
    output wire l1d_app_wdf_rdy,
    
    output reg [255:0] l1d_app_rd_data,
    output reg l1d_app_rd_data_end,
    output reg l1d_app_rd_data_valid
);

// State machine for arbitration
localparam IDLE = 2'b00;
localparam L1I_ACTIVE = 2'b01;
localparam L1D_ACTIVE = 2'b10;

reg [1:0] state;
reg [1:0] next_state;

// Round-robin priority
reg l1i_priority;

// Transaction tracking
reg l1i_transaction_active;
reg l1d_transaction_active;

// State machine
always @(posedge clk or posedge reset) begin
    if (reset) begin
        state <= IDLE;
        l1i_priority <= 1'b1; // Start with L1i having priority
        l1i_transaction_active <= 1'b0;
        l1d_transaction_active <= 1'b0;
    end else begin
        state <= next_state;
        
        // Track transaction completion
        if (l1i_app_rd_data_valid && l1i_app_rd_data_end) begin
            l1i_transaction_active <= 1'b0;
        end else if (state == L1I_ACTIVE && l1i_app_en && app_rdy) begin
            l1i_transaction_active <= 1'b1;
        end
        
        if (l1d_app_rd_data_valid && l1d_app_rd_data_end) begin
            l1d_transaction_active <= 1'b0;
        end else if (state == L1D_ACTIVE && l1d_app_en && app_rdy) begin
            l1d_transaction_active <= 1'b1;
        end
        
        // Update priority for round-robin
        if (state == L1I_ACTIVE && !l1i_transaction_active && !l1i_app_en) begin
            l1i_priority <= 1'b0;
        end else if (state == L1D_ACTIVE && !l1d_transaction_active && !l1d_app_en) begin
            l1i_priority <= 1'b1;
        end
    end
end

// Next state logic
always @(*) begin
    next_state = state;
    
    case (state)
        IDLE: begin
            if (l1i_app_en && l1i_priority) begin
                next_state = L1I_ACTIVE;
            end else if (l1d_app_en && !l1i_priority) begin
                next_state = L1D_ACTIVE;
            end else if (l1i_app_en) begin
                next_state = L1I_ACTIVE;
            end else if (l1d_app_en) begin
                next_state = L1D_ACTIVE;
            end
        end
        
        L1I_ACTIVE: begin
            if (!l1i_transaction_active && !l1i_app_en) begin
                if (l1d_app_en) begin
                    next_state = L1D_ACTIVE;
                end else begin
                    next_state = IDLE;
                end
            end
        end
        
        L1D_ACTIVE: begin
            if (!l1d_transaction_active && !l1d_app_en) begin
                if (l1i_app_en) begin
                    next_state = L1I_ACTIVE;
                end else begin
                    next_state = IDLE;
                end
            end
        end
    endcase
end

// MIG7 output mux
always @(*) begin
    case (state)
        L1I_ACTIVE: begin
            app_addr = l1i_app_addr;
            app_cmd = l1i_app_cmd;
            app_en = l1i_app_en;
            app_wdf_data = l1i_app_wdf_data;
            app_wdf_end = l1i_app_wdf_end;
            app_wdf_mask = l1i_app_wdf_mask;
            app_wdf_wren = l1i_app_wdf_wren;
        end
        
        L1D_ACTIVE: begin
            app_addr = l1d_app_addr;
            app_cmd = l1d_app_cmd;
            app_en = l1d_app_en;
            app_wdf_data = l1d_app_wdf_data;
            app_wdf_end = l1d_app_wdf_end;
            app_wdf_mask = l1d_app_wdf_mask;
            app_wdf_wren = l1d_app_wdf_wren;
        end
        
        default: begin // IDLE
            app_addr = 29'b0;
            app_cmd = 3'b0;
            app_en = 1'b0;
            app_wdf_data = 256'b0;
            app_wdf_end = 1'b0;
            app_wdf_mask = 32'b0;
            app_wdf_wren = 1'b0;
        end
    endcase
end

// Ready signal routing
assign l1i_app_rdy = (state == L1I_ACTIVE) ? app_rdy : 1'b0;
assign l1d_app_rdy = (state == L1D_ACTIVE) ? app_rdy : 1'b0;

assign l1i_app_wdf_rdy = (state == L1I_ACTIVE) ? app_wdf_rdy : 1'b0;
assign l1d_app_wdf_rdy = (state == L1D_ACTIVE) ? app_wdf_rdy : 1'b0;

// Read data routing
always @(*) begin
    // Default values
    l1i_app_rd_data = 256'b0;
    l1i_app_rd_data_end = 1'b0;
    l1i_app_rd_data_valid = 1'b0;
    
    l1d_app_rd_data = 256'b0;
    l1d_app_rd_data_end = 1'b0;
    l1d_app_rd_data_valid = 1'b0;
    
    case (state)
        L1I_ACTIVE: begin
            l1i_app_rd_data = app_rd_data;
            l1i_app_rd_data_end = app_rd_data_end;
            l1i_app_rd_data_valid = app_rd_data_valid;
        end
        
        L1D_ACTIVE: begin
            l1d_app_rd_data = app_rd_data;
            l1d_app_rd_data_end = app_rd_data_end;
            l1d_app_rd_data_valid = app_rd_data_valid;
        end
    endcase
end

endmodule
