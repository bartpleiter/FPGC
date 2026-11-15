// Forwarding unit
module cpu_forward (
    // The instruction in ID
    input  wire [3:0] areg,
    input  wire [3:0] breg,
    input  wire       reads_areg,
    input  wire       reads_breg,

    // The instructions in EX and MEM
    input  wire       ex_writes_dreg,
    input  wire [3:0] ex_dreg,
    input  wire       ex_late,
    input  wire       mem_writes_dreg,
    input  wire [3:0] mem_dreg,

    // Forwarding outputs
    output wire       areg_from_mem,
    output wire       areg_from_wb,
    output wire       breg_from_mem,
    output wire       breg_from_wb,
    output wire       late_result_stall
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
