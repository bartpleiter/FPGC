/*
* L1 Instruction Cache
* Sits between Instrmem and arbiter
* Currently skipped because of issues
*/
module L1Icache(
    // clock/reset inputs
    input               clk,
    input               reset,
    input               cache_reset,

    // CPU bus
    input [31:0]        l2_addr,
    input [31:0]        l2_data,
    input               l2_we,
    input               l2_start,
    output [31:0]       l2_q,
    output              l2_done,

    // SDRAM controller bus
    output [31:0]       sdc_addr,
    output [31:0]       sdc_data,
    output              sdc_we,
    output              sdc_start,
    input [31:0]        sdc_q,
    input               sdc_done
);

// passthrough to skip
assign sdc_addr =  l2_addr;
assign sdc_data = l2_data;
assign sdc_we =   l2_we;
assign sdc_start = l2_start;
assign l2_q =       sdc_q;
assign l2_done =  sdc_done;

endmodule