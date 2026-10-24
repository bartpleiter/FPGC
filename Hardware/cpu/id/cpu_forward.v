// Forwarding unit
// Decides for each register the instruction in ID reads where its value will come from
// once the instruction is in EX, and whether the instruction has to wait in ID.
//
// When ID moves into EX, EX and MEM move too. So the instruction now in EX will be in
// MEM, and the instruction now in MEM will be in WB. If both write the register, the
// younger one, in EX, wins. A late result exists only at the end of MEM, too late to
// forward from MEM, so then the instruction waits one cycle in ID (the late-result stall).
module cpu_forward (
    // The instruction in ID
    input  wire [3:0] areg,
    input  wire [3:0] breg,
    input  wire       reads_areg,
    input  wire       reads_breg,

    // The instructions in EX and MEM, with writes_dreg 0 if there is none
    input  wire       ex_writes_dreg,
    input  wire [3:0] ex_dreg,
    input  wire       ex_late,
    input  wire       mem_writes_dreg,
    input  wire [3:0] mem_dreg,

    // Take the value in EX from the result in MEM or the value in WB, instead of the
    // value read from the register file
    output wire       areg_from_mem,
    output wire       areg_from_wb,
    output wire       breg_from_mem,
    output wire       breg_from_wb,
    output wire       late_result_stall // wait in ID, because a value isn't there in time
);

wire areg_in_ex  = reads_areg && ex_writes_dreg  && ex_dreg  == areg;
wire areg_in_mem = reads_areg && mem_writes_dreg && mem_dreg == areg;
wire breg_in_ex  = reads_breg && ex_writes_dreg  && ex_dreg  == breg;
wire breg_in_mem = reads_breg && mem_writes_dreg && mem_dreg == breg;

assign areg_from_mem = areg_in_ex;
assign areg_from_wb  = areg_in_mem && !areg_in_ex;
assign breg_from_mem = breg_in_ex;
assign breg_from_wb  = breg_in_mem && !breg_in_ex;

assign late_result_stall = ex_late && (areg_in_ex || breg_in_ex);

endmodule
