// Hazard unit
module cpu_hazard (
    input  wire busy_if,
    input  wire busy_id,
    input  wire busy_ex,
    input  wire busy_mem,
    input  wire l1d_miss,
    input  wire l1d_wait,
    input  wire redirect,

    output wire stall_if,
    output wire stall_id,
    output wire flush_id,
    output wire stall_ex,
    output wire flush_ex,
    output wire stall_mem,
    output wire flush_mem,
    output wire flush_wb,
    output wire refetch_if
);

// A stage stalls when it, or any stage after it, is busy.
assign stall_mem = busy_mem || l1d_miss;
assign stall_ex  = busy_mem || busy_ex || l1d_miss;
assign stall_id  = busy_mem || busy_ex || busy_id || l1d_miss;
assign stall_if  = busy_mem || busy_ex || busy_id || busy_if || l1d_wait;

// A redirect flushes the instructions behind it. A stage that moves on while the stage
// before it stalls is flushed too, so it gets a bubble instead of a copy.
assign flush_id  = redirect || (stall_if && !stall_id);
assign flush_ex  = redirect || (stall_id && !stall_ex);
assign flush_mem = redirect || (stall_ex && !stall_mem);
assign flush_wb  = stall_mem;

// When ID stalls but IF moves on, the instruction that was in IF is lost. That only
// happens in the first cycle of an L1D miss, and IF then fetches that instruction again.
assign refetch_if = stall_id && !stall_if;

endmodule
