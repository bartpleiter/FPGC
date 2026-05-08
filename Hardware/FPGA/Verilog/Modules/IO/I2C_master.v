/*
 * I2C_master — Generic I2C master controller
 * --------------------------------------------
 * Supports single-byte register read and write over I2C (compatible with SCCB).
 *
 * Write transaction:
 *   START → [dev_addr + W] → ACK → [reg_addr] → ACK → [wr_data] → ACK → STOP
 *
 * Read transaction:
 *   START → [dev_addr + W] → ACK → [reg_addr] → ACK →
 *   RESTART → [dev_addr + R] → ACK → [rd_data] → NACK → STOP
 *
 * Open-drain I/O:
 *   scl_oe / sda_oe: 1 = pull line LOW, 0 = release (external pull-up)
 *   sda_in: always-sampled SDA pin state (for ACK/data reads)
 *
 * Usage:
 *   1. Set dev_addr, reg_addr, wr_data, rw (0=write, 1=read)
 *   2. Pulse start for 1 cycle
 *   3. Poll busy until 0
 *   4. For reads: rd_data contains the received byte
 *   5. ack_err indicates if any expected ACK was not received
 */
`timescale 1ns / 1ps

module I2C_master #(
    parameter CLK_FREQ = 100_000_000,
    parameter I2C_FREQ = 100_000        // Standard mode 100 kHz
)(
    input  wire        clk,
    input  wire        reset,

    // Transaction interface
    input  wire        start,           // Pulse to begin transaction
    input  wire        rw,              // 0 = write, 1 = read
    input  wire [6:0]  dev_addr,        // 7-bit I2C device address
    input  wire [7:0]  reg_addr,        // Register address byte
    input  wire [7:0]  wr_data,         // Write data byte
    output reg  [7:0]  rd_data = 8'd0,  // Read result (valid after busy→0)
    output reg         busy    = 1'b0,
    output reg         ack_err = 1'b0,  // Sticky: set if slave NAK'd

    // I2C open-drain bus
    output reg         scl_oe  = 1'b0,  // 1 = pull SCL low
    output reg         sda_oe  = 1'b0,  // 1 = pull SDA low
    input  wire        sda_in,          // Sampled SDA pin

    // Debug
    output wire [4:0]  dbg_state_out
);

// ---- Timing ----
localparam QUARTER = CLK_FREQ / (4 * I2C_FREQ);  // 250 @ 100M/100k
localparam HALF    = CLK_FREQ / (2 * I2C_FREQ);  // 500

// ---- Latched transaction parameters ----
reg        rw_lat      = 1'b0;
reg [6:0]  dev_lat     = 7'd0;
reg [7:0]  reg_lat     = 8'd0;
reg [7:0]  wrdata_lat  = 8'd0;

// ---- FSM states ----
localparam [4:0]
    S_IDLE       = 5'd0,
    S_START_SDA  = 5'd1,   // Pull SDA low (SCL still high) = START
    S_START_SCL  = 5'd2,   // Pull SCL low after START
    S_TX_SETUP   = 5'd3,   // SCL low, set SDA data bit
    S_TX_HIGH    = 5'd4,   // SCL high (slave samples)
    S_TX_LOW     = 5'd5,   // SCL low after bit
    S_ACK_SETUP  = 5'd6,   // SCL low, release SDA
    S_ACK_HIGH   = 5'd7,   // SCL high, sample ACK
    S_ACK_DONE   = 5'd8,   // SCL low, decide next phase
    S_RS_RELEASE = 5'd9,   // Repeated START: release SDA
    S_RS_SDA     = 5'd10,  // Repeated START: pull SDA low
    S_RS_SCL     = 5'd11,  // Repeated START: pull SCL low
    S_RX_SETUP   = 5'd12,  // SCL low, release SDA
    S_RX_HIGH    = 5'd13,  // SCL high, sample SDA
    S_RX_LOW     = 5'd14,  // SCL low after bit
    S_NACK_SETUP = 5'd15,  // SCL low, SDA released (=NACK)
    S_NACK_HIGH  = 5'd16,  // SCL high (slave reads NACK)
    S_STOP_SDA   = 5'd17,  // SCL low, pull SDA low
    S_STOP_SCL   = 5'd18,  // SCL high, SDA still low
    S_STOP_REL   = 5'd19,  // SCL high, release SDA = STOP
    S_DONE       = 5'd20,
    S_TIMER      = 5'd21;

reg [4:0] state        = S_IDLE;
reg [4:0] return_state = S_IDLE;

assign dbg_state_out = state;

// ---- Byte/bit tracking ----
reg [7:0]  tx_shift  = 8'd0;   // Shift register for TX (MSB first)
reg [7:0]  rx_shift  = 8'd0;   // Shift register for RX
reg [3:0]  bit_cnt   = 4'd0;   // Bit counter (0-7)
reg [1:0]  phase     = 2'd0;   // 0=dev+W, 1=reg, 2=data(W)/dev+R(R), 3=rx(R)

// ---- Timer ----
reg [31:0] timer     = 32'd0;

always @(posedge clk) begin
    if (reset) begin
        state       <= S_IDLE;
        busy        <= 1'b0;
        ack_err     <= 1'b0;
        scl_oe      <= 1'b0;
        sda_oe      <= 1'b0;
        rd_data     <= 8'd0;
        rw_lat      <= 1'b0;
        dev_lat     <= 7'd0;
        reg_lat     <= 8'd0;
        wrdata_lat  <= 8'd0;
        tx_shift    <= 8'd0;
        rx_shift    <= 8'd0;
        bit_cnt     <= 4'd0;
        phase       <= 2'd0;
        timer       <= 32'd0;
        return_state <= S_IDLE;
    end else begin
        case (state)

        // ============================================================
        // IDLE: wait for start pulse
        // ============================================================
        S_IDLE: begin
            scl_oe <= 1'b0;
            sda_oe <= 1'b0;
            if (start && !busy) begin
                busy       <= 1'b1;
                ack_err    <= 1'b0;
                rw_lat     <= rw;
                dev_lat    <= dev_addr;
                reg_lat    <= reg_addr;
                wrdata_lat <= wr_data;
                phase      <= 2'd0;
                // START: pull SDA low while SCL is high
                sda_oe       <= 1'b1;
                timer        <= QUARTER;
                return_state <= S_START_SCL;
                state        <= S_TIMER;
            end
        end

        // After SDA low, pull SCL low and load TX byte
        S_START_SCL: begin
            scl_oe   <= 1'b1;
            // Phase 0 (initial START): send dev_addr + W
            // Phase 2 (after RESTART): send dev_addr + R
            if (phase == 2'd2)
                tx_shift <= {dev_lat, 1'b1};
            else
                tx_shift <= {dev_lat, 1'b0};
            bit_cnt  <= 4'd0;
            timer        <= QUARTER;
            return_state <= S_TX_SETUP;
            state        <= S_TIMER;
        end

        // ============================================================
        // TX: send one bit (MSB first)
        // ============================================================

        // SCL low: set SDA to current bit
        S_TX_SETUP: begin
            scl_oe <= 1'b1;
            sda_oe <= !tx_shift[7];  // 1=pull low (bit=0), 0=release (bit=1)
            tx_shift <= {tx_shift[6:0], 1'b0};
            timer        <= QUARTER;
            return_state <= S_TX_HIGH;
            state        <= S_TIMER;
        end

        // SCL high: slave samples SDA
        S_TX_HIGH: begin
            scl_oe  <= 1'b0;
            bit_cnt <= bit_cnt + 1'b1;
            timer        <= HALF;
            return_state <= S_TX_LOW;
            state        <= S_TIMER;
        end

        // SCL low: check if byte complete
        S_TX_LOW: begin
            scl_oe <= 1'b1;
            if (bit_cnt == 4'd8) begin
                timer        <= QUARTER;
                return_state <= S_ACK_SETUP;
            end else begin
                timer        <= QUARTER;
                return_state <= S_TX_SETUP;
            end
            state <= S_TIMER;
        end

        // ============================================================
        // ACK: release SDA, clock SCL, sample slave ACK
        // ============================================================

        // SCL low: release SDA for slave to drive
        S_ACK_SETUP: begin
            scl_oe <= 1'b1;
            sda_oe <= 1'b0;
            timer        <= QUARTER;
            return_state <= S_ACK_HIGH;
            state        <= S_TIMER;
        end

        // SCL high: sample SDA (ACK=0, NAK=1)
        S_ACK_HIGH: begin
            scl_oe <= 1'b0;
            if (sda_in)
                ack_err <= 1'b1;
            timer        <= HALF;
            return_state <= S_ACK_DONE;
            state        <= S_TIMER;
        end

        // SCL low: decide what comes next
        S_ACK_DONE: begin
            scl_oe <= 1'b1;
            case (phase)
            2'd0: begin
                // Sent dev+W → send reg_addr
                phase    <= 2'd1;
                tx_shift <= reg_lat;
                bit_cnt  <= 4'd0;
                timer        <= QUARTER;
                return_state <= S_TX_SETUP;
            end
            2'd1: begin
                if (rw_lat) begin
                    // Read mode: need repeated START → dev+R
                    phase <= 2'd2;
                    timer        <= QUARTER;
                    return_state <= S_RS_RELEASE;
                end else begin
                    // Write mode: send data byte
                    phase    <= 2'd2;
                    tx_shift <= wrdata_lat;
                    bit_cnt  <= 4'd0;
                    timer        <= QUARTER;
                    return_state <= S_TX_SETUP;
                end
            end
            2'd2: begin
                if (rw_lat) begin
                    // Sent dev+R, ACK'd → receive data byte
                    bit_cnt  <= 4'd0;
                    rx_shift <= 8'd0;
                    timer        <= QUARTER;
                    return_state <= S_RX_SETUP;
                end else begin
                    // Write data sent and ACK'd → STOP
                    timer        <= QUARTER;
                    return_state <= S_STOP_SDA;
                end
            end
            default: begin
                timer        <= QUARTER;
                return_state <= S_STOP_SDA;
            end
            endcase
            state <= S_TIMER;
        end

        // ============================================================
        // REPEATED START (for read mode)
        // ============================================================

        // Release SDA (while SCL is low)
        S_RS_RELEASE: begin
            scl_oe <= 1'b1;  // SCL low
            sda_oe <= 1'b0;  // Release SDA → goes high
            timer        <= QUARTER;
            return_state <= S_RS_SDA;
            state        <= S_TIMER;
        end

        // SCL goes high, then pull SDA low = repeated START
        S_RS_SDA: begin
            scl_oe <= 1'b0;  // SCL high
            // Wait for SCL setup, then pull SDA low
            timer        <= QUARTER;
            return_state <= S_RS_SCL;
            state        <= S_TIMER;
        end

        // Pull SDA low while SCL high = START condition
        S_RS_SCL: begin
            sda_oe <= 1'b1;  // SDA low = START
            timer        <= QUARTER;
            return_state <= S_START_SCL;  // Reuse: pull SCL low, load byte
            // But we need to load dev+R this time, not dev+W
            // Override in S_START_SCL won't work because it always loads dev+W.
            // Solution: load tx_shift here and skip S_START_SCL's load
            state <= S_TIMER;
        end

        // ============================================================
        // RX: receive one byte from slave (MSB first)
        // ============================================================

        // SCL low: release SDA for slave
        S_RX_SETUP: begin
            scl_oe <= 1'b1;
            sda_oe <= 1'b0;
            timer        <= QUARTER;
            return_state <= S_RX_HIGH;
            state        <= S_TIMER;
        end

        // SCL high: sample SDA
        S_RX_HIGH: begin
            scl_oe   <= 1'b0;
            rx_shift <= {rx_shift[6:0], sda_in};
            bit_cnt  <= bit_cnt + 1'b1;
            timer        <= HALF;
            return_state <= S_RX_LOW;
            state        <= S_TIMER;
        end

        // SCL low: check if byte complete
        S_RX_LOW: begin
            scl_oe <= 1'b1;
            if (bit_cnt == 4'd8) begin
                rd_data <= rx_shift;
                timer        <= QUARTER;
                return_state <= S_NACK_SETUP;
            end else begin
                timer        <= QUARTER;
                return_state <= S_RX_SETUP;
            end
            state <= S_TIMER;
        end

        // ============================================================
        // NACK: master doesn't ACK (SDA high during ACK slot)
        // ============================================================

        // SCL low: SDA released = NACK
        S_NACK_SETUP: begin
            scl_oe <= 1'b1;
            sda_oe <= 1'b0;  // SDA high = NACK
            timer        <= QUARTER;
            return_state <= S_NACK_HIGH;
            state        <= S_TIMER;
        end

        // SCL high: slave sees NACK
        S_NACK_HIGH: begin
            scl_oe <= 1'b0;
            timer        <= HALF;
            return_state <= S_STOP_SDA;
            state        <= S_TIMER;
        end

        // ============================================================
        // STOP sequence
        // ============================================================

        // SCL low, pull SDA low
        S_STOP_SDA: begin
            scl_oe <= 1'b1;
            sda_oe <= 1'b1;
            timer        <= QUARTER;
            return_state <= S_STOP_SCL;
            state        <= S_TIMER;
        end

        // SCL high, SDA still low
        S_STOP_SCL: begin
            scl_oe <= 1'b0;
            sda_oe <= 1'b1;
            timer        <= QUARTER;
            return_state <= S_STOP_REL;
            state        <= S_TIMER;
        end

        // SCL high, release SDA → SDA rises = STOP
        S_STOP_REL: begin
            scl_oe <= 1'b0;
            sda_oe <= 1'b0;
            timer        <= HALF;
            return_state <= S_DONE;
            state        <= S_TIMER;
        end

        // ============================================================
        S_DONE: begin
            busy  <= 1'b0;
            state <= S_IDLE;
        end

        // ============================================================
        // Timer subroutine
        S_TIMER: begin
            if (timer == 32'd0)
                state <= return_state;
            else
                timer <= timer - 1'b1;
        end

        default: state <= S_IDLE;
        endcase
    end
end

endmodule
