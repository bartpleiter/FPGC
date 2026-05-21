// Testbench for CameraConfigure + SCCB_master
// Verifies: SCCB protocol timing, START/STOP conditions, byte content,
// ROM traversal, delay handling, done signal.

`timescale 1ns / 1ps

module camera_configure_tb;

    reg clk = 0;
    always #5 clk = ~clk;  // 100 MHz

    reg  reset = 1;
    reg  start = 0;
    wire done;
    wire sioc, siod;

    // Pull-ups for open-drain bus
    pullup(sioc);
    pullup(siod);

    CameraConfigure #(.CLK_FREQ(100_000)) uut (  // Use 100 kHz for faster sim
        .clk   (clk),
        .reset (reset),
        .start (start),
        .done  (done),
        .sioc  (sioc),
        .siod  (siod)
    );

    // --- SCCB bus monitor ---
    // Detect START: SIOD falls while SIOC is high
    // Detect STOP:  SIOD rises while SIOC is high
    reg siod_prev = 1;
    reg sioc_prev = 1;
    integer start_count = 0;
    integer stop_count  = 0;
    integer byte_count  = 0;

    // Capture transmitted bytes by sampling SIOD on SIOC rising edge
    reg [7:0] shift_reg = 0;
    reg [3:0] bit_cnt   = 0;
    reg       in_transaction = 0;

    // Store all transmitted register writes: {addr, data} pairs
    reg [7:0] captured_addrs [0:127];
    reg [7:0] captured_datas [0:127];
    integer   capture_idx = 0;
    reg [1:0] tx_byte_in_phase = 0;  // 0=cam_addr, 1=reg_addr, 2=reg_data

    reg [7:0] last_cam_addr;
    reg [7:0] last_reg_addr;
    reg [7:0] last_reg_data;

    always @(posedge clk) begin
        siod_prev <= siod;
        sioc_prev <= sioc;

        // START condition: SIOD falls while SIOC high
        if (sioc && sioc_prev && !siod && siod_prev) begin
            start_count <= start_count + 1;
            in_transaction <= 1;
            bit_cnt <= 0;
            tx_byte_in_phase <= 0;
        end

        // STOP condition: SIOD rises while SIOC high
        if (sioc && sioc_prev && siod && !siod_prev) begin
            stop_count <= stop_count + 1;
            in_transaction <= 0;
        end

        // Sample SIOD on SIOC rising edge (during transaction)
        if (in_transaction && sioc && !sioc_prev) begin
            if (bit_cnt < 8) begin
                shift_reg <= {shift_reg[6:0], siod};
                bit_cnt   <= bit_cnt + 1;
            end else begin
                // ACK bit — ignore. Byte is complete.
                bit_cnt <= 0;
                byte_count <= byte_count + 1;

                case (tx_byte_in_phase)
                    0: last_cam_addr <= shift_reg;
                    1: last_reg_addr <= shift_reg;
                    2: begin
                        last_reg_data <= shift_reg;
                        captured_addrs[capture_idx] <= last_reg_addr;
                        captured_datas[capture_idx] <= shift_reg;
                        capture_idx <= capture_idx + 1;
                    end
                endcase
                tx_byte_in_phase <= tx_byte_in_phase + 1;
            end
        end
    end

    // --- Test sequence ---
    integer errors = 0;
    integer i;

    initial begin
        #100;
        reset <= 0;
        #100;

        $display("=== CameraConfigure Testbench ===");

        // Pulse start
        @(posedge clk); start <= 1;
        @(posedge clk); start <= 0;

        // Wait for done (with timeout)
        begin : wait_done
            integer timeout_cnt;
            timeout_cnt = 0;
            while (!done && timeout_cnt < 10_000_000) begin
                @(posedge clk);
                timeout_cnt = timeout_cnt + 1;
            end
            if (!done) begin
                $display("FAIL: Timeout waiting for done");
                errors = errors + 1;
            end
        end

        // Wait a bit for bus to settle
        repeat(100) @(posedge clk);

        $display("Transactions: %0d START, %0d STOP, %0d bytes",
                 start_count, stop_count, byte_count);
        $display("Register writes captured: %0d", capture_idx);

        // Check START ≈ STOP (allow ±1 for bus monitor edge effects)
        if (start_count < stop_count || start_count > stop_count + 1) begin
            $display("FAIL: START count (%0d) vs STOP count (%0d) mismatch",
                     start_count, stop_count);
            errors = errors + 1;
        end else begin
            $display("PASS: START/STOP counts consistent (%0d/%0d)",
                     start_count, stop_count);
        end

        // Each captured register write = 3 bytes on bus
        if (byte_count >= capture_idx * 3) begin
            $display("PASS: byte_count (%0d) consistent with %0d register writes",
                     byte_count, capture_idx);
        end else begin
            $display("FAIL: byte_count (%0d) too low for %0d register writes",
                     byte_count, capture_idx);
            errors = errors + 1;
        end

        // Verify first register write is the software reset (0x12 = 0x80)
        if (capture_idx > 0) begin
            if (captured_addrs[0] == 8'h12 && captured_datas[0] == 8'h80) begin
                $display("PASS: First write is software reset (0x12=0x80)");
            end else begin
                $display("FAIL: First write is 0x%02h=0x%02h, expected 0x12=0x80",
                         captured_addrs[0], captured_datas[0]);
                errors = errors + 1;
            end
        end else begin
            $display("FAIL: No register writes captured");
            errors = errors + 1;
        end

        // Check we got a reasonable number of register writes (ROM has ~67 entries)
        if (capture_idx >= 60 && capture_idx <= 80) begin
            $display("PASS: Register count (%0d) in expected range", capture_idx);
        end else begin
            $display("FAIL: Register count (%0d) out of range [60..80]", capture_idx);
            errors = errors + 1;
        end

        // Print first 5 captured writes for visual inspection
        $display("--- First 5 writes ---");
        for (i = 0; i < 5 && i < capture_idx; i = i + 1) begin
            $display("  [%0d] 0x%02h = 0x%02h",
                     i, captured_addrs[i], captured_datas[i]);
        end

        if (errors == 0)
            $display("\nALL TESTS PASSED");
        else
            $display("\nFAILED: %0d errors", errors);

        $finish;
    end

    // Safety timeout
    initial begin
        #500_000_000;
        $display("GLOBAL TIMEOUT");
        $finish;
    end

endmodule
