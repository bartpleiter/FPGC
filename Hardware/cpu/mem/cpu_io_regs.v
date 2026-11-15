// CPU-internal I/O registers
module cpu_io_regs (
    input  wire        clk,
    input  wire        reset,

    input  wire [4:2]  addr,           // the word inside the I/O region
    input  wire        word,           // a word access
    output wire        bad_read,
    output wire        bad_write,
    output reg  [31:0] read_value,

    input  wire        write,
    input  wire [31:0] write_data,

    // Hardware stack pointer
    input  wire [7:0]  sp,
    output wire        sp_write,

    // The current program counter at this clock edge
    input  wire [31:0] pc,

    // Error events
    input  wire [5:0]  events,

    input  wire        take_interrupt,
    input  wire [7:0]  interrupt_id,
    input  wire        reti,
    output reg  [31:0] pc_backup,
    output reg         interrupts_enabled,
    output reg  [7:0]  last_interrupt  // what INTID reads
);

// Addresses (words) within the I/O region
localparam PC_BACKUP  = 3'd0;
localparam SP         = 3'd1;
localparam ERR_STATUS = 3'd2;
localparam ERR_PC     = 3'd3;
localparam INT_ENABLE = 3'd4;

wire [2:0] register = addr[4:2];

// The error register starts at 0 when the FPGA is configured, and isn't reset.
reg [5:0]  err_flags = 6'd0;
reg [2:0]  err_first = 3'd0;  // the kind of the first event, or 0
reg [31:0] err_pc    = 32'd0;

assign bad_read  = !word;
assign bad_write = !word || register == ERR_PC;

always @(*) begin
    case (register)
        PC_BACKUP:  read_value = pc_backup;
        SP:         read_value = {24'd0, sp};
        ERR_STATUS: read_value = {21'd0, err_first, 2'd0, err_flags};
        ERR_PC:     read_value = err_pc;
        default:    read_value = {31'd0, interrupts_enabled};
    endcase
    if (bad_read)
        read_value = 32'd0;
end

wire writes = write && !bad_write;
wire clear  = writes && register == ERR_STATUS;

assign sp_write = writes && register == SP;

// The lowest kind counts as the first when one instruction has several events.
wire [2:0] lowest_kind = events[0] ? 3'd1 :
                         events[1] ? 3'd2 :
                         events[2] ? 3'd3 :
                         events[3] ? 3'd4 :
                         events[4] ? 3'd5 :
                                     3'd6;

wire first = events != 6'd0 && (clear || err_first == 3'd0);

// Interrupt register handling
always @(posedge clk) begin
    if (reset) begin
        pc_backup          <= 32'd0;
        interrupts_enabled <= 1'b0;
        last_interrupt     <= 8'd0;
    end else begin
        if (take_interrupt) begin
            pc_backup          <= pc;
            interrupts_enabled <= 1'b0;
            last_interrupt     <= interrupt_id;
        end else if (reti) begin
            interrupts_enabled <= 1'b1;
        end
        if (writes && register == PC_BACKUP)
            pc_backup <= write_data;
        if (writes && register == INT_ENABLE)
            interrupts_enabled <= write_data[0];
    end
end

always @(posedge clk) begin
    if (!reset) begin
        err_flags <= (clear ? 6'd0 : err_flags) | events;
        if (first) begin
            err_first <= lowest_kind;
            err_pc    <= pc;
        end else if (clear) begin
            err_first <= 3'd0;
            err_pc    <= 32'd0;
        end
    end
end

endmodule
