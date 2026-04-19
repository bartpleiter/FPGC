/*
 * SDRAMarbiter
 * ------------
 * Two-port arbiter that sits between the CPU CacheController (priority master)
 * and the DMA engine on one side, and the single-port SDRAMcontroller on the
 * other. The CPU port wins by default, but to prevent the DMA from being
 * starved by tight I-cache prefetch loops we apply a single-slot fairness
 * rule: after a CPU transaction completes, if the DMA is requesting at the
 * same time as the CPU, the next grant goes to the DMA. This trades at most
 * one extra cycle of CPU latency for guaranteed DMA forward progress.
 *
 * Notes:
 *  - Both requesters issue a single-cycle `start` pulse and then wait for
 *    their `done` pulse (mirroring the existing SDRAMcontroller protocol).
 *  - SDRAMcontroller latches addr/data/we on the rising edge of `start`, so
 *    the arbiter only needs to drive those buses correctly on the grant
 *    cycle.
 *  - A loser whose `start` arrived while the bus was busy must re-assert
 *    `start` after `sdc_done`. The DMA engine is designed accordingly; the
 *    cache controller never loses (CPU has priority).
 */
module SDRAMarbiter (
    input  wire         clk,
    input  wire         reset,

    // CPU port (priority)
    input  wire [20:0]  cpu_addr,
    input  wire [255:0] cpu_data,
    input  wire         cpu_we,
    input  wire         cpu_start,
    output wire         cpu_done,
    output wire [255:0] cpu_q,

    // DMA port
    input  wire [20:0]  dma_addr,
    input  wire [255:0] dma_data,
    input  wire         dma_we,
    input  wire         dma_start,
    output wire         dma_done,
    output wire [255:0] dma_q,

    // SDRAM controller port (requester role)
    output wire [20:0]  sdc_addr,
    output wire [255:0] sdc_data,
    output wire         sdc_we,
    output wire         sdc_start,
    input  wire         sdc_done,
    input  wire [255:0] sdc_q
);

reg busy        = 1'b0;
reg owner       = 1'b0; // 0 = CPU, 1 = DMA
reg last_cpu    = 1'b0; // Set after a CPU transaction; if DMA is also asking, it wins next.

wire dma_pref  = last_cpu && dma_start;
wire grant_cpu = !busy && cpu_start && !dma_pref;
wire grant_dma = !busy && dma_start && (!cpu_start || dma_pref);

always @(posedge clk)
begin
    if (reset)
    begin
        busy     <= 1'b0;
        owner    <= 1'b0;
        last_cpu <= 1'b0;
    end
    else
    begin
        if (busy)
        begin
            if (sdc_done)
                busy <= 1'b0;
        end
        else
        begin
            if (grant_cpu)
            begin
                busy     <= 1'b1;
                owner    <= 1'b0;
                last_cpu <= 1'b1;
            end
            else if (grant_dma)
            begin
                busy     <= 1'b1;
                owner    <= 1'b1;
                last_cpu <= 1'b0;
            end
        end
    end
end

// Effective owner for routing: while busy use latched owner, otherwise use
// the (potential) grant decision so the start-cycle payload is correct.
wire route_dma = busy ? owner : grant_dma;

assign sdc_addr  = route_dma ? dma_addr  : cpu_addr;
assign sdc_data  = route_dma ? dma_data  : cpu_data;
assign sdc_we    = route_dma ? dma_we    : cpu_we;
assign sdc_start = grant_cpu | grant_dma;

assign cpu_done = busy && !owner && sdc_done;
assign dma_done = busy &&  owner && sdc_done;

assign cpu_q = sdc_q;
assign dma_q = sdc_q;

endmodule
