#!/usr/bin/env python3
"""
Simple test runner for B32P3 CPU tests
"""

import os
import re
import shutil
import subprocess
import sys
from pathlib import Path
from concurrent.futures import ProcessPoolExecutor, as_completed

# Color codes
GREEN = "\033[92m"
RED = "\033[91m"
YELLOW = "\033[93m"
RESET = "\033[0m"

REPO_ROOT = Path(__file__).parent.parent.parent
TESTS_DIR = REPO_ROOT / "Tests/CPU"
TESTBENCH = "Hardware/FPGA/Verilog/Simulation/b32p3_tests_tb.v"
ROM_LIST_PATH = "Hardware/FPGA/Verilog/Simulation/MemoryLists/rom.list"
VERILOG_OUTPUT = "Hardware/FPGA/Verilog/Simulation/Output/b32p3.out"


def parse_expected_result(asm_file: Path) -> int:
    """Parse expected result from assembly file - always checks r15."""
    with open(asm_file) as f:
        for line in f:
            if "expected=" in line:
                try:
                    return int(line.split("expected=")[1].strip())
                except (ValueError, IndexError):
                    pass
    return None


def run_test(test_file: Path, temp_idx: int) -> tuple[str, bool, str]:
    """Run a single test and return (test_name, passed, message)."""
    test_name = f"{test_file.parent.name}/{test_file.name}"
    
    # Parse expected result (always r15)
    exp_val = parse_expected_result(test_file)
    if exp_val is None:
        return test_name, False, "No expected result found"
    
    # Create isolated temp directory for this test
    temp_dir = Path(f"/tmp/b32p3_test_{temp_idx}")
    temp_dir.mkdir(parents=True, exist_ok=True)
    rom_list = temp_dir / "rom.list"
    testbench_copy = temp_dir / "testbench.v"
    sim_output = temp_dir / "sim.out"
    
    # Assemble to the temp rom.list
    asm_result = subprocess.run(
        ["python3", "BuildTools/ASMPY/asmpy/app.py", str(test_file), str(rom_list)],
        capture_output=True, text=True, cwd=REPO_ROOT
    )
    if asm_result.returncode != 0:
        return test_name, False, f"Assembly failed: {asm_result.stderr}"
    
    # Create testbench copy that loads from temp directory
    with open(REPO_ROOT / TESTBENCH, 'r') as f:
        tb_content = f.read()
    # Replace the rom.list path
    tb_content = tb_content.replace(
        "Hardware/FPGA/Verilog/Simulation/MemoryLists/rom.list",
        str(rom_list)
    )
    with open(testbench_copy, 'w') as f:
        f.write(tb_content)
    
    # Compile testbench
    compile_result = subprocess.run(
        ["iverilog", "-g2012", "-o", str(sim_output), "-s", "b32p3_tb", "-I", ".", str(testbench_copy)],
        capture_output=True, text=True, cwd=REPO_ROOT
    )
    if compile_result.returncode != 0:
        return test_name, False, f"Compile failed: {compile_result.stderr}"
    
    # Run simulation
    try:
        sim_result = subprocess.run(
            [str(sim_output)], capture_output=True, text=True, cwd=REPO_ROOT, timeout=30
        )
    except subprocess.TimeoutExpired:
        return test_name, False, "Simulation timeout"
    
    # Parse r15 output
    output = sim_result.stdout + sim_result.stderr
    reg_pattern = r'reg r15:\s*(-?\d+)'
    matches = re.findall(reg_pattern, output)
    
    if not matches:
        return test_name, False, f"No output for r15, expected {exp_val}"
    
    # Get the last value for r15
    actual_val = int(matches[-1])
    
    # Cleanup temp directory
    try:
        shutil.rmtree(temp_dir)
    except:
        pass
    
    if actual_val == exp_val:
        return test_name, True, f"r15={actual_val}"
    else:
        return test_name, False, f"r15: expected={exp_val}, got={actual_val}"


def main():
    # Check for single file argument
    single_file = None
    if len(sys.argv) > 1:
        single_file = sys.argv[1]
        # Handle both relative and absolute paths
        if not single_file.startswith('/'):
            single_file = TESTS_DIR / single_file
        else:
            single_file = Path(single_file)
        
        if not single_file.exists():
            print(f"{RED}Error: Test file not found: {single_file}{RESET}")
            return 1
        
        test_files = [single_file]
        print(f"Running single test: {single_file.name}\n")
    else:
        # Gather all test files
        test_files = sorted(TESTS_DIR.rglob("*.asm"))
        print(f"Running {len(test_files)} tests against B32P3...\n")
    
    passed = 0
    failed = 0
    failures = []
    
    # Run tests in parallel (or single test)
    with ProcessPoolExecutor(max_workers=os.cpu_count()) as executor:
        futures = {
            executor.submit(run_test, test_file, idx): test_file 
            for idx, test_file in enumerate(test_files)
        }
        
        for future in as_completed(futures):
            test_name, success, msg = future.result()
            if success:
                print(f"{GREEN}✓ {test_name}: {msg}{RESET}")
                passed += 1
            else:
                print(f"{RED}✗ {test_name}: {msg}{RESET}")
                failed += 1
                failures.append((test_name, msg))
    
    print(f"\n{'='*60}")
    print(f"Results: {GREEN}{passed} passed{RESET}, {RED}{failed} failed{RESET}")
    
    if failures:
        print(f"\n{RED}Failures:{RESET}")
        for name, msg in sorted(failures):
            print(f"  {name}: {msg}")
    
    return 0 if failed == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
