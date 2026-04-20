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
 *  - The arbiter LATCHES a one-cycle `start` pulse that arrives while the
 *    bus is busy (`busy==1`) or while it loses fair-share arbitration to
 *    the other master, along with the requester's `addr`/`data`/`we`. The
 *    pending request is then granted on a future cycle once the bus frees
 *    up. This keeps the requester-side protocol simple (single-pulse
 *    fire-and-wait) and was added to fix a deadlock where the CPU cache
 *    controller's one-cycle `cpu_start` was silently dropped if it arrived
 *    while the DMA held the bus.
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

// Pending-request latches: capture a one-cycle start pulse that wasn't
// granted immediately so it can be granted on a future cycle without the
// requester having to keep its inputs asserted.
reg          cpu_pend_valid = 1'b0;
reg [20:0]   cpu_pend_addr  = 21'd0;
reg [255:0]  cpu_pend_data  = 256'd0;
reg          cpu_pend_we    = 1'b0;
reg          dma_pend_valid = 1'b0;
reg [20:0]   dma_pend_addr  = 21'd0;
reg [255:0]  dma_pend_data  = 256'd0;
reg          dma_pend_we    = 1'b0;

// Effective request signals: either a fresh pulse or a pending latched
// request still waiting for grant.
wire cpu_req = cpu_start | cpu_pend_valid;
wire dma_req = dma_start | dma_pend_valid;

wire dma_pref  = last_cpu && dma_req;
wire grant_cpu = !busy && cpu_req && !dma_pref;
wire grant_dma = !busy && dma_req && (!cpu_req || dma_pref);

// Effective payload: latched if pending, otherwise the live input.
wire [20:0]  eff_cpu_addr = cpu_pend_valid ? cpu_pend_addr : cpu_addr;
wire [255:0] eff_cpu_data = cpu_pend_valid ? cpu_pend_data : cpu_data;
wire         eff_cpu_we   = cpu_pend_valid ? cpu_pend_we   : cpu_we;
wire [20:0]  eff_dma_addr = dma_pend_valid ? dma_pend_addr : dma_addr;
wire [255:0] eff_dma_data = dma_pend_valid ? dma_pend_data : dma_data;
wire         eff_dma_we   = dma_pend_valid ? dma_pend_we   : dma_we;

always @(posedge clk)
begin
    if (reset)
    begin
        busy           <= 1'b0;
        owner          <= 1'b0;
        last_cpu       <= 1'b0;
        cpu_pend_valid <= 1'b0;
        dma_pend_valid <= 1'b0;
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
                busy           <= 1'b1;
                owner          <= 1'b0;
                last_cpu       <= 1'b1;
                cpu_pend_valid <= 1'b0; // grant satisfies any pending request
            end
            else if (grant_dma)
            begin
                busy           <= 1'b1;
                owner          <= 1'b1;
                last_cpu       <= 1'b0;
                dma_pend_valid <= 1'b0;
            end
        end

        // Capture a fresh start pulse that did not get granted this cycle.
        // (If the same cycle's start IS granted, the grant_* clauses above
        // already cleared *_pend_valid and we don't need to latch anything.)
        if (cpu_start && !grant_cpu)
        begin
            cpu_pend_valid <= 1'b1;
            cpu_pend_addr  <= cpu_addr;
            cpu_pend_data  <= cpu_data;
            cpu_pend_we    <= cpu_we;
        end
        if (dma_start && !grant_dma)
        begin
            dma_pend_valid <= 1'b1;
            dma_pend_addr  <= dma_addr;
            dma_pend_data  <= dma_data;
            dma_pend_we    <= dma_we;
        end
    end
end

// Effective owner for routing: while busy use latched owner, otherwise use
// the (potential) grant decision so the start-cycle payload is correct.
wire route_dma = busy ? owner : grant_dma;

assign sdc_addr  = route_dma ? eff_dma_addr : eff_cpu_addr;
assign sdc_data  = route_dma ? eff_dma_data : eff_cpu_data;
assign sdc_we    = route_dma ? eff_dma_we   : eff_cpu_we;
assign sdc_start = grant_cpu | grant_dma;

assign cpu_done = busy && !owner && sdc_done;
assign dma_done = busy &&  owner && sdc_done;

assign cpu_q = sdc_q;
assign dma_q = sdc_q;

endmodule
