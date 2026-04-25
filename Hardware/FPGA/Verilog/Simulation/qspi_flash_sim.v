/*
 * qspi_flash_sim
 * --------------
 * Minimal slave-side simulation model of a Quad-SPI flash chip, just enough
 * to validate QSPIflash.v's Fast Read (opcode 0xEB) state machine.
 *
 * Protocol (matches QSPIflash.v / W25Q128JV "Fast Read Quad I/O"):
 *   1. CS falling edge resets the model to "expect opcode".
 *   2. 8 SCK cycles: master shifts out 0xEB on IO0 (1 bit/cycle, MSB first).
 *      Any other opcode is silently ignored (we never drive IO[3:0]).
 *   3. 6 SCK cycles: master drives 24-bit address on IO[3:0] (4 bits/cycle,
 *      MSB nibble first; bit i within the nibble maps to IOi).
 *   4. 2 SCK cycles: master drives M[7:0] on IO[3:0] (we don't latch
 *      continuous mode -- this sim model accepts a fresh opcode every
 *      burst regardless of M[7:4]).
 *   5. 4 SCK dummy cycles: bus tristated by master.
 *   6. Data phase (until CS rises): on every SCK falling edge we drive
 *      the next nibble onto IO[3:0]. The data pattern is deterministic:
 *
 *          data[byte_idx] = (start_addr + byte_idx) & 0xFF
 *
 *      so the testbench can verify SDRAM contents by computing the
 *      expected ramp from the starting QSPI byte address.
 *
 * The model is purely combinational w.r.t. the IO drivers (gated by
 * `drive`), and clocks all state on spi_clk edges. cs_n is async-reset.
 *
 * Pin connections (from the QSPIflash side):
 *   IO0 = MOSI  (master drives during opcode/addr/mode)
 *   IO1 = MISO  (master drives during addr/mode; this model drives during data)
 *   IO2 = WP_n  (master drives during addr/mode; this model drives during data)
 *   IO3 = HOLD_n (master drives during addr/mode; this model drives during data)
 *
 * The testbench is responsible for the actual tristate wiring -- this
 * module just produces an io_out[3:0] / io_oe[3:0] pair which the parent
 * combines with the master's wires.
 */
module qspi_flash_sim (
    input  wire        cs_n,
    input  wire        spi_clk,

    // Inputs from the master (sampled on SCK rising edge)
    input  wire [3:0]  io_in,

    // Outputs from this slave (driven on SCK falling edge during data phase)
    output reg  [3:0]  io_out = 4'd0,
    output wire [3:0]  io_oe              // single bit replicated x4
);

localparam ST_OPCODE = 3'd0;
localparam ST_ADDR   = 3'd1;
localparam ST_MODE   = 3'd2;
localparam ST_DUMMY  = 3'd3;
localparam ST_DATA   = 3'd4;

reg  [2:0]  state         = ST_OPCODE;
reg  [3:0]  cycle         = 4'd0;
reg  [7:0]  opcode_buf    = 8'd0;
reg  [23:0] addr_buf      = 24'd0;
reg  [23:0] cur_addr      = 24'd0;
reg         byte_hi_phase = 1'b1;   // 1 = next negedge drives high nibble
reg         opcode_ok     = 1'b0;   // becomes 1 when opcode_buf matches 0xEB

// Drive IO[3:0] only during the data phase, after a valid opcode was seen.
wire drive = !cs_n && (state == ST_DATA) && opcode_ok;
assign io_oe = {4{drive}};

// ---- SCK rising edge: sample what the master is sending ----
always @(posedge spi_clk or posedge cs_n)
begin
    if (cs_n)
    begin
        state         <= ST_OPCODE;
        cycle         <= 4'd0;
        opcode_buf    <= 8'd0;
        addr_buf      <= 24'd0;
        opcode_ok     <= 1'b0;
    end
    else
    begin
        case (state)
            ST_OPCODE: begin
                opcode_buf <= {opcode_buf[6:0], io_in[0]};
                if (cycle == 4'd7)
                begin
                    cycle     <= 4'd0;
                    state     <= ST_ADDR;
                    opcode_ok <= ({opcode_buf[6:0], io_in[0]} == 8'hEB);
                end
                else
                    cycle <= cycle + 1'b1;
            end

            ST_ADDR: begin
                addr_buf <= {addr_buf[19:0], io_in[3], io_in[2], io_in[1], io_in[0]};
                if (cycle == 4'd5)
                begin
                    cycle <= 4'd0;
                    state <= ST_MODE;
                end
                else
                    cycle <= cycle + 1'b1;
            end

            ST_MODE: begin
                // We don't latch M[7:4] (no continuous-mode support in
                // this sim model -- testbench is expected to toggle CS
                // between bursts). After the second mode cycle, freeze
                // the address and head into the dummy phase.
                if (cycle == 4'd1)
                begin
                    cycle    <= 4'd0;
                    state    <= ST_DUMMY;
                    cur_addr <= addr_buf;
                end
                else
                    cycle <= cycle + 1'b1;
            end

            ST_DUMMY: begin
                if (cycle == 4'd3)
                begin
                    cycle <= 4'd0;
                    state <= ST_DATA;
                end
                else
                    cycle <= cycle + 1'b1;
            end

            ST_DATA: begin
                // Nothing to do on rising edges -- cur_addr is advanced
                // in the negedge block after driving the LOW nibble of
                // each byte (otherwise the increment would race the next
                // negedge and we'd send byte_N high + byte_N+1 low).
            end

            default: state <= ST_OPCODE;
        endcase
    end
end

// ---- SCK falling edge: drive the next data nibble ----
always @(negedge spi_clk or posedge cs_n)
begin
    if (cs_n)
    begin
        io_out        <= 4'd0;
        byte_hi_phase <= 1'b1;
    end
    else if (state == ST_DATA && opcode_ok)
    begin
        // Pattern: data[i] = (start_addr + i) & 0xFF
        // High nibble first, low nibble next; advance cur_addr only
        // after the LOW nibble is on the wire.
        if (byte_hi_phase)
        begin
            io_out        <= cur_addr[7:4];
            byte_hi_phase <= 1'b0;
        end
        else
        begin
            io_out        <= cur_addr[3:0];
            byte_hi_phase <= 1'b1;
            cur_addr      <= cur_addr + 24'd1;
        end
    end
end

endmodule
