module MIG7Mock
#(
    parameter ADDR_WIDTH = 29,
    parameter DATA_WIDTH = 256,
    parameter MASK_WIDTH = 32,
    parameter RAM_DEPTH = 1024,  // Configurable RAM depth for simulation
    parameter LIST = "memory/mig7mock.list" // Initialization file for RAM
)
(
    // System interface
    input                           sys_clk_i,
    input                           sys_rst,
    output                          ui_clk,
    output                          ui_clk_sync_rst,
    output                          init_calib_complete,
    
    // Application interface
    input [ADDR_WIDTH-1:0]          app_addr,
    input [2:0]                     app_cmd,
    input                           app_en,
    output                          app_rdy,
    
    // Write data interface
    input [DATA_WIDTH-1:0]          app_wdf_data,
    input                           app_wdf_end,
    input [MASK_WIDTH-1:0]          app_wdf_mask,
    input                           app_wdf_wren,
    output                          app_wdf_rdy,
    
    // Read data interface
    output [DATA_WIDTH-1:0]         app_rd_data,
    output                          app_rd_data_end,
    output                          app_rd_data_valid,
    
    // Unused signals (tied off)
    input                           app_sr_req,
    input                           app_ref_req,
    input                           app_zq_req,
    output                          app_sr_active,
    output                          app_ref_ack,
    output                          app_zq_ack
);

// Clock and reset generation
assign ui_clk = sys_clk_i;
assign ui_clk_sync_rst = sys_rst;

// Unused signal tie-offs
assign app_sr_active = 1'b0;
assign app_ref_ack = 1'b0;
assign app_zq_ack = 1'b0;

// State machine parameters
localparam INIT_CYCLES = 50;
localparam READ_CYCLES = 10;
localparam WRITE_CYCLES = 10;

// State definitions
localparam INIT = 3'b000;
localparam IDLE = 3'b001;
localparam WRITE_CMD = 3'b010;
localparam WRITE_DATA = 3'b011;
localparam READ_CMD = 3'b100;
localparam READ_DATA = 3'b101;

// Internal registers
reg [2:0] state = 3'b000;
reg [7:0] cycle_counter = 8'd0;
reg init_complete = 1'b0;
reg app_rdy_reg = 1'b0;
reg app_wdf_rdy_reg = 1'b0;
reg [DATA_WIDTH-1:0] app_rd_data_reg = {DATA_WIDTH{1'b0}};
reg app_rd_data_valid_reg = 1'b0;
reg app_rd_data_end_reg = 1'b0;

// Command and address storage
reg [ADDR_WIDTH-1:0] stored_addr = {ADDR_WIDTH{1'b0}};
reg [2:0] stored_cmd = 3'b000;

// RAM storage
reg [DATA_WIDTH-1:0] ram_memory [0:RAM_DEPTH-1];
wire [9:0] ram_addr; // 10 bits for 1024 depth

// Extract word address from byte address (divide by 32 since 256 bits = 32 bytes)
assign ram_addr = stored_addr; // Use stored address instead of live app_addr

// Output assignments
assign init_calib_complete = init_complete;
assign app_rdy = app_rdy_reg;
assign app_wdf_rdy = app_wdf_rdy_reg;
assign app_rd_data = app_rd_data_valid_reg ? app_rd_data_reg : {DATA_WIDTH{1'b0}};
assign app_rd_data_valid = app_rd_data_valid_reg;
assign app_rd_data_end = app_rd_data_end_reg;

// Initialize RAM from file
initial begin
    $readmemb(LIST, ram_memory);
end

// Main state machine
always @(posedge ui_clk) begin
    if (ui_clk_sync_rst) begin
        state <= INIT;
        cycle_counter <= 8'd0;
        init_complete <= 1'b0;
        app_rdy_reg <= 1'b0;
        app_wdf_rdy_reg <= 1'b0;
        app_rd_data_reg <= {DATA_WIDTH{1'b0}};
        app_rd_data_valid_reg <= 1'b0;
        app_rd_data_end_reg <= 1'b0;
        stored_addr <= {ADDR_WIDTH{1'b0}};
        stored_cmd <= 3'b000;
    end
    else begin
        case (state)
            INIT: begin
                // Initialization phase
                app_rdy_reg <= 1'b0;
                app_wdf_rdy_reg <= 1'b0;
                app_rd_data_valid_reg <= 1'b0;
                app_rd_data_end_reg <= 1'b0;
                
                if (cycle_counter < INIT_CYCLES - 1) begin
                    cycle_counter <= cycle_counter + 1;
                end
                else begin
                    init_complete <= 1'b1;
                    state <= IDLE;
                    cycle_counter <= 8'd0;
                end
            end
            
            IDLE: begin
                // Ready to accept commands
                app_rdy_reg <= 1'b1;
                app_wdf_rdy_reg <= 1'b1;
                app_rd_data_valid_reg <= 1'b0;
                app_rd_data_end_reg <= 1'b0;
                
                if (app_en && app_rdy_reg) begin
                    stored_addr <= app_addr;
                    stored_cmd <= app_cmd;
                    app_rdy_reg <= 1'b0;
                    cycle_counter <= 8'd0;
                    
                    if (app_cmd == 3'b000) begin // Write command
                        state <= WRITE_CMD;
                        app_wdf_rdy_reg <= 1'b1; // Keep write data ready high initially
                    end
                    else if (app_cmd == 3'b001) begin // Read command
                        state <= READ_CMD;
                        app_wdf_rdy_reg <= 1'b0;
                    end
                end
            end
            
            WRITE_CMD: begin
                // Wait for write data
                app_rdy_reg <= 1'b0;
                
                if (app_wdf_wren && app_wdf_rdy_reg && app_wdf_end) begin
                    // Write data to RAM (apply mask if needed)
                    if (ram_addr < RAM_DEPTH) begin
                        // Simple implementation: if mask bit is 0, write the byte
                        // For simplicity, we'll write the full word if any mask bit allows it
                        if (app_wdf_mask != {MASK_WIDTH{1'b1}}) begin
                            ram_memory[ram_addr] <= app_wdf_data;
                            $display("Time %0t: MIG7Mock WRITE: addr=0x%h, data=0x%h", $time, stored_addr, app_wdf_data);
                        end else begin
                            $display("Time %0t: MIG7Mock WRITE MASKED: addr=0x%h, mask=0x%h (write blocked)", $time, stored_addr, app_wdf_mask);
                        end
                    end else begin
                        $display("Time %0t: MIG7Mock WRITE OUT-OF-BOUNDS: addr=0x%h (>= 0x%h)", $time, stored_addr, RAM_DEPTH);
                    end
                    
                    app_wdf_rdy_reg <= 1'b0;
                    state <= WRITE_DATA;
                    cycle_counter <= 8'd0;
                end
            end
            
            WRITE_DATA: begin
                // Write processing delay
                app_rdy_reg <= 1'b0;
                app_wdf_rdy_reg <= 1'b0;
                
                if (cycle_counter < WRITE_CYCLES - 1) begin
                    cycle_counter <= cycle_counter + 1;
                end
                else begin
                    state <= IDLE;
                    cycle_counter <= 8'd0;
                end
            end
            
            READ_CMD: begin
                // Read processing delay
                app_rdy_reg <= 1'b0;
                app_wdf_rdy_reg <= 1'b0;
                
                if (cycle_counter < READ_CYCLES - 1) begin
                    cycle_counter <= cycle_counter + 1;
                end
                else begin
                    // Load data from RAM
                    if (ram_addr < RAM_DEPTH) begin
                        app_rd_data_reg <= ram_memory[ram_addr];
                        $display("Time %0t: MIG7Mock READ: addr=0x%h, data=0x%h", $time, stored_addr, ram_memory[ram_addr]);
                    end
                    else begin
                        app_rd_data_reg <= {DATA_WIDTH{1'b0}}; // Return zeros for out-of-bounds
                        $display("Time %0t: MIG7Mock READ OUT-OF-BOUNDS: addr=0x%h (>= 0x%h), returning 0x0", $time, stored_addr, RAM_DEPTH);
                    end
                    
                    state <= READ_DATA;
                    cycle_counter <= 8'd0;
                end
            end
            
            READ_DATA: begin
                // Output read data
                app_rdy_reg <= 1'b0;
                app_wdf_rdy_reg <= 1'b0;
                app_rd_data_valid_reg <= 1'b1;
                app_rd_data_end_reg <= 1'b1;
                
                // Return to idle after one cycle
                state <= IDLE;
            end
            
            default: begin
                state <= INIT;
            end
        endcase
    end
end

endmodule
