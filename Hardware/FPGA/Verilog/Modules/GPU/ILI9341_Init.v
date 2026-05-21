/*
 * ILI9341_Init
 * One-shot initialization sequencer for the ILI9341 display controller.
 *
 * After reset, this module:
 *   1. Holds LCD_RST_N low for 10ms, then high, waits 120ms
 *   2. Sends the full ILI9341 initialization command sequence
 *   3. Asserts init_done (stays high forever)
 *
 * The init sequence is stored in a ROM of (type, byte) pairs:
 *   type=0: send as command byte (DC=0)
 *   type=1: send as data byte (DC=1)
 *   type=2: delay marker (byte = delay in ms, max 255)
 *   type=3: end of sequence
 *
 * Clocked at 100 MHz. SPI runs at a slow speed during init (divider=50,
 * ~2 MHz) to ensure reliable communication during power-up.
 */
module ILI9341_Init (
    input  wire        clk,        // 100 MHz
    input  wire        reset,

    // SPI master interface
    output reg  [7:0]  spi_tx_data,
    output reg         spi_tx_valid,
    input  wire        spi_tx_ready,
    output reg         spi_dc,

    // Control signals
    output reg         spi_cs_n,   // Directly control CS during init
    output reg         lcd_rst_n,  // Hardware reset pin

    // Status
    output reg         init_done   // Goes high when init is complete
);

    // ---- Delay counter ----
    // 100 MHz → 1ms = 100,000 cycles
    localparam CYCLES_PER_MS = 100_000;
    reg [23:0] delay_cnt = 24'd0;
    reg        delay_active = 1'b0;
    reg        spi_accepted = 1'b0; // Tracks that tx_ready went low

    // ---- Init ROM ----
    // Format: {type[1:0], byte[7:0]} = 10 bits per entry
    // type: 00=cmd, 01=data, 10=delay(ms), 11=end
    localparam TYPE_CMD   = 2'b00;
    localparam TYPE_DATA  = 2'b01;
    localparam TYPE_DELAY = 2'b10;
    localparam TYPE_END   = 2'b11;

    localparam ROM_SIZE = 128;
    reg [9:0] init_rom [0:ROM_SIZE-1];
    reg [6:0] rom_idx = 7'd0;

    initial begin
        // ---- Hardware reset delays are handled in state machine ----

        // Software Reset
        init_rom[0]  = {TYPE_CMD,   8'h01};
        init_rom[1]  = {TYPE_DELAY, 8'd5};     // 5ms

        // Display OFF
        init_rom[2]  = {TYPE_CMD,   8'h28};

        // Power Control A
        init_rom[3]  = {TYPE_CMD,   8'hCB};
        init_rom[4]  = {TYPE_DATA,  8'h39};
        init_rom[5]  = {TYPE_DATA,  8'h2C};
        init_rom[6]  = {TYPE_DATA,  8'h00};
        init_rom[7]  = {TYPE_DATA,  8'h34};
        init_rom[8]  = {TYPE_DATA,  8'h02};

        // Power Control B
        init_rom[9]  = {TYPE_CMD,   8'hCF};
        init_rom[10] = {TYPE_DATA,  8'h00};
        init_rom[11] = {TYPE_DATA,  8'hC1};
        init_rom[12] = {TYPE_DATA,  8'h30};

        // Driver Timing Control A
        init_rom[13] = {TYPE_CMD,   8'hE8};
        init_rom[14] = {TYPE_DATA,  8'h85};
        init_rom[15] = {TYPE_DATA,  8'h00};
        init_rom[16] = {TYPE_DATA,  8'h78};

        // Driver Timing Control B
        init_rom[17] = {TYPE_CMD,   8'hEA};
        init_rom[18] = {TYPE_DATA,  8'h00};
        init_rom[19] = {TYPE_DATA,  8'h00};

        // Power On Sequence Control
        init_rom[20] = {TYPE_CMD,   8'hED};
        init_rom[21] = {TYPE_DATA,  8'h64};
        init_rom[22] = {TYPE_DATA,  8'h03};
        init_rom[23] = {TYPE_DATA,  8'h12};
        init_rom[24] = {TYPE_DATA,  8'h81};

        // Pump Ratio Control
        init_rom[25] = {TYPE_CMD,   8'hF7};
        init_rom[26] = {TYPE_DATA,  8'h20};

        // Power Control 1 (VRH=4.60V)
        init_rom[27] = {TYPE_CMD,   8'hC0};
        init_rom[28] = {TYPE_DATA,  8'h23};

        // Power Control 2
        init_rom[29] = {TYPE_CMD,   8'hC1};
        init_rom[30] = {TYPE_DATA,  8'h10};

        // VCOM Control 1
        init_rom[31] = {TYPE_CMD,   8'hC5};
        init_rom[32] = {TYPE_DATA,  8'h3E};
        init_rom[33] = {TYPE_DATA,  8'h28};

        // VCOM Control 2
        init_rom[34] = {TYPE_CMD,   8'hC7};
        init_rom[35] = {TYPE_DATA,  8'h86};

        // MADCTL: Memory Access Control
        // 0x28 = Row/Column Exchange + BGR
        // This gives landscape 320x240 with correct color order
        init_rom[36] = {TYPE_CMD,   8'h36};
        init_rom[37] = {TYPE_DATA,  8'h28};

        // Display Inversion OFF
        init_rom[38] = {TYPE_CMD,   8'h20};

        // Pixel Format Set: RGB565 (16 bits/pixel)
        init_rom[39] = {TYPE_CMD,   8'h3A};
        init_rom[40] = {TYPE_DATA,  8'h55};

        // Frame Rate Control (Normal Mode)
        // DIVA=fosc, RTNA=0x10 (119 Hz internal refresh)
        init_rom[41] = {TYPE_CMD,   8'hB1};
        init_rom[42] = {TYPE_DATA,  8'h00};
        init_rom[43] = {TYPE_DATA,  8'h10};

        // Display Function Control
        init_rom[44] = {TYPE_CMD,   8'hB6};
        init_rom[45] = {TYPE_DATA,  8'h08};
        init_rom[46] = {TYPE_DATA,  8'h82};
        init_rom[47] = {TYPE_DATA,  8'h27};

        // Enable 3G (disabled)
        init_rom[48] = {TYPE_CMD,   8'hF2};
        init_rom[49] = {TYPE_DATA,  8'h02};

        // Gamma Set: curve 1
        init_rom[50] = {TYPE_CMD,   8'h26};
        init_rom[51] = {TYPE_DATA,  8'h01};

        // Positive Gamma Correction
        init_rom[52] = {TYPE_CMD,   8'hE0};
        init_rom[53] = {TYPE_DATA,  8'h0F};
        init_rom[54] = {TYPE_DATA,  8'h31};
        init_rom[55] = {TYPE_DATA,  8'h2B};
        init_rom[56] = {TYPE_DATA,  8'h0C};
        init_rom[57] = {TYPE_DATA,  8'h0E};
        init_rom[58] = {TYPE_DATA,  8'h08};
        init_rom[59] = {TYPE_DATA,  8'h4E};
        init_rom[60] = {TYPE_DATA,  8'hF1};
        init_rom[61] = {TYPE_DATA,  8'h37};
        init_rom[62] = {TYPE_DATA,  8'h07};
        init_rom[63] = {TYPE_DATA,  8'h10};
        init_rom[64] = {TYPE_DATA,  8'h03};
        init_rom[65] = {TYPE_DATA,  8'h0E};
        init_rom[66] = {TYPE_DATA,  8'h09};
        init_rom[67] = {TYPE_DATA,  8'h00};

        // Negative Gamma Correction
        init_rom[68] = {TYPE_CMD,   8'hE1};
        init_rom[69] = {TYPE_DATA,  8'h00};
        init_rom[70] = {TYPE_DATA,  8'h0E};
        init_rom[71] = {TYPE_DATA,  8'h14};
        init_rom[72] = {TYPE_DATA,  8'h03};
        init_rom[73] = {TYPE_DATA,  8'h11};
        init_rom[74] = {TYPE_DATA,  8'h07};
        init_rom[75] = {TYPE_DATA,  8'h31};
        init_rom[76] = {TYPE_DATA,  8'hC1};
        init_rom[77] = {TYPE_DATA,  8'h48};
        init_rom[78] = {TYPE_DATA,  8'h08};
        init_rom[79] = {TYPE_DATA,  8'h0F};
        init_rom[80] = {TYPE_DATA,  8'h0C};
        init_rom[81] = {TYPE_DATA,  8'h31};
        init_rom[82] = {TYPE_DATA,  8'h36};
        init_rom[83] = {TYPE_DATA,  8'h0F};

        // Sleep Out
        init_rom[84] = {TYPE_CMD,   8'h11};
        init_rom[85] = {TYPE_DELAY, 8'd120};   // 120ms

        // Display ON
        init_rom[86] = {TYPE_CMD,   8'h29};
        init_rom[87] = {TYPE_DELAY, 8'd20};    // 20ms settle

        // Set address window (landscape 320x240) and start RAMWR
        // CASET: columns 0-319
        init_rom[88]  = {TYPE_CMD,   8'h2A};
        init_rom[89]  = {TYPE_DATA,  8'h00}; // Start col high
        init_rom[90]  = {TYPE_DATA,  8'h00}; // Start col low
        init_rom[91]  = {TYPE_DATA,  8'h01}; // End col high (319=0x013F)
        init_rom[92]  = {TYPE_DATA,  8'h3F}; // End col low
        // PASET: rows 0-239
        init_rom[93]  = {TYPE_CMD,   8'h2B};
        init_rom[94]  = {TYPE_DATA,  8'h00}; // Start row high
        init_rom[95]  = {TYPE_DATA,  8'h00}; // Start row low
        init_rom[96]  = {TYPE_DATA,  8'h00}; // End row high (239=0x00EF)
        init_rom[97]  = {TYPE_DATA,  8'hEF}; // End row low
        // RAMWR: start memory write (stays active until next command)
        init_rom[98]  = {TYPE_CMD,   8'h2C};

        // End of sequence
        init_rom[99]  = {TYPE_END,   8'h00};
    end

    // ---- Main state machine ----
    localparam
        S_RESET_LOW  = 3'd0,   // Hold RST low
        S_RESET_WAIT = 3'd1,   // Wait after RST high
        S_RUN_ROM    = 3'd2,   // Process ROM entries
        S_SEND_BYTE  = 3'd3,   // Send a byte over SPI
        S_WAIT_SPI   = 3'd4,   // Wait for SPI to finish
        S_DELAY      = 3'd5,   // Timed delay
        S_DONE       = 3'd6;   // Init complete

    reg [2:0] state = S_RESET_LOW;

    always @(posedge clk) begin
        if (reset) begin
            state <= S_RESET_LOW;
            rom_idx <= 7'd0;
            lcd_rst_n <= 1'b0;
            spi_cs_n <= 1'b1;
            spi_tx_valid <= 1'b0;
            spi_tx_data <= 8'd0;
            spi_dc <= 1'b0;
            init_done <= 1'b0;
            delay_cnt <= 24'd0;
            delay_active <= 1'b0;
            spi_accepted <= 1'b0;
        end else begin
            spi_tx_valid <= 1'b0; // Default: no transmit

            case (state)
                S_RESET_LOW: begin
                    lcd_rst_n <= 1'b0;
                    spi_cs_n <= 1'b1;
                    if (delay_active) begin
                        if (delay_cnt == 24'd0) begin
                            delay_active <= 1'b0;
                            lcd_rst_n <= 1'b1;
                            // Wait 120ms after reset release
                            delay_cnt <= 24'd120 * CYCLES_PER_MS;
                            delay_active <= 1'b1;
                            state <= S_RESET_WAIT;
                        end else begin
                            delay_cnt <= delay_cnt - 24'd1;
                        end
                    end else begin
                        // Start 10ms reset pulse
                        delay_cnt <= 24'd10 * CYCLES_PER_MS;
                        delay_active <= 1'b1;
                    end
                end

                S_RESET_WAIT: begin
                    lcd_rst_n <= 1'b1;
                    if (delay_cnt == 24'd0) begin
                        delay_active <= 1'b0;
                        rom_idx <= 7'd0;
                        spi_cs_n <= 1'b0; // Assert CS for commands
                        state <= S_RUN_ROM;
                    end else begin
                        delay_cnt <= delay_cnt - 24'd1;
                    end
                end

                S_RUN_ROM: begin
                    // Read current ROM entry
                    case (init_rom[rom_idx][9:8])
                        TYPE_CMD: begin
                            spi_dc <= 1'b0; // Command
                            spi_tx_data <= init_rom[rom_idx][7:0];
                            state <= S_SEND_BYTE;
                        end
                        TYPE_DATA: begin
                            spi_dc <= 1'b1; // Data
                            spi_tx_data <= init_rom[rom_idx][7:0];
                            state <= S_SEND_BYTE;
                        end
                        TYPE_DELAY: begin
                            delay_cnt <= {16'd0, init_rom[rom_idx][7:0]} * CYCLES_PER_MS;
                            delay_active <= 1'b1;
                            rom_idx <= rom_idx + 7'd1;
                            state <= S_DELAY;
                        end
                        TYPE_END: begin
                            // Keep CS asserted — FrameScanEngine will
                            // continue streaming pixel data immediately.
                            state <= S_DONE;
                        end
                    endcase
                end

                S_SEND_BYTE: begin
                    if (spi_tx_ready) begin
                        spi_tx_valid <= 1'b1;
                        spi_accepted <= 1'b0;
                        state <= S_WAIT_SPI;
                    end
                end

                S_WAIT_SPI: begin
                    // Wait for tx_ready to drop (byte accepted by SPIMaster),
                    // then wait for it to rise again (byte transmitted).
                    if (!spi_accepted) begin
                        if (!spi_tx_ready)
                            spi_accepted <= 1'b1;
                    end else begin
                        if (spi_tx_ready) begin
                            rom_idx <= rom_idx + 7'd1;
                            state <= S_RUN_ROM;
                        end
                    end
                end

                S_DELAY: begin
                    if (delay_cnt == 24'd0) begin
                        delay_active <= 1'b0;
                        state <= S_RUN_ROM;
                    end else begin
                        delay_cnt <= delay_cnt - 24'd1;
                    end
                end

                S_DONE: begin
                    init_done <= 1'b1;
                    // CS stays low (asserted) — pixel streaming follows
                end
            endcase
        end
    end

endmodule
