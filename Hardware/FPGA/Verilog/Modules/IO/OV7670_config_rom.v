// OV7670 Configuration ROM — YUV422 QVGA (320×240) for Y-channel extraction
// 16-bit entries: {register_address[15:8], register_data[7:0]}
// Special: 0xFFF0 = 10ms delay, 0xFFFF = end of ROM

`timescale 1ns / 1ps

module OV7670_config_rom (
    input  wire       clk,
    input  wire [7:0] addr,
    output reg [15:0] dout
);

    always @(posedge clk) begin
        case (addr)
        // --- Software reset ---
        0:  dout <= 16'h12_80;  // COM7: reset all registers
        1:  dout <= 16'hFF_F0;  // 10ms delay

        // --- Output format: YUV422, QVGA ---
        2:  dout <= 16'h12_10;  // COM7: QVGA mode, YUV output
        3:  dout <= 16'h11_00;  // CLKRC: no prescaler (PCLK = XCLK)
        4:  dout <= 16'h0C_04;  // COM3: enable scaling
        5:  dout <= 16'h3E_19;  // COM14: PCLK scaling = manual, divide by 2
        6:  dout <= 16'h72_11;  // Downsampling: internal 2× downsample
        7:  dout <= 16'h73_f1;  // DSP scaling: clock divider
        8:  dout <= 16'h70_3a;  // Scaling XSC
        9:  dout <= 16'h71_35;  // Scaling YSC
        10: dout <= 16'ha2_02;  // Scaling PCLK delay

        // --- Timing / window ---
        11: dout <= 16'h17_14;  // HSTART
        12: dout <= 16'h18_02;  // HSTOP
        13: dout <= 16'h32_80;  // HREF edge offset
        14: dout <= 16'h19_03;  // VSTART
        15: dout <= 16'h1A_7B;  // VSTOP
        16: dout <= 16'h03_0A;  // VREF

        // --- YUV / data format ---
        17: dout <= 16'h04_00;  // COM1: disable CCIR656
        18: dout <= 16'h40_C0;  // COM15: YUV full output range [00..FF]
        19: dout <= 16'h3A_04;  // TSLB: YUYV byte order, auto output window
        20: dout <= 16'h3D_88;  // COM13: gamma enable, UV auto adjust
        21: dout <= 16'h15_00;  // COM10: VSYNC negative, HREF positive, PCLK normal

        // --- AGC / AEC ---
        22: dout <= 16'h13_E0;  // COM8: disable AGC/AEC temporarily
        23: dout <= 16'h00_00;  // GAIN: set to 0
        24: dout <= 16'h10_00;  // AECH: exposure = 0
        25: dout <= 16'h0D_40;  // COM4: reserved magic
        26: dout <= 16'h14_18;  // COM9: max AGC 4×
        27: dout <= 16'hA5_05;  // BD50MAX
        28: dout <= 16'hAB_07;  // BD60MAX
        29: dout <= 16'h24_95;  // AGC upper limit
        30: dout <= 16'h25_33;  // AGC lower limit
        31: dout <= 16'h26_E3;  // AGC/AEC fast mode region
        32: dout <= 16'h9F_78;  // HAECC1
        33: dout <= 16'hA0_68;  // HAECC2
        34: dout <= 16'hA1_03;  // Magic
        35: dout <= 16'hA6_D8;  // HAECC3
        36: dout <= 16'hA7_D8;  // HAECC4
        37: dout <= 16'hA8_F0;  // HAECC5
        38: dout <= 16'hA9_90;  // HAECC6
        39: dout <= 16'hAA_94;  // HAECC7
        40: dout <= 16'h13_E5;  // COM8: re-enable AGC/AEC

        // --- Gamma curve ---
        41: dout <= 16'h7A_20;
        42: dout <= 16'h7B_10;
        43: dout <= 16'h7C_1E;
        44: dout <= 16'h7D_35;
        45: dout <= 16'h7E_5A;
        46: dout <= 16'h7F_69;
        47: dout <= 16'h80_76;
        48: dout <= 16'h81_80;
        49: dout <= 16'h82_88;
        50: dout <= 16'h83_8F;
        51: dout <= 16'h84_96;
        52: dout <= 16'h85_A3;
        53: dout <= 16'h86_AF;
        54: dout <= 16'h87_C4;
        55: dout <= 16'h88_D7;
        56: dout <= 16'h89_E8;

        // --- Misc ---
        57: dout <= 16'h0F_41;  // COM6: reset timings
        58: dout <= 16'h1E_00;  // MVFP: no mirror/flip
        59: dout <= 16'h33_0B;  // CHLF: magic from reference
        60: dout <= 16'h3C_78;  // COM12: no HREF when VSYNC low
        61: dout <= 16'h69_00;  // GFIX: gain control fix
        62: dout <= 16'h74_00;  // REG74: digital gain = 0
        63: dout <= 16'hB0_84;  // RSVD: required for good output
        64: dout <= 16'hB1_0C;  // ABLC1
        65: dout <= 16'hB2_0E;  // RSVD: magic
        66: dout <= 16'hB3_80;  // THL_ST

        // --- Night mode (auto frame rate reduction) ---
        67: dout <= 16'h3B_0A;  // COM11: night mode enable, 1/2 frame rate min

        default: dout <= 16'hFF_FF;  // End of ROM
        endcase
    end

endmodule
