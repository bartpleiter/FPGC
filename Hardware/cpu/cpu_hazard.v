// Hazard unit
// Turns the busy signals of the stages and the redirect into a stall and a flush for
// every pipeline register.
module cpu_hazard (
    // A stage is busy when its instruction can't move on yet. Each stage gates its
    // busy signal with its own valid bit.
    input  wire busy_if,
    input  wire busy_id,
    input  wire busy_ex,
    input  wire busy_mem,
    input  wire l1d_miss,   // MEM waits for the L1D. It comes late, from a tag compare.
    input  wire l1d_wait,   // the part of l1d_miss that is known from registers
    input  wire redirect,   // from MEM, flushes the instructions in IF, ID and EX

    // A stalled register keeps its value, and a flushed register gets valid 0.
    output wire stall_if,   // the PC
    output wire stall_id,   // IF/ID
    output wire flush_id,
    output wire stall_ex,   // ID/EX
    output wire flush_ex,
    output wire stall_mem,  // EX/MEM
    output wire flush_mem,
    output wire flush_wb,   // MEM/WB, which never stalls
    output wire refetch_if  // IF fetches its instruction again
);

// A stage stalls when it, or any stage after it, is busy. The L1D miss is MEM's too.
// It's the last term everywhere, so the logic in front of it is done when it comes.
//
// IF only stalls for the part of the miss that is known from registers. The rest comes
// from the tag compare in the first cycle of a miss, too late for IF, which is far from
// MEM and holds its fetch memories with their clock enables. See timing-closure.md.
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
// happens in the first cycle of an L1D miss, and IF then fetches that instruction again
// while MEM waits.
assign refetch_if = stall_id && !stall_if;

endmodule
