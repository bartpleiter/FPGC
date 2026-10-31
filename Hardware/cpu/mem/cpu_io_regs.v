// CPU-internal I/O registers
// The error register records illegal events with a sticky flag per
// kind of event, and the kind and instruction address of the first one. Next to the
// registers in the region, this keeps the rest of the interrupt state: whether
// interrupts are enabled, and the last interrupt taken.
module cpu_io_regs (
    input  wire        clk,
    input  wire        reset,

    // An access to the region. Only a word access to a register is allowed, and ERR_PC
    // can't be written.
    input  wire [3:2]  addr,           // the word inside the region
    input  wire        word,           // a word access
    output wire        bad_read,
    output wire        bad_write,
    output reg  [31:0] read_value,     // 0 for a bad read

    // A write at this clock edge. A bad write does nothing.
    input  wire        write,
    input  wire [31:0] write_data,

    // SP, which the stack keeps
    input  wire [7:0]  sp,
    output wire        sp_write,

    // The address of the instruction at this clock edge
    input  wire [31:0] pc,

    // The events at this clock edge, bit n for kind n + 1. They go in after a write to
    // ERR_STATUS by the same instruction.
    input  wire [5:0]  events,

    // An interrupt taken at this clock edge, instead of the instruction, or a RETI
    input  wire        take_interrupt,
    input  wire [7:0]  interrupt_id,
    input  wire        reti,
    output reg  [31:0] pc_backup,
    output reg         interrupts_enabled,
    output reg  [7:0]  last_interrupt  // what INTID reads
);

// The register addresses, bits [3:2] of the address
localparam PC_BACKUP  = 2'd0;
localparam SP         = 2'd1;
localparam ERR_STATUS = 2'd2;
localparam ERR_PC     = 2'd3;

wire [1:0] register = addr[3:2];

reg [5:0]  err_flags;
reg [2:0]  err_first;  // the kind of the first event, or 0
reg [31:0] err_pc;

assign bad_read  = !word;
assign bad_write = !word || register == ERR_PC;

always @(*) begin
    case (register)
        PC_BACKUP:  read_value = pc_backup;
        SP:         read_value = {24'd0, sp};
        ERR_STATUS: read_value = {21'd0, err_first, 2'd0, err_flags};
        default:    read_value = err_pc;
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

// Interrupts are enabled after reset. An interrupt disables them, and RETI enables them
// again.
always @(posedge clk) begin
    if (reset) begin
        pc_backup          <= 32'd0;
        interrupts_enabled <= 1'b1;
        last_interrupt     <= 8'd0;
        err_flags          <= 6'd0;
        err_first          <= 3'd0;
        err_pc             <= 32'd0;
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
