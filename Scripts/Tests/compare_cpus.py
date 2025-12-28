#!/usr/bin/env python3
"""
CPI Comparison Script - B32P2 vs B32P3
Compares cycle counts and CPI (Cycles Per Instruction) between the two CPU versions.
"""

import subprocess
import os
import re
import sys
from pathlib import Path

# Configuration
REPO_ROOT = Path(__file__).parent.parent.parent
ASSEMBLER_CMD = ["uv", "run", "asmpy"]
IVERILOG_CMD = "iverilog"
VVP_CMD = "vvp"

# Test files to benchmark
BENCHMARK_TESTS = [
    "benchmark/cpi_benchmark.asm",
    "02_alu_basic/add_sub_logic_shift.asm",
    "04_multiply/multiply_fixed_point.asm",
    "05_jump/jump_offset.asm",
    "07_branch/branch_all_conditions.asm",
    "09_pipeline_hazards/consecutive_multicycle.asm",
    "09_pipeline_hazards/data_hazards_alu.asm",
    "10_sdram_cache/cache_miss_hazard.asm",
    "10_sdram_cache/cache_hit_same_line.asm",
    "11_division/unsigned_div_mod.asm",
]

def assemble_test(test_file: str) -> bool:
    """Assemble a test file to ROM list."""
    test_path = REPO_ROOT / "Tests" / "CPU" / test_file
    rom_list = REPO_ROOT / "Hardware" / "FPGA" / "Verilog" / "Simulation" / "MemoryLists" / "rom.list"
    
    if not test_path.exists():
        print(f"  ERROR: Test file not found: {test_path}")
        return False
    
    # asmpy expects: file output (not -o output file)
    cmd = ASSEMBLER_CMD + [str(test_path), str(rom_list)]
    result = subprocess.run(cmd, capture_output=True, text=True, cwd=REPO_ROOT)
    
    if result.returncode != 0:
        print(f"  Assembly error: {result.stderr}")
        return False
    
    return True

def run_simulation(cpu_version: str, timeout: int = 30) -> dict:
    """Run simulation for a CPU version and extract metrics."""
    sim_dir = REPO_ROOT / "Hardware" / "FPGA" / "Verilog" / "Simulation"
    
    if cpu_version == "B32P2":
        tb_file = sim_dir / "cpu_tests_tb.v"
        output_exe = sim_dir / "Output" / "cpu_test"
    else:  # B32P3
        tb_file = sim_dir / "b32p3_tests_tb.v"
        output_exe = sim_dir / "Output" / "b32p3_test"
    
    # Compile testbench - run from REPO_ROOT so includes work correctly
    compile_cmd = [
        IVERILOG_CMD, "-g2012", "-Wall",
        "-o", str(output_exe),
        "-I", str(REPO_ROOT / "Hardware" / "FPGA" / "Verilog" / "Modules"),
        str(tb_file)
    ]
    
    result = subprocess.run(compile_cmd, capture_output=True, text=True, cwd=REPO_ROOT)
    if result.returncode != 0:
        return {"error": f"Compile error: {result.stderr}"}
    
    # Run simulation from REPO_ROOT
    run_cmd = [VVP_CMD, str(output_exe)]
    result = subprocess.run(run_cmd, capture_output=True, text=True, 
                           cwd=REPO_ROOT, timeout=timeout)
    
    output = result.stdout
    
    # Parse output for register values and timing
    metrics = {
        "r15": None,
        "cycles": None,
        "error": None
    }
    
    # Find final r15 value
    r15_matches = re.findall(r'(\d+\.?\d*)\s*ns\s+reg\s+r15:\s*(\d+)', output)
    if r15_matches:
        last_r15 = r15_matches[-1]
        metrics["r15"] = int(last_r15[1])
        # Extract time in ns (simulation time)
        end_time_ns = float(last_r15[0])
        # Convert ns to cycles (assuming 10ns clock period = 100MHz)
        metrics["cycles"] = int(end_time_ns / 10)
    
    # Check for simulation timeout
    if "Simulation timeout" in output:
        metrics["error"] = "Timeout"
    
    return metrics

def count_instructions(test_file: str) -> int:
    """Count approximate instructions in test file."""
    test_path = REPO_ROOT / "Tests" / "CPU" / test_file
    
    if not test_path.exists():
        return 0
    
    with open(test_path, 'r') as f:
        lines = f.readlines()
    
    # Count non-empty, non-comment, non-label lines
    count = 0
    for line in lines:
        stripped = line.strip()
        if stripped and not stripped.startswith(';') and not stripped.endswith(':'):
            # This is likely an instruction
            count += 1
    
    return count

def run_comparison():
    """Run comparison between B32P2 and B32P3."""
    print("=" * 70)
    print("B32P2 vs B32P3 CPI Comparison")
    print("=" * 70)
    print()
    
    results = []
    
    for test_file in BENCHMARK_TESTS:
        print(f"Testing: {test_file}")
        
        # Assemble test
        if not assemble_test(test_file):
            print(f"  SKIP: Assembly failed")
            continue
        
        # Count instructions
        instr_count = count_instructions(test_file)
        
        # Run B32P2
        print(f"  Running B32P2...", end=" ", flush=True)
        b32p2_metrics = run_simulation("B32P2")
        if b32p2_metrics.get("error"):
            print(f"ERROR: {b32p2_metrics['error']}")
        else:
            print(f"r15={b32p2_metrics['r15']}, cycles={b32p2_metrics['cycles']}")
        
        # Run B32P3
        print(f"  Running B32P3...", end=" ", flush=True)
        b32p3_metrics = run_simulation("B32P3")
        if b32p3_metrics.get("error"):
            print(f"ERROR: {b32p3_metrics['error']}")
        else:
            print(f"r15={b32p3_metrics['r15']}, cycles={b32p3_metrics['cycles']}")
        
        # Store results
        results.append({
            "test": test_file,
            "instructions": instr_count,
            "b32p2": b32p2_metrics,
            "b32p3": b32p3_metrics,
        })
        print()
    
    # Print summary table
    print("=" * 70)
    print("SUMMARY")
    print("=" * 70)
    print(f"{'Test':<40} {'B32P2':>10} {'B32P3':>10} {'Diff':>8}")
    print(f"{'':40} {'Cycles':>10} {'Cycles':>10} {'%':>8}")
    print("-" * 70)
    
    total_b32p2 = 0
    total_b32p3 = 0
    
    for r in results:
        test_name = r["test"].split("/")[-1].replace(".asm", "")[:38]
        b32p2_cycles = r["b32p2"].get("cycles", "N/A")
        b32p3_cycles = r["b32p3"].get("cycles", "N/A")
        
        if isinstance(b32p2_cycles, int) and isinstance(b32p3_cycles, int):
            diff = ((b32p3_cycles - b32p2_cycles) / b32p2_cycles * 100)
            diff_str = f"{diff:+.1f}%"
            total_b32p2 += b32p2_cycles
            total_b32p3 += b32p3_cycles
        else:
            diff_str = "N/A"
        
        print(f"{test_name:<40} {str(b32p2_cycles):>10} {str(b32p3_cycles):>10} {diff_str:>8}")
        
        # Verify results match
        b32p2_r15 = r["b32p2"].get("r15")
        b32p3_r15 = r["b32p3"].get("r15")
        if b32p2_r15 != b32p3_r15:
            print(f"  WARNING: Result mismatch! B32P2 r15={b32p2_r15}, B32P3 r15={b32p3_r15}")
    
    print("-" * 70)
    if total_b32p2 > 0 and total_b32p3 > 0:
        total_diff = ((total_b32p3 - total_b32p2) / total_b32p2 * 100)
        print(f"{'TOTAL':<40} {total_b32p2:>10} {total_b32p3:>10} {total_diff:+.1f}%")
    
    print()
    print("Notes:")
    print("- Negative % = B32P3 is faster")
    print("- Positive % = B32P2 is faster")
    print("- Cycles measured from simulation time (10ns clock = 100MHz)")
    
    return results

if __name__ == "__main__":
    try:
        run_comparison()
    except KeyboardInterrupt:
        print("\nInterrupted")
        sys.exit(1)
    except Exception as e:
        print(f"Error: {e}")
        sys.exit(1)
