"""
Modern C Compiler Test Suite (cproc + QBE)

This script automatically tests the modern C toolchain by:
1. Compiling all .c files in Tests/C/ using cproc + QBE + crt0
2. Preparing the compiled code for Verilog simulation
3. Running the Verilog simulation
4. Extracting the UART output from the simulation
5. Comparing against the expected value (from // expected=0xXX comments)

Usage:
    python3 Scripts/Tests/c_tests.py
    python3 Scripts/Tests/c_tests.py 01_return/return_constant.c

    Or via Makefile:
    make test-c
"""

import os
import subprocess
import sys
import logging
import re
import shutil
from concurrent.futures import ProcessPoolExecutor, as_completed
from dataclasses import dataclass
from typing import Optional


# Configure colored logging
class ColoredFormatter(logging.Formatter):
    COLORS = {
        "WARNING": "\033[93m",
        "ERROR": "\033[91m",
        "RESET": "\033[0m",
    }

    def format(self, record):
        if record.levelname in self.COLORS:
            record.msg = (
                f"{self.COLORS[record.levelname]}{record.msg}{self.COLORS['RESET']}"
            )
        return super().format(record)


logger = logging.getLogger(__name__)
handler = logging.StreamHandler()
handler.setFormatter(ColoredFormatter("%(message)s"))
logger.addHandler(handler)
logger.setLevel(logging.INFO)


@dataclass
class CTestConfig:
    """Configuration constants for modern C tests."""

    TESTS_DIRECTORY: str = "Tests/C"
    TMP_DIRECTORY: str = "Tests/C/tmp"
    ROM_LIST_PATH: str = "Hardware/FPGA/Verilog/Simulation/MemoryLists/rom.list"
    RAM_LIST_PATH: str = "Hardware/FPGA/Verilog/Simulation/MemoryLists/ram.list"
    SDRAM_INIT_LIST_PATH: str = (
        "Hardware/FPGA/Verilog/Simulation/MemoryLists/sdram.list"
    )
    BOOTLOADER_ROM_PATH: str = "Software/ASM/Simulation/sim_jump_to_ram.asm"
    TESTBENCH_PATH: str = "Hardware/FPGA/Verilog/Simulation/cpu_tests_tb.v"
    VERILOG_OUTPUT_PATH: str = "Hardware/FPGA/Verilog/Simulation/Output/cpu.out"
    MEMORY_LISTS_DIR: str = "Hardware/FPGA/Verilog/Simulation/MemoryLists"

    COMPILE_SCRIPT: str = "Scripts/BCC/compile_modern_c.sh"
    CRT0_PATH: str = "Software/ASM/crt0/crt0_baremetal.asm"
    CONVERTER_SCRIPT: str = "Scripts/Simulation/convert_to_256_bit.py"
    ROM_OFFSET: str = "0x1E000000"

    PARALLEL_TMP_DIR: str = "Tests/tmp"


class CTestError(Exception):
    pass


class CompilerError(CTestError):
    pass


class AssemblerError(CTestError):
    pass


class SimulationError(CTestError):
    pass


class ResultParsingError(CTestError):
    pass


class CTestRunner:
    """Main class for running modern C tests."""

    def __init__(
        self,
        config: CTestConfig = None,
        temp_dir: Optional[str] = None,
    ):
        self.config = config or CTestConfig()
        self.temp_dir = temp_dir

        if temp_dir:
            self._setup_temp_paths()

    def _setup_temp_paths(self):
        self.config.TMP_DIRECTORY = self.temp_dir
        self.config.ROM_LIST_PATH = os.path.join(self.temp_dir, "rom.list")
        self.config.RAM_LIST_PATH = os.path.join(self.temp_dir, "ram.list")
        self.config.SDRAM_INIT_LIST_PATH = os.path.join(self.temp_dir, "sdram.list")
        self.config.VERILOG_OUTPUT_PATH = os.path.join(self.temp_dir, "cpu.out")
        self.testbench_path = os.path.join(self.temp_dir, "cpu_tests_tb.v")

    def _prepare_temp_testbench(self):
        with open(self.config.TESTBENCH_PATH, "r") as f:
            content = f.read()

        old_base = "Hardware/FPGA/Verilog/Simulation/MemoryLists"
        content = content.replace(old_base, self.temp_dir)

        with open(self.testbench_path, "w") as f:
            f.write(content)

        static_files = [
            "vram32.list",
            "vram8.list",
            "vramPX.list",
            "spiflash1.list",
            "spiflash2.list",
        ]
        for filename in static_files:
            src = os.path.join(self.config.MEMORY_LISTS_DIR, filename)
            dst = os.path.join(self.temp_dir, filename)
            if os.path.exists(src):
                shutil.copy(src, dst)

    def _run_command(
        self, command: str, description: str, cwd: Optional[str] = None
    ) -> tuple[int, str]:
        logger.debug(f"Running command: {command}")
        try:
            result = subprocess.run(
                command,
                shell=True,
                capture_output=True,
                text=True,
                timeout=60,
                cwd=cwd,
            )
            return result.returncode, result.stdout + result.stderr
        except subprocess.TimeoutExpired:
            raise CTestError(f"Command timed out: {description}")
        except Exception as e:
            raise CTestError(f"Failed to execute command '{description}': {e}")

    def _parse_simulation_result(self, result: str) -> int:
        lines = result.split("\n")
        uart_pattern = re.compile(r"UART TX:\s*([0-9a-fA-F]+)")

        for line in lines:
            match = uart_pattern.search(line)
            if match:
                try:
                    return int(match.group(1), 16)
                except (ValueError, IndexError) as e:
                    raise ResultParsingError(
                        f"Failed to parse UART value from line: {line}"
                    ) from e

        raise ResultParsingError("No UART transmission found in simulation output")

    def _run_simulation(self) -> str:
        testbench = self.testbench_path if self.temp_dir else self.config.TESTBENCH_PATH

        compile_cmd = f"iverilog -o {self.config.VERILOG_OUTPUT_PATH} {testbench}"
        exit_code, output = self._run_command(compile_cmd, "Compiling testbench")
        if exit_code != 0:
            raise SimulationError(f"Failed to compile testbench: {output}")

        sim_cmd = f"vvp {self.config.VERILOG_OUTPUT_PATH}"
        exit_code, output = self._run_command(sim_cmd, "Running simulation")
        if exit_code != 0:
            raise SimulationError(f"Simulation failed: {output}")

        return output

    def _get_extra_sources(self, test_lines: list[str]) -> list[str]:
        """Parse // extra_sources=path1,path2 directive from test file."""
        pattern = re.compile(r"//\s*extra_sources\s*=\s*(.+)", re.IGNORECASE)
        for line in test_lines:
            match = pattern.search(line)
            if match:
                return [s.strip() for s in match.group(1).split(",") if s.strip()]
        return []

    def _get_compile_flags(self, test_lines: list[str]) -> str:
        """Parse // compile_flags=... directive from test file."""
        pattern = re.compile(r"//\s*compile_flags\s*=\s*(.+)", re.IGNORECASE)
        for line in test_lines:
            match = pattern.search(line)
            if match:
                return match.group(1).strip()
        return ""

    def _compile_modern_c(
        self,
        c_path: str,
        list_path: str,
        extra_sources: list[str] = None,
        compile_flags: str = "",
    ) -> None:
        """Compile C code using the modern toolchain (cproc + QBE + crt0)."""
        sources = f"{self.config.CRT0_PATH} {c_path}"
        if extra_sources:
            sources += " " + " ".join(extra_sources)
        compile_cmd = (
            f"{self.config.COMPILE_SCRIPT} "
            f"{sources} "
            f"-h {compile_flags} -o {list_path.replace('.list', '.bin')}"
        )
        exit_code, output = self._run_command(
            compile_cmd, f"Compiling {c_path} (modern C)"
        )
        if exit_code != 0:
            raise CompilerError(f"Modern C compilation failed for {c_path}: {output}")

    def _prepare_simulation_environment(self, list_path: str) -> None:
        """Prepare simulation: ROM bootloader + SDRAM from compiled test."""
        # Assemble ROM bootloader
        assemble_cmd = (
            f"asmpy {self.config.BOOTLOADER_ROM_PATH} "
            f"{self.config.ROM_LIST_PATH} -o {self.config.ROM_OFFSET}"
        )
        exit_code, output = self._run_command(assemble_cmd, "Assembling ROM bootloader")
        if exit_code != 0:
            raise AssemblerError(f"ROM bootloader assembly failed: {output}")

        # Copy compiled list to RAM list path
        shutil.copy(list_path, self.config.RAM_LIST_PATH)

        # Convert to 256-bit format for SDRAM
        convert_cmd = (
            f"python3 {self.config.CONVERTER_SCRIPT} "
            f"{self.config.RAM_LIST_PATH} {self.config.SDRAM_INIT_LIST_PATH}"
        )
        exit_code, output = self._run_command(
            convert_cmd, "Converting to 256-bit format"
        )
        if exit_code != 0:
            raise AssemblerError(f"Failed to convert to 256-bit format: {output}")

    def _get_expected_value(self, test_lines: list[str]) -> int:
        expected_pattern = re.compile(
            r"//\s*expected\s*=\s*(0x[0-9a-fA-F]+)", re.IGNORECASE
        )
        for line in test_lines:
            match = expected_pattern.search(line)
            if match:
                try:
                    return int(match.group(1), 16)
                except (ValueError, IndexError) as e:
                    raise CTestError(
                        f"Invalid expected value format in line: {line}"
                    ) from e
        raise CTestError("No expected value found in test file")

    def run_single_test(self, test_file: str) -> None:
        test_path = os.path.join(self.config.TESTS_DIRECTORY, test_file)

        # Read expected value
        try:
            with open(test_path, "r") as file:
                lines = file.readlines()
        except FileNotFoundError:
            raise CTestError(f"Test file not found: {test_path}")

        expected_value = self._get_expected_value(lines)
        extra_sources = self._get_extra_sources(lines)
        compile_flags = self._get_compile_flags(lines)

        # Prepare temp testbench if in isolated mode
        if self.temp_dir:
            self._prepare_temp_testbench()

        # Compile using modern C toolchain
        base_name = os.path.splitext(test_file)[0].replace(os.sep, "_")
        list_path = os.path.join(self.config.TMP_DIRECTORY, f"{base_name}.list")
        self._compile_modern_c(test_path, list_path, extra_sources, compile_flags)

        # Prepare simulation environment
        self._prepare_simulation_environment(list_path)

        # Run simulation
        simulation_output = self._run_simulation()

        # Parse and check result
        resulting_value = self._parse_simulation_result(simulation_output)
        if resulting_value != expected_value:
            raise CTestError(
                f"Expected 0x{expected_value:02X}, got 0x{resulting_value:02X}"
            )

    def get_test_files(self) -> list[str]:
        try:
            tests = []
            for root, dirs, files in os.walk(self.config.TESTS_DIRECTORY):
                dirs[:] = [d for d in dirs if d not in ("tmp",)]
                for file in files:
                    if file.endswith(".c"):
                        full_path = os.path.join(root, file)
                        rel_path = os.path.relpath(
                            full_path, self.config.TESTS_DIRECTORY
                        )
                        tests.append(rel_path)
            return sorted(tests)
        except FileNotFoundError:
            raise CTestError(
                f"Tests directory not found: {self.config.TESTS_DIRECTORY}"
            )

    def run_tests(self) -> tuple[list[str], list[tuple[str, str]]]:
        GREEN = "\033[92m"
        RED = "\033[91m"
        RESET = "\033[0m"

        print("Running modern C compiler tests...\n")
        os.makedirs(self.config.TMP_DIRECTORY, exist_ok=True)

        tests = self.get_test_files()
        total = len(tests)
        passed_tests: list[str] = []
        failed_tests: list[tuple[str, str]] = []
        completed = 0

        for test in tests:
            completed += 1
            try:
                self.run_single_test(test)
                print(f"{GREEN}.{RESET}", end="", flush=True)
                passed_tests.append(test)
            except Exception as e:
                print(f"{RED}F{RESET}", end="", flush=True)
                failed_tests.append((test, str(e)))

            if completed % 50 == 0:
                print(f" [{completed}/{total}]")

        if completed % 50 != 0:
            print(f" [{completed}/{total}]")

        return passed_tests, failed_tests


def _run_single_test_parallel(args: tuple) -> tuple[str, bool, str]:
    test_file, temp_base_dir, test_index = args
    temp_dir = os.path.join(temp_base_dir, f"test_{test_index}")
    os.makedirs(temp_dir, exist_ok=True)

    try:
        runner = CTestRunner(temp_dir=temp_dir)
        runner.run_single_test(test_file)
        return (test_file, True, "")
    except Exception as e:
        return (test_file, False, str(e))
    finally:
        try:
            shutil.rmtree(temp_dir)
        except Exception:
            pass


def _display_results_grouped(
    passed: list[str], failed: list[tuple[str, str]], total_tests: int
) -> None:
    GREEN = "\033[92m"
    RED = "\033[91m"
    YELLOW = "\033[93m"
    BOLD = "\033[1m"
    RESET = "\033[0m"

    from collections import defaultdict

    categories: dict[str, dict] = defaultdict(lambda: {"passed": [], "failed": []})

    for test in passed:
        cat = test.split(os.sep)[0] if os.sep in test else "root"
        categories[cat]["passed"].append(test)

    for test, error in failed:
        cat = test.split(os.sep)[0] if os.sep in test else "root"
        categories[cat]["failed"].append((test, error))

    print(f"\n{BOLD}{'=' * 60}{RESET}")
    print(f"{BOLD}TEST RESULTS{RESET}")
    print(f"{'=' * 60}\n")

    for cat in sorted(categories.keys()):
        cat_passed = categories[cat]["passed"]
        cat_failed = categories[cat]["failed"]
        cat_total = len(cat_passed) + len(cat_failed)

        if cat_failed:
            status_color = RED if len(cat_passed) == 0 else YELLOW
        else:
            status_color = GREEN

        cat_display = cat.replace("_", " ").title()
        print(
            f"{status_color}{BOLD}{cat_display}{RESET} "
            f"[{GREEN}{len(cat_passed)}{RESET}/{cat_total}]"
        )

        for test in sorted(cat_passed):
            name = os.path.basename(test).replace(".c", "")
            print(f"  {GREEN}✓{RESET} {name}")

        for test, error in sorted(cat_failed):
            name = os.path.basename(test).replace(".c", "")
            short_error = error.split("\n")[0][:127]
            print(f"  {RED}✗{RESET} {name}: {short_error}")

        print()

    print(f"{'=' * 60}")
    if len(failed) == 0:
        print(f"{GREEN}{BOLD}All {len(passed)} tests passed!{RESET}")
    else:
        print(
            f"{BOLD}{GREEN}{len(passed)} passed{RESET}, "
            f"{BOLD}{RED}{len(failed)} failed{RESET} "
            f"(out of {total_tests} tests)"
        )
    print(f"{'=' * 60}\n")


class ParallelCTestRunner:
    DEFAULT_WORKERS = int(os.environ.get("FPGC_TEST_WORKERS", 4))

    def __init__(self, max_workers: Optional[int] = None):
        self.max_workers = max_workers or self.DEFAULT_WORKERS
        self.config = CTestConfig()

    def run_tests_parallel(self) -> tuple[list[str], list[tuple[str, str]]]:
        GREEN = "\033[92m"
        RED = "\033[91m"
        RESET = "\033[0m"

        print(
            f"Running modern C compiler tests in parallel "
            f"({self.max_workers} workers)...\n"
        )

        temp_base_dir = os.path.abspath(self.config.PARALLEL_TMP_DIR)
        os.makedirs(temp_base_dir, exist_ok=True)

        runner = CTestRunner()
        tests = runner.get_test_files()
        total = len(tests)

        test_args = [(test, temp_base_dir, i) for i, test in enumerate(tests)]

        passed_tests: list[str] = []
        failed_tests: list[tuple[str, str]] = []
        completed = 0

        try:
            with ProcessPoolExecutor(max_workers=self.max_workers) as executor:
                futures = {
                    executor.submit(_run_single_test_parallel, args): args[0]
                    for args in test_args
                }

                for future in as_completed(futures):
                    test_file, passed, error_msg = future.result()
                    completed += 1

                    if passed:
                        print(f"{GREEN}.{RESET}", end="", flush=True)
                        passed_tests.append(test_file)
                    else:
                        print(f"{RED}F{RESET}", end="", flush=True)
                        failed_tests.append((test_file, error_msg))

                    if completed % 50 == 0:
                        print(f" [{completed}/{total}]")

            if completed % 50 != 0:
                print(f" [{completed}/{total}]")

        finally:
            try:
                if os.path.exists(temp_base_dir) and not os.listdir(temp_base_dir):
                    os.rmdir(temp_base_dir)
            except Exception:
                pass

        return sorted(passed_tests), sorted(failed_tests)


def main() -> None:
    import argparse

    parser = argparse.ArgumentParser(description="Modern C Compiler Test Suite")
    parser.add_argument(
        "--workers",
        type=int,
        default=None,
        help="Number of parallel workers (default: 4)",
    )
    parser.add_argument(
        "test_file",
        nargs="?",
        help="Specific test file to run (e.g., 01_return/return_constant.c)",
    )
    args = parser.parse_args()

    if args.test_file:
        runner = CTestRunner()
        try:
            os.makedirs(runner.config.TMP_DIRECTORY, exist_ok=True)
            runner.run_single_test(args.test_file)
            logger.info(f"PASS: {args.test_file}")
        except Exception as e:
            logger.error(f"FAIL: {args.test_file} -> {e}")
            sys.exit(1)
    else:
        runner = ParallelCTestRunner(max_workers=args.workers)
        passed, failed = runner.run_tests_parallel()
        total = len(passed) + len(failed)
        _display_results_grouped(passed, failed, total)
        if failed:
            sys.exit(1)


if __name__ == "__main__":
    main()
