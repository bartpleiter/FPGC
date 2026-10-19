// Testbench of cpu_decoder. See tb_unit.vh for how the vectors are applied.
`timescale 1ns / 1ps
module tb_cpu_decoder;

`include "tb_unit.vh"

reg  [31:0] instr;
wire [3:0]  areg;
wire [3:0]  breg;
wire [3:0]  dreg;
wire [3:0]  alu_op;
wire [2:0]  branch_op;
wire        branch_signed;
wire [31:0] constant;
wire        alu_use_const;
wire        const_add_pc;
wire        jump_absolute;
wire        reads_areg;
wire        reads_breg;
wire        writes_dreg;
wire        late;
wire        mem_byte;
wire        mem_half;
wire        read_sign_extend;
wire        ccache_data_only;
wire        arith;
wire        mult;
wire        div;
wire        read;
wire        write;
wire        push;
wire        pop;
wire        branch;
wire        jump;
wire        jumpr;
wire        halt;
wire        reti;
wire        ccache;
wire        intid;
wire        illegal;

cpu_decoder dut (
    .instr(instr),
    .areg(areg),
    .breg(breg),
    .dreg(dreg),
    .alu_op(alu_op),
    .branch_op(branch_op),
    .branch_signed(branch_signed),
    .constant(constant),
    .alu_use_const(alu_use_const),
    .const_add_pc(const_add_pc),
    .jump_absolute(jump_absolute),
    .reads_areg(reads_areg),
    .reads_breg(reads_breg),
    .writes_dreg(writes_dreg),
    .late(late),
    .mem_byte(mem_byte),
    .mem_half(mem_half),
    .read_sign_extend(read_sign_extend),
    .ccache_data_only(ccache_data_only),
    .arith(arith),
    .mult(mult),
    .div(div),
    .read(read),
    .write(write),
    .push(push),
    .pop(pop),
    .branch(branch),
    .jump(jump),
    .jumpr(jumpr),
    .halt(halt),
    .reti(reti),
    .ccache(ccache),
    .intid(intid),
    .illegal(illegal)
);

initial begin
    tb_open;
    while ($fscanf(vectors_fd, "%h %h\n", label, instr) == 2)
        @(negedge clk) $fwrite(outputs_fd, "%h %h %h %h %h %h %h %h %h %h %h %h %h %h %h %h %h %h %h %h %h %h %h %h %h %h %h %h %h %h %h %h %h\n",
                 areg, breg, dreg, alu_op, branch_op, branch_signed,
                 constant, alu_use_const, const_add_pc, jump_absolute, reads_areg, reads_breg,
                 writes_dreg, late, mem_byte, mem_half, read_sign_extend, ccache_data_only,
                 arith, mult, div, read, write, push,
                 pop, branch, jump, jumpr, halt, reti,
                 ccache, intid, illegal);
    tb_close;
end

endmodule
