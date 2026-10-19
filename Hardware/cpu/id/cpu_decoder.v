// Instruction decoder
// Splits an instruction word into register numbers, a constant and control signals.
module cpu_decoder (
    input  wire [31:0] instr,

    // Bit fields, passed on for every instruction
    output wire [3:0]  areg,              // A register
    output wire [3:0]  breg,              // B register: ALU B, branch B, write and push data
    output wire [3:0]  dreg,              // D register: destination
    output wire [3:0]  alu_op,            // ALU or multi-cycle ALU operation
    output wire [2:0]  branch_op,         // branch condition
    output wire        branch_signed,     // the S bit

    // Constants
    output reg  [31:0] constant,          // c16 or c27, extended; 4 for ccache
    output wire        alu_use_const,     // ALU B is the constant instead of breg
    output wire        const_add_pc,      // the constant is PC + constant (savpc, jumpro)
    output wire        jump_absolute,     // the target is the constant, not PC + constant (jump)

    // Registers
    output wire        reads_areg,
    output wire        reads_breg,
    output wire        writes_dreg,
    output wire        late,              // the result exists only at the end of MEM

    // Read and write size, a word if neither byte nor half
    output wire        mem_byte,
    output wire        mem_half,
    output wire        read_sign_extend,  // readb, readh
    output wire        ccache_data_only,  // ccached

    // The instruction type: one-hot, or all 0 for an illegal instruction
    output wire        arith,             // ARITH, ARITHC and SAVPC, the single-cycle ALU
    output wire        mult,              // ARITHM multiply
    output wire        div,               // ARITHM divide and modulo
    output wire        read,
    output wire        write,
    output wire        push,
    output wire        pop,
    output wire        branch,
    output wire        jump,              // jump, jumpo
    output wire        jumpr,             // jumpr, jumpro
    output wire        halt,
    output wire        reti,
    output wire        ccache,            // ccache, ccached
    output wire        intid,
    output wire        illegal
);

// Opcodes
localparam ARITH   = 4'b0000;
localparam ARITHC  = 4'b0001;
localparam ARITHM  = 4'b0010;
localparam ARITHMC = 4'b0011;
localparam RETI    = 4'b0100;
localparam SAVPC   = 4'b0101;
localparam BRANCH  = 4'b0110;
localparam CCACHE  = 4'b0111;
localparam JUMPR   = 4'b1000;
localparam JUMP    = 4'b1001;
localparam POP     = 4'b1010;
localparam PUSH    = 4'b1011;
localparam INTID   = 4'b1100;
localparam WRITE   = 4'b1101;
localparam READ    = 4'b1110;
localparam HALT    = 4'b1111;

// The ALU ops the decoder needs to know: savpc is the ALU's load with the PC
// as constant, and load and loadhi take a zero-extended constant.
localparam ALU_LOAD   = 4'b1100;
localparam ALU_LOADHI = 4'b1101;

wire [3:0]  instr_op      = instr[31:28];  // opcode
wire [3:0]  op            = instr[27:24];  // ALU op of ARITH(M)(C)
wire [3:0]  read_subop    = instr[7:4];    // read size
wire [3:0]  write_subop   = instr[3:0];    // write size
wire        o             = instr[0];      // JUMP and JUMPR: offset from the PC
wire [15:0] const16_arith = instr[23:8];   // ARITHC, ARITHMC
wire [15:0] const16       = instr[27:12];  // BRANCH, JUMPR, READ, WRITE
wire [26:0] const27       = instr[27:1];   // JUMP

wire arith_or_arithc   = instr_op == ARITH  || instr_op == ARITHC;
wire arithm_or_arithmc = instr_op == ARITHM || instr_op == ARITHMC;
wire alu_b_is_breg     = instr_op == ARITH  || instr_op == ARITHM;
wire savpc             = instr_op == SAVPC;

// Reserved encodings
wire alu_op_ok = op != 4'b1000 && op != 4'b1001 && op != 4'b1111;
wire mult_op   = op <= 4'b0010 || op >= 4'b1110;  // mults, multu, multfp, mulshi, multuhi
wire div_op    = op >= 4'b0011 && op <= 4'b0111;  // divs, divu, divfp, mods, modu
wire branch_ok = instr[2:1] != 2'b11;             // branch ops 011 and 111
wire read_ok   = read_subop == 4'b0000 || read_subop == 4'b0001 || read_subop == 4'b0101 ||
                 read_subop == 4'b0010 || read_subop == 4'b0110;
wire write_ok  = write_subop == 4'b0000 || write_subop == 4'b0001 || write_subop == 4'b0010;

assign illegal = (arith_or_arithc && !alu_op_ok) || (arithm_or_arithmc && !mult_op && !div_op) ||
                 (instr_op == BRANCH && !branch_ok) ||
                 (instr_op == READ && !read_ok) || (instr_op == WRITE && !write_ok);

// Type of instruction
assign arith  = (arith_or_arithc && alu_op_ok) || savpc;
assign mult   = arithm_or_arithmc && mult_op;
assign div    = arithm_or_arithmc && div_op;
assign read   = instr_op == READ && read_ok;
assign write  = instr_op == WRITE && write_ok;
assign push   = instr_op == PUSH;
assign pop    = instr_op == POP;
assign branch = instr_op == BRANCH && branch_ok;
assign jump   = instr_op == JUMP;
assign jumpr  = instr_op == JUMPR;
assign halt   = instr_op == HALT;
assign reti   = instr_op == RETI;
assign ccache = instr_op == CCACHE;
assign intid  = instr_op == INTID;

// Bit fields
assign areg          = (instr_op == ARITHC || instr_op == ARITHMC || instr_op == JUMPR) ?
                       instr[7:4] : instr[11:8];
assign breg          = instr[7:4];
assign dreg          = instr[3:0];
assign alu_op        = savpc ? ALU_LOAD : op;
assign branch_op     = instr[3:1];
assign branch_signed = instr[0];

// Registers
wire alu = arith || mult || div;

assign reads_areg  = (alu && !savpc) || read || write || branch || jumpr;
assign reads_breg  = (alu && alu_b_is_breg) || write || branch || push;
assign writes_dreg = (alu || read || pop || intid) && dreg != 4'd0;
assign late        = mult || div || read || pop || intid;

// The constant
assign alu_use_const = alu && !alu_b_is_breg;
assign const_add_pc  = savpc || (jumpr && o);
assign jump_absolute = jump && !o;

always @(*) begin
    if (illegal)
        constant = 32'd0;
    else case (instr_op)
        ARITHC:  constant = (op == ALU_LOAD || op == ALU_LOADHI) ?
                            {16'd0, const16_arith} : {{16{const16_arith[15]}}, const16_arith};
        ARITHMC: constant = {{16{const16_arith[15]}}, const16_arith};
        BRANCH, JUMPR, READ, WRITE:
                 constant = {{16{const16[15]}}, const16};
        JUMP:    constant = o ? {{5{const27[26]}}, const27} : {5'd0, const27};
        CCACHE:  constant = 32'd4;  // the target: the next instruction
        default: constant = 32'd0;
    endcase
end

// Read and write size
assign mem_byte         = (read && read_subop[1:0] == 2'b01) || (write && write_subop[1:0] == 2'b01);
assign mem_half         = (read && read_subop[1:0] == 2'b10) || (write && write_subop[1:0] == 2'b10);
assign read_sign_extend = read && !read_subop[2] && read_subop[1:0] != 2'b00;
assign ccache_data_only = ccache && instr[0];

endmodule
