#!/usr/bin/env python3
"""Sweep IRQ injection timing across many cycle offsets to try to
reproduce the post-Phase-B CPU freeze in simulation.

Strategy:
  - Build the cpu_irq_inject_tb once.
  - Re-run vvp with different +IRQ_PERIOD / +IRQ_FIRST values.
  - Flag any run whose output contains "[hang-detect]" or whose
    final r15 is not the expected iteration count.
"""

import os
import subprocess
import sys
from concurrent.futures import ProcessPoolExecutor, as_completed
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
TB = REPO / "Hardware/FPGA/Verilog/Simulation/cpu_irq_inject_tb.v"
ROM_LIST = REPO / "Hardware/FPGA/Verilog/Simulation/MemoryLists/rom.list"
RAM_LIST = REPO / "Hardware/FPGA/Verilog/Simulation/MemoryLists/sdram.list"
ASM = REPO / os.environ.get(
    "IRQ_SWEEP_ASM", "Tests/host/dma_irq_sim/spi2mem_irq_stress.asm"
)
EXPECTED = int(os.environ.get("IRQ_SWEEP_EXPECTED", "8"))
OUT = REPO / "tmp/cpu_irq_inject.out"
USE_RAM = os.environ.get("IRQ_SWEEP_RAM", "0") == "1"
BOOT = REPO / "Software/ASM/Simulation/sim_jump_to_ram.asm"
CONV = REPO / "Scripts/Simulation/convert_to_256_bit.py"


def run_one(args):
    p, f = args
    cmd = f"vvp {OUT} +IRQ_PERIOD={p} +IRQ_FIRST={f}"
    r = subprocess.run(
        cmd, shell=True, cwd=str(REPO), capture_output=True, text=True, timeout=60
    )
    out = r.stdout + r.stderr
    wedge = "[hang-detect]" in out
    r15 = None
    for line in out.splitlines():
        if "reg r15:" in line:
            try:
                r15 = int(line.split("reg r15:")[1].strip())
            except ValueError:
                pass
    return (p, f, wedge, r15, out)


def main():
    OUT.parent.mkdir(parents=True, exist_ok=True)
    if USE_RAM:
        # Build sim_jump_to_ram into ROM, test into RAM via SDRAM init
        r = subprocess.run(
            f"asmpy {BOOT} {ROM_LIST} -o 0x1E000000",
            shell=True,
            cwd=str(REPO),
            capture_output=True,
            text=True,
        )
        if r.returncode != 0:
            print("asmpy boot failed:", r.stderr)
            sys.exit(1)
        ram_tmp = REPO / "tmp/test_ram.list"
        r = subprocess.run(
            f"asmpy {ASM} {ram_tmp}",
            shell=True,
            cwd=str(REPO),
            capture_output=True,
            text=True,
        )
        if r.returncode != 0:
            print("asmpy ram failed:", r.stderr)
            sys.exit(1)
        r = subprocess.run(
            f"python3 {CONV} {ram_tmp} {RAM_LIST}",
            shell=True,
            cwd=str(REPO),
            capture_output=True,
            text=True,
        )
        if r.returncode != 0:
            print("convert failed:", r.stderr)
            sys.exit(1)
    else:
        r = subprocess.run(
            f"asmpy {ASM} {ROM_LIST}",
            shell=True,
            cwd=str(REPO),
            capture_output=True,
            text=True,
        )
        if r.returncode != 0:
            print("asmpy failed:", r.stderr)
            sys.exit(1)
    r = subprocess.run(
        f"iverilog -o {OUT} {TB}",
        shell=True,
        cwd=str(REPO),
        capture_output=True,
        text=True,
    )
    if r.returncode != 0:
        print("iverilog failed:", r.stderr)
        sys.exit(1)

    periods = [
        50,
        80,
        110,
        150,
        200,
        300,
        500,
        800,
        1100,
        1500,
        1700,
        2000,
        2500,
        3000,
        3500,
        4000,
    ]
    firsts = [
        50,
        70,
        90,
        110,
        130,
        150,
        170,
        190,
        210,
        230,
        250,
        270,
        290,
        310,
        330,
        350,
        370,
        390,
    ]
    grid = [(p, f) for p in periods for f in firsts]

    hangs = []
    bad_results = []
    done = 0
    workers = max(1, os.cpu_count() // 2)
    print(f"Running {len(grid)} simulations with {workers} workers...")
    with ProcessPoolExecutor(max_workers=workers) as ex:
        futures = {ex.submit(run_one, item): item for item in grid}
        for fut in as_completed(futures):
            p, f, wedge, r15, out = fut.result()
            done += 1
            tag = "OK"
            if wedge:
                tag = "HANG"
                hangs.append((p, f, out))
            elif r15 != EXPECTED:
                tag = f"BADRES r15={r15}"
                bad_results.append((p, f, r15, out))
            print(
                f"[{done:4d}/{len(grid)}] PERIOD={p:5d} FIRST={f:4d} -> {tag}",
                flush=True,
            )

    print()
    print(f"Total: {len(grid)}  Hangs: {len(hangs)}  Bad results: {len(bad_results)}")
    if hangs:
        print("\n=== First HANG (truncated): ===")
        p, f, out = hangs[0]
        print(f"PERIOD={p} FIRST={f}")
        print(out[-3000:])
    if bad_results:
        print("\n=== First BAD RESULT (truncated): ===")
        p, f, r15, out = bad_results[0]
        print(f"PERIOD={p} FIRST={f} r15={r15}")
        print(out[-3000:])

    sys.exit(0 if not (hangs or bad_results) else 2)


if __name__ == "__main__":
    main()
