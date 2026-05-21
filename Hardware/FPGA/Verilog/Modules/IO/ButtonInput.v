/*
 * ButtonInput
 * General-purpose debounced button reader with change-detect interrupt.
 *
 * - Up to 32 active-low GPIO button inputs
 * - 2-stage synchronizer per button (metastability protection)
 * - Counter-based debouncer per button (~20 ms default window)
 * - btn_state[31:0]: bit i = 1 when button i is pressed (inverted from pin)
 * - btn_changed: single-cycle pulse when any debounced state changes
 */
module ButtonInput #(
    parameter NUM_BUTTONS = 8,
    parameter CLK_FREQ    = 100_000_000,
    parameter DEBOUNCE_MS = 20
)(
    input  wire                    clk,
    input  wire                    reset,
    input  wire [NUM_BUTTONS-1:0]  btn_pins,    // Raw GPIO inputs (active-low)
    output wire [31:0]             btn_state,   // Debounced state (active-high)
    output reg                     btn_changed = 1'b0  // IRQ pulse on any change
);

    localparam DEBOUNCE_COUNT = (CLK_FREQ / 1000) * DEBOUNCE_MS;
    // Bit width needed for the counter
    localparam CNT_BITS = $clog2(DEBOUNCE_COUNT + 1);

    // 2-stage synchronizer
    reg [NUM_BUTTONS-1:0] sync_s1 = {NUM_BUTTONS{1'b1}};
    reg [NUM_BUTTONS-1:0] sync_s2 = {NUM_BUTTONS{1'b1}};

    // Debounced state (active-low, matches pin polarity)
    reg [NUM_BUTTONS-1:0] debounced = {NUM_BUTTONS{1'b1}};
    reg [NUM_BUTTONS-1:0] debounced_prev = {NUM_BUTTONS{1'b1}};

    // Per-button counter
    reg [CNT_BITS-1:0] counter [0:NUM_BUTTONS-1];

    // Output: invert debounced (active-low pins → active-high state)
    // Pad unused upper bits to 0
    generate
        if (NUM_BUTTONS < 32) begin : g_pad
            assign btn_state = {{(32-NUM_BUTTONS){1'b0}}, ~debounced};
        end else begin : g_full
            assign btn_state = ~debounced;
        end
    endgenerate

    integer i;

    always @(posedge clk) begin
        if (reset) begin
            sync_s1       <= {NUM_BUTTONS{1'b1}};
            sync_s2       <= {NUM_BUTTONS{1'b1}};
            debounced     <= {NUM_BUTTONS{1'b1}};
            debounced_prev <= {NUM_BUTTONS{1'b1}};
            btn_changed   <= 1'b0;
            for (i = 0; i < NUM_BUTTONS; i = i + 1)
                counter[i] <= {CNT_BITS{1'b0}};
        end else begin
            // Synchronizer
            sync_s1 <= btn_pins;
            sync_s2 <= sync_s1;

            // Debounce each button
            for (i = 0; i < NUM_BUTTONS; i = i + 1) begin
                if (sync_s2[i] != debounced[i]) begin
                    // Pin disagrees with debounced state — count up
                    if (counter[i] == DEBOUNCE_COUNT[CNT_BITS-1:0]) begin
                        // Stable for long enough — accept new state
                        debounced[i] <= sync_s2[i];
                        counter[i]   <= {CNT_BITS{1'b0}};
                    end else begin
                        counter[i] <= counter[i] + {{(CNT_BITS-1){1'b0}}, 1'b1};
                    end
                end else begin
                    // Pin matches debounced state — reset counter
                    counter[i] <= {CNT_BITS{1'b0}};
                end
            end

            // Edge detect: pulse btn_changed for one cycle on any change
            debounced_prev <= debounced;
            btn_changed <= (debounced != debounced_prev);
        end
    end

endmodule
