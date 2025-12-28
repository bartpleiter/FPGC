#!/usr/bin/env python3
"""
CPI Comparison Script - B32P2 vs B32P3
Compares cycle counts between the two CPU versions when executing from RAM.
This is the realistic scenario as all user programs run from SDRAM.
"""

import subprocess
import os
import re
import sys
import shutil
from pathlib import Path

# Configuration
REPO_ROOT = Path(__file__).parent.parent.parent
ASSEMBLER_CMD = ["uv", "run", "asmpy"]
IVERILOG_CMD = "iverilog"
VVP_CMD = "vvp"
CONVERTER_SCRIPT = REPO_ROOT / "Scripts" / "Simulation" / "convert_to_256_bit.py"

# Memory list paths
MEMORY_LISTS_DIR = REPO_ROOT / "Hardware" / "FPGA" / "Verilog" / "Simulation" / "MemoryLists"
ROM_LIST = MEMORY_LISTS_DIR / "rom.list"
RAM_LIST = MEMORY_LISTS_DIR / "ram.list"
SDRAM_LIST = MEMORY_LISTS_DIR / "sdram.list"

# Bootloader for RAM execution
BOOTLOADER_PATH = REPO_ROOT / "Software" / "ASM" / "Simulation" / "sim_jump_to_ram.asm"

# Benchmark test
BENCHMARK_PATH = REPO_ROOT / "Software" / "ASM" / "benchmark" / "cpi_benchmark.asm"

# Standard tests to include in comparison
STANDARD_TESTS = [
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


def assemble_for_ram(test_path: Path) -> bool:
    """Assemble a test file for RAM execution (with bootloader)."""
    # First, assemble bootloader to ROM
    cmd = ASSEMBLER_CMD + [str(BOOTLOADER_PATH), str(ROM_LIST)]
    result = subprocess.run(cmd, capture_output=True, text=True, cwd=REPO_ROOT)
    
    if result.returncode != 0:
        print(f"  Assembly error (bootloader): {result.stderr}")
        return False
    
    # Assemble test code to RAM list
    cmd = ASSEMBLER_CMD + [str(test_path), str(RAM_LIST)]
    result = subprocess.run(cmd, capture_output=True, text=True, cwd=REPO_ROOT)
    
    if result.returncode != 0:
        print(f"  Assembly error (RAM): {result.stderr}")
        return False
    
    # Convert to 256-bit format for SDRAM
    cmd = ["python3", str(CONVERTER_SCRIPT), str(RAM_LIST), str(SDRAM_LIST)]
    result = subprocess.run(cmd, capture_output=True, text=True, cwd=REPO_ROOT)
    
    if result.returncode != 0:
        print(f"  Conversion error: {result.stderr}")
        return False
    
    return True


def create_testbench(cpu_version: str, output_path: Path) -> bool:
    """Create a testbench for the specified CPU version."""
    sim_dir = REPO_ROOT / "Hardware" / "FPGA" / "Verilog" / "Simulation"
    src = sim_dir / "cpu_tests_tb.v"
    
    with open(src, 'r') as f:
        content = f.read()
    
    if cpu_version == "B32P2":
        # Replace B32P3 with B32P2
        content = content.replace('`include "Hardware/FPGA/Verilog/Modules/CPU/B32P3.v"',
                                 '`include "Hardware/FPGA/Verilog/Modules/CPU/B32P2.v"')
        content = content.replace('B32P3 cpu', 'B32P2 cpu')
    # else: keep B32P3 (current testbench)
    
    with open(output_path, 'w') as f:
        f.write(content)
    
    return True


def run_simulation(cpu_version: str, timeout: int = 60) -> dict:
    """Run simulation for a CPU version and extract metrics."""
    sim_dir = REPO_ROOT / "Hardware" / "FPGA" / "Verilog" / "Simulation"
    output_dir = sim_dir / "Output"
    output_dir.mkdir(exist_ok=True)
    
    # Create temporary testbench with correct CPU
    tb_file = output_dir / f"{cpu_version.lower()}_bench.v"
    output_exe = output_dir / f"{cpu_version.lower()}_bench.out"
    
    if not create_testbench(cpu_version, tb_file):
        return {"error": "Failed to create testbench"}
    
    # Compile testbench
    compile_cmd = [IVERILOG_CMD, "-o", str(output_exe), str(tb_file)]
    
    result = subprocess.run(compile_cmd, capture_output=True, text=True, cwd=REPO_ROOT)
    if result.returncode != 0:
        return {"error": f"Compile error: {result.stderr}"}
    
    # Run simulation
    run_cmd = [VVP_CMD, str(output_exe)]
    try:
        result = subprocess.run(run_cmd, capture_output=True, text=True, 
                               cwd=REPO_ROOT, timeout=timeout)
    except subprocess.TimeoutExpired:
        return {"error": "Simulation timeout"}
    
    output = result.stdout
    
    # Parse output for register values and timing
    metrics = {
        "r15": None,
        "cycles": None,
        "error": None
    }
    
    # Find final r15 value before halt
    # Format: "1234.0 ns     reg r15:      42"
    r15_matches = re.findall(r'(\d+\.?\d*)\s*ns\s+reg\s+r15:\s*(\d+)', output)
    if r15_matches:
        last_r15 = r15_matches[-1]
        metrics["r15"] = int(last_r15[1])
        # Extract time in ns (simulation time)
        end_time_ns = float(last_r15[0])
        # Convert ns to cycles (20ns clock period = 50MHz main clock)
        metrics["cycles"] = int(end_time_ns / 20)
    
    return metrics


def run_comparison():
    """Run comparison between B32P2 and B32P3 executing from RAM."""
    print("=" * 80)
    print("B32P2 vs B32P3 CPI Comparison (Executing from RAM)")
    print("=" * 80)
    print()
    print("Note: Tests execute from SDRAM via bootloader, simulating real-world usage.")
    print()
    
    results = []
    
    # First, run the main benchmark
    test_files = []
    
    if BENCHMARK_PATH.exists():
        test_files.append(("cpi_benchmark", BENCHMARK_PATH))
    
    # Add standard tests
    for test in STANDARD_TESTS:
        test_path = REPO_ROOT / "Tests" / "CPU" / test
        if test_path.exists():
            test_name = test.split("/")[-1].replace(".asm", "")
            test_files.append((test_name, test_path))
    
    for test_name, test_path in test_files:
        print(f"Testing: {test_name}")
        
        # Assemble test for RAM execution
        if not assemble_for_ram(test_path):
            print(f"  SKIP: Assembly failed")
            continue
        
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
            "test": test_name,
            "b32p2": b32p2_metrics,
            "b32p3": b32p3_metrics,
        })
        print()
    
    # Print summary table
    print("=" * 80)
    print("SUMMARY - RAM Execution")
    print("=" * 80)
    print(f"{'Test':<35} {'B32P2':>12} {'B32P3':>12} {'Diff':>10} {'Notes':<10}")
    print(f"{'':35} {'Cycles':>12} {'Cycles':>12} {'':>10}")
    print("-" * 80)
    
    total_b32p2 = 0
    total_b32p3 = 0
    
    for r in results:
        test_name = r["test"][:33]
        b32p2_cycles = r["b32p2"].get("cycles", "N/A")
        b32p3_cycles = r["b32p3"].get("cycles", "N/A")
        
        notes = ""
        if isinstance(b32p2_cycles, int) and isinstance(b32p3_cycles, int):
            diff = ((b32p3_cycles - b32p2_cycles) / b32p2_cycles * 100)
            diff_str = f"{diff:+.1f}%"
            total_b32p2 += b32p2_cycles
            total_b32p3 += b32p3_cycles
            
            if diff < -5:
                notes = "✓ Faster"
            elif diff > 5:
                notes = "✗ Slower"
        else:
            diff_str = "N/A"
        
        print(f"{test_name:<35} {str(b32p2_cycles):>12} {str(b32p3_cycles):>12} {diff_str:>10} {notes:<10}")
        
        # Verify results match
        b32p2_r15 = r["b32p2"].get("r15")
        b32p3_r15 = r["b32p3"].get("r15")
        if b32p2_r15 is not None and b32p3_r15 is not None and b32p2_r15 != b32p3_r15:
            print(f"  ⚠ Result mismatch! B32P2 r15={b32p2_r15}, B32P3 r15={b32p3_r15}")
    
    print("-" * 80)
    if total_b32p2 > 0 and total_b32p3 > 0:
        total_diff = ((total_b32p3 - total_b32p2) / total_b32p2 * 100)
        verdict = "B32P3 FASTER" if total_diff < 0 else "B32P2 FASTER"
        print(f"{'TOTAL':<35} {total_b32p2:>12} {total_b32p3:>12} {total_diff:+.1f}%  {verdict}")
    
    print()
    print("Analysis:")
    print("-" * 80)
    print("- Negative % = B32P3 is faster")
    print("- Positive % = B32P2 is faster")
    print("- Tests execute from SDRAM (address 0x0) via bootloader")
    print("- This measures real-world performance including L1i cache behavior")
    print()
    
    # Performance recommendation
    if total_b32p2 > 0 and total_b32p3 > 0:
        if total_diff < 0:
            print(f"Conclusion: B32P3 is {-total_diff:.1f}% faster than B32P2 from RAM.")
            if total_diff < -7:
                print("This exceeds the target improvement.")
        elif total_diff > 0:
            print(f"Conclusion: B32P3 is {total_diff:.1f}% slower than B32P2 from RAM.")
            print("WARNING: B32P3 needs optimization for RAM execution.")
        else:
            print("Conclusion: B32P2 and B32P3 have similar performance from RAM.")
    
    return results


if __name__ == "__main__":
    try:
        run_comparison()
    except KeyboardInterrupt:
        print("\nInterrupted")
        sys.exit(1)
    except Exception as e:
        import traceback
        traceback.print_exc()
        print(f"Error: {e}")
        sys.exit(1)
