// The shared part of the unit testbenches (tb_<unit>.v). Include it inside the module.
//
// A unit testbench applies one vector per clock cycle:
//
//   1. At a falling edge, it reads the next line of the vector file into the inputs.
//   2. The rising edge in the middle of the cycle clocks a sequential unit.
//   3. At the next falling edge, it writes the outputs as one line, then goes to 1.
//
// So the waveform reads from left to right as a list of cases. Plusargs:
//
//   +vectors=<file>   the input vectors: one per line, hex columns (required). The
//                     first column is a label: up to 12 ASCII characters that name the
//                     case in the waveform, such as the operation.
//   +outputs=<file>   the outputs: one line per vector, hex columns (required)
//   +vcd=<file>       also dump a waveform
//
// The test (test_<unit>.py) writes the vectors and checks the outputs.

reg clk = 0;
always #5 clk = !clk;

integer vectors_fd;
integer outputs_fd;
reg [8*12-1:0] label;  // shown as ASCII in the presets

task tb_open;
    reg [8*256-1:0] path;
    begin
        if (!$value$plusargs("vectors=%s", path))
            $fatal(1, "+vectors=<file> is missing");
        vectors_fd = $fopen(path, "r");
        if (vectors_fd == 0)
            $fatal(1, "cannot read %0s", path);
        if (!$value$plusargs("outputs=%s", path))
            $fatal(1, "+outputs=<file> is missing");
        outputs_fd = $fopen(path, "w");
        if ($value$plusargs("vcd=%s", path)) begin
            $dumpfile(path);
            $dumpvars(1, clk);
            $dumpvars(1, label);
            $dumpvars(0, dut);  // every testbench names the unit `dut`
        end
    end
endtask

task tb_close;
    begin
        $fclose(outputs_fd);
        $finish(0);  // 0: without a message
    end
endtask
