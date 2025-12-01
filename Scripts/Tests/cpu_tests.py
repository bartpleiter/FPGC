import os
import subprocess
import sys
import logging
import shutil
from concurrent.futures import ProcessPoolExecutor, as_completed
from dataclasses import dataclass
from typing import Optional


# Configure colored logging
class ColoredFormatter(logging.Formatter):
    """Custom formatter with colored output for different log levels."""

    COLORS = {
        "WARNING": "\033[93m",  # Yellow
        "ERROR": "\033[91m",  # Red
        "RESET": "\033[0m",  # Reset to default
    }

    def format(self, record):
        if record.levelname in self.COLORS:
            record.msg = (
                f"{self.COLORS[record.levelname]}{record.msg}{self.COLORS['RESET']}"
            )
        return super().format(record)


# Configure logging
logger = logging.getLogger(__name__)
handler = logging.StreamHandler()
handler.setFormatter(ColoredFormatter("%(message)s"))
logger.addHandler(handler)
logger.setLevel(logging.INFO)


@dataclass
class CPUTestConfig:
    """Configuration constants for CPU tests."""

    TESTS_DIRECTORY: str = "Tests/CPU"
    ROM_LIST_PATH: str = "Hardware/FPGA/Verilog/Simulation/MemoryLists/rom.list"
    RAM_LIST_PATH: str = "Hardware/FPGA/Verilog/Simulation/MemoryLists/ram.list"
    MIG7MOCK_LIST_PATH: str = (
        "Hardware/FPGA/Verilog/Simulation/MemoryLists/mig7mock.list"
    )
    BOOTLOADER_ROM_PATH: str = "Software/BareMetalASM/Simulation/sim_jump_to_ram.asm"
    TESTBENCH_PATH: str = "Hardware/FPGA/Verilog/Simulation/cpu_tests_tb.v"
    VERILOG_OUTPUT_PATH: str = "Hardware/FPGA/Verilog/Simulation/Output/cpu.out"
    MEMORY_LISTS_DIR: str = "Hardware/FPGA/Verilog/Simulation/MemoryLists"

    CONVERTER_SCRIPT: str = "Scripts/Simulation/convert_to_256_bit.py"

    # Parallel test temp directory
    PARALLEL_TMP_DIR: str = "Tests/tmp"


class CPUTestError(Exception):
    """Custom exception for CPU test related errors."""

    pass


class AssemblerError(CPUTestError):
    """Exception raised when assembler fails."""

    pass


class SimulationError(CPUTestError):
    """Exception raised when simulation fails."""

    pass


class ResultParsingError(CPUTestError):
    """Exception raised when result parsing fails."""

    pass


class CPUTestRunner:
    """Main class for running CPU tests with improved error handling and logging."""

    def __init__(self, config: CPUTestConfig = None, temp_dir: Optional[str] = None):
        """Initialize the test runner with configuration.

        Args:
            config: Configuration for the test runner
            temp_dir: Optional temporary directory for isolated test execution
        """
        self.config = config or CPUTestConfig()
        self.temp_dir = temp_dir

        # If using temp_dir, override paths for isolation
        if temp_dir:
            self._setup_temp_paths()

    def _setup_temp_paths(self):
        """Set up paths for isolated execution in temp directory."""
        self.config.ROM_LIST_PATH = os.path.join(self.temp_dir, "rom.list")
        self.config.RAM_LIST_PATH = os.path.join(self.temp_dir, "ram.list")
        self.config.MIG7MOCK_LIST_PATH = os.path.join(self.temp_dir, "mig7mock.list")
        self.config.VERILOG_OUTPUT_PATH = os.path.join(self.temp_dir, "cpu.out")
        self.testbench_path = os.path.join(self.temp_dir, "cpu_tests_tb.v")

    def _prepare_temp_testbench(self):
        """Create a modified testbench with paths pointing to temp directory."""
        with open(self.config.TESTBENCH_PATH, "r") as f:
            content = f.read()

        # Replace all MemoryLists paths with temp directory paths
        # The testbench uses relative paths from project root
        old_base = "Hardware/FPGA/Verilog/Simulation/MemoryLists"
        content = content.replace(old_base, self.temp_dir)

        with open(self.testbench_path, "w") as f:
            f.write(content)

        # Copy static memory list files that don't change (vram, spiflash)
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

    def _run_command(self, command: str, description: str) -> tuple[int, str]:
        """
        Run a shell command and return the exit code and output.

        Args:
            command: The command to execute
            description: Description of what the command does for logging

        Returns:
            Tuple of (exit_code, output)

        Raises:
            CPUTestError: If command execution fails
        """
        logger.debug(f"Running command: {command}")
        try:
            result = subprocess.run(
                command, shell=True, capture_output=True, text=True, timeout=60
            )
            return result.returncode, result.stdout + result.stderr
        except subprocess.TimeoutExpired:
            raise CPUTestError(f"Command timed out: {description}")
        except Exception as e:
            raise CPUTestError(f"Failed to execute command '{description}': {e}")

    def _parse_simulation_result(self, result: str) -> int:
        """
        Parse the simulation result to extract the final register value.

        Args:
            result: Raw simulation output

        Returns:
            The value of register 15

        Raises:
            ResultParsingError: If no result is found
        """
        lines = result.split("\n")
        last_reg15_line = None

        # Find the last occurrence of "reg r15:" (new format) or "reg15 :=" (old format)
        for line in lines:
            if "reg r15:" in line or "reg15 :=" in line:
                last_reg15_line = line

        if last_reg15_line is None:
            raise ResultParsingError("No result found in simulation output")

        try:
            # Handle new format: "1234.0 ns reg r15: 123"
            if "reg r15:" in last_reg15_line:
                return int(last_reg15_line.split("reg r15:")[1].strip())
            # Handle old format: "... reg15 := 123"
            else:
                return int(last_reg15_line.split("reg15 :=")[1].strip())
        except (ValueError, IndexError) as e:
            raise ResultParsingError(
                f"Failed to parse register value from line: {last_reg15_line}"
            ) from e

    def _run_simulation(self) -> str:
        """
        Run the Verilog simulation.

        Returns:
            Simulation output

        Raises:
            SimulationError: If simulation fails
        """
        # Use temp testbench if in temp directory mode
        testbench = self.testbench_path if self.temp_dir else self.config.TESTBENCH_PATH

        # Compile testbench
        compile_cmd = f"iverilog -o {self.config.VERILOG_OUTPUT_PATH} {testbench}"
        exit_code, output = self._run_command(compile_cmd, "Compiling testbench")

        if exit_code != 0:
            raise SimulationError(f"Failed to compile testbench: {output}")

        # Run simulation
        sim_cmd = f"vvp {self.config.VERILOG_OUTPUT_PATH}"
        exit_code, output = self._run_command(sim_cmd, "Running simulation")

        if exit_code != 0:
            raise SimulationError(f"Simulation failed: {output}")

        return output

    def _assemble_code(
        self, source_path: str, output_path: str, description: str
    ) -> None:
        """
        Assemble assembly code to a list file.

        Args:
            source_path: Path to source assembly file
            output_path: Path to output list file
            description: Description for logging

        Raises:
            AssemblerError: If assembly fails
        """
        # Run asmpy assembler directly
        assemble_cmd = f"asmpy {source_path} {output_path}"
        exit_code, output = self._run_command(assemble_cmd, f"Assembling {description}")

        if exit_code != 0:
            raise AssemblerError(f"Assembler failed for {description}: {output}")

    def _assemble_code_to_rom(self, path: str) -> None:
        """Assemble code for ROM execution."""
        self._assemble_code(path, self.config.ROM_LIST_PATH, "ROM code")

    def _assemble_code_to_ram(self, path: str) -> None:
        """Assemble code for RAM execution."""
        # First, set up the ROM with bootloader mock
        self._assemble_code(
            self.config.BOOTLOADER_ROM_PATH, self.config.ROM_LIST_PATH, "ROM bootloader"
        )

        # Now compile the test code for RAM
        self._assemble_code(path, self.config.RAM_LIST_PATH, "RAM code")

        # Convert to 256 bit lines for mig7 mock
        convert_cmd = f"python3 {self.config.CONVERTER_SCRIPT} {self.config.RAM_LIST_PATH} {self.config.MIG7MOCK_LIST_PATH}"
        exit_code, output = self._run_command(
            convert_cmd, "Converting to 256-bit format"
        )

        if exit_code != 0:
            raise AssemblerError(f"Failed to convert to 256-bit format: {output}")

    def _get_expected_value(self, test_lines: list[str]) -> int:
        """
        Extract expected value from test file lines.

        Args:
            test_lines: Lines from the test file

        Returns:
            Expected test result value

        Raises:
            CPUTestError: If no expected value is found
        """
        for line in test_lines:
            if "expected=" in line:
                try:
                    return int(line.split("expected=")[1].strip())
                except (ValueError, IndexError) as e:
                    raise CPUTestError(
                        f"Invalid expected value format in line: {line}"
                    ) from e

        raise CPUTestError("No expected value found in test file")

    def run_single_test(self, test_file: str, use_ram: bool = False) -> None:
        """
        Run a single test.

        Args:
            test_file: Name of the test file
            use_ram: Whether to run the test from RAM

        Raises:
            CPUTestError: If test fails
        """
        test_path = os.path.join(self.config.TESTS_DIRECTORY, test_file)

        try:
            with open(test_path, "r") as file:
                lines = file.readlines()
        except FileNotFoundError:
            raise CPUTestError(f"Test file not found: {test_path}")
        except Exception as e:
            raise CPUTestError(f"Failed to read test file {test_path}: {e}")

        expected_value = self._get_expected_value(lines)

        # Prepare temp testbench if running in isolated mode
        if self.temp_dir:
            self._prepare_temp_testbench()

        if use_ram:
            self._assemble_code_to_ram(test_path)
        else:
            self._assemble_code_to_rom(test_path)

        simulation_output = self._run_simulation()
        resulting_value = self._parse_simulation_result(simulation_output)

        if resulting_value != expected_value:
            raise CPUTestError(f"Expected {expected_value}, got {resulting_value}")

    def get_test_files(self) -> list[str]:
        """
        Get a sorted list of test files, recursively searching subdirectories.

        Returns:
            List of test file paths relative to TESTS_DIRECTORY

        Raises:
            CPUTestError: If tests directory cannot be read
        """
        try:
            tests = []
            for root, dirs, files in os.walk(self.config.TESTS_DIRECTORY):
                # Skip 'tmp' directory
                dirs[:] = [d for d in dirs if d not in ("tmp",)]
                for file in files:
                    if file.endswith(".asm"):
                        # Get path relative to TESTS_DIRECTORY
                        full_path = os.path.join(root, file)
                        rel_path = os.path.relpath(
                            full_path, self.config.TESTS_DIRECTORY
                        )
                        tests.append(rel_path)
            return sorted(tests)
        except FileNotFoundError:
            raise CPUTestError(
                f"Tests directory not found: {self.config.TESTS_DIRECTORY}"
            )
        except Exception as e:
            raise CPUTestError(f"Failed to read tests directory: {e}")

    def run_tests(
        self, use_ram: bool = False
    ) -> tuple[list[str], list[tuple[str, str]]]:
        """
        Run all tests.

        Args:
            use_ram: Whether to run tests from RAM

        Returns:
            Tuple of (passed_tests, failed_tests_with_errors)
        """
        GREEN = "\033[92m"
        RED = "\033[91m"
        RESET = "\033[0m"

        memory_type = "RAM" if use_ram else "ROM"
        print(f"Running CPU tests from {memory_type}...\n")

        tests = self.get_test_files()
        total = len(tests)
        passed_tests: list[str] = []
        failed_tests: list[tuple[str, str]] = []
        completed = 0

        for test in tests:
            completed += 1
            try:
                self.run_single_test(test, use_ram=use_ram)
                print(f"{GREEN}.{RESET}", end="", flush=True)
                passed_tests.append(test)
            except Exception as e:
                print(f"{RED}F{RESET}", end="", flush=True)
                failed_tests.append((test, str(e)))

            # Line break every 50 tests
            if completed % 50 == 0:
                print(f" [{completed}/{total}]")

        # Final line break if needed
        if completed % 50 != 0:
            print(f" [{completed}/{total}]")

        return passed_tests, failed_tests

    def run_tests_from_rom(self) -> list[tuple[str, str]]:
        """Run all tests from ROM and display results.

        Returns:
            List of failed tests with errors.
        """
        passed, failed = self.run_tests(use_ram=False)
        total = len(passed) + len(failed)
        _display_results_grouped(passed, failed, total, "ROM")
        return failed

    def run_tests_from_ram(self) -> list[tuple[str, str]]:
        """Run all tests from RAM and display results.

        Returns:
            List of failed tests with errors.
        """
        passed, failed = self.run_tests(use_ram=True)
        total = len(passed) + len(failed)
        _display_results_grouped(passed, failed, total, "RAM")
        return failed


def _run_single_test_parallel(args: tuple) -> tuple[str, bool, str]:
    """
    Run a single test in isolation for parallel execution.

    Args:
        args: Tuple of (test_file, use_ram, temp_base_dir, test_index)

    Returns:
        Tuple of (test_file, passed, error_message)
    """
    test_file, use_ram, temp_base_dir, test_index = args

    # Create a unique temp directory for this test
    temp_dir = os.path.join(temp_base_dir, f"test_{test_index}")
    os.makedirs(temp_dir, exist_ok=True)

    try:
        runner = CPUTestRunner(temp_dir=temp_dir)
        runner.run_single_test(test_file, use_ram=use_ram)
        return (test_file, True, "")
    except Exception as e:
        return (test_file, False, str(e))
    finally:
        # Clean up temp directory
        try:
            shutil.rmtree(temp_dir)
        except Exception:
            pass


def _display_results_grouped(
    passed: list[str], failed: list[tuple[str, str]], total_tests: int, memory_type: str
) -> None:
    """Display test results grouped by category with pytest-style formatting."""
    GREEN = "\033[92m"
    RED = "\033[91m"
    YELLOW = "\033[93m"
    BOLD = "\033[1m"
    RESET = "\033[0m"

    # Group results by category (first directory component)
    from collections import defaultdict

    categories: dict[str, dict] = defaultdict(lambda: {"passed": [], "failed": []})

    for test in passed:
        cat = test.split(os.sep)[0] if os.sep in test else "root"
        categories[cat]["passed"].append(test)

    for test, error in failed:
        cat = test.split(os.sep)[0] if os.sep in test else "root"
        categories[cat]["failed"].append((test, error))

    # Print grouped results
    print(f"\n{BOLD}{'=' * 60}{RESET}")
    print(f"{BOLD}CPU TEST RESULTS ({memory_type}){RESET}")
    print(f"{'=' * 60}\n")

    for cat in sorted(categories.keys()):
        cat_passed = categories[cat]["passed"]
        cat_failed = categories[cat]["failed"]
        cat_total = len(cat_passed) + len(cat_failed)

        # Category header with pass/fail counts
        if cat_failed:
            status_color = RED if len(cat_passed) == 0 else YELLOW
        else:
            status_color = GREEN

        cat_display = cat.replace("_", " ").title()
        print(
            f"{status_color}{BOLD}{cat_display}{RESET} "
            f"[{GREEN}{len(cat_passed)}{RESET}/{cat_total}]"
        )

        # Show individual test results (compact)
        for test in sorted(cat_passed):
            name = os.path.basename(test).replace(".asm", "")
            print(f"  {GREEN}✓{RESET} {name}")

        for test, error in sorted(cat_failed):
            name = os.path.basename(test).replace(".asm", "")
            short_error = error.split("\n")[0][:50]
            print(f"  {RED}✗{RESET} {name}: {short_error}")

        print()

    # Summary line
    passed_count = len(passed)
    failed_count = len(failed)
    print(f"{'=' * 60}")
    if failed_count == 0:
        print(f"{GREEN}{BOLD}All {passed_count} tests passed!{RESET}")
    else:
        print(
            f"{BOLD}{GREEN}{passed_count} passed{RESET}, "
            f"{BOLD}{RED}{failed_count} failed{RESET} "
            f"(out of {total_tests} tests)"
        )
    print(f"{'=' * 60}\n")


def _display_results_combined(
    rom_results: dict[str, tuple[bool, str]],
    ram_results: dict[str, tuple[bool, str]],
) -> int:
    """Display combined ROM and RAM test results in a single overview.

    Args:
        rom_results: Dict mapping test path to (passed, error_msg) for ROM tests
        ram_results: Dict mapping test path to (passed, error_msg) for RAM tests

    Returns:
        Number of failed test scenarios (each test counts as 2: ROM + RAM)
    """
    GREEN = "\033[92m"
    RED = "\033[91m"
    YELLOW = "\033[93m"
    BOLD = "\033[1m"
    RESET = "\033[0m"

    from collections import defaultdict

    # Get all test files
    all_tests = sorted(set(rom_results.keys()) | set(ram_results.keys()))

    # Group by category
    categories: dict[str, list[str]] = defaultdict(list)
    for test in all_tests:
        cat = test.split(os.sep)[0] if os.sep in test else "root"
        categories[cat].append(test)

    # Print header
    print(f"\n{BOLD}{'=' * 70}{RESET}")
    print(f"{BOLD}CPU TEST RESULTS (ROM + RAM){RESET}")
    print(f"{'=' * 70}\n")

    total_scenarios = 0
    passed_scenarios = 0
    failed_details: list[tuple[str, str, str]] = []  # (test, type, error)

    for cat in sorted(categories.keys()):
        cat_tests = sorted(categories[cat])
        cat_all_pass = True
        cat_any_pass = False

        # Check category status
        for test in cat_tests:
            rom_pass = rom_results.get(test, (False, ""))[0]
            ram_pass = ram_results.get(test, (False, ""))[0]
            if rom_pass and ram_pass:
                cat_any_pass = True
            else:
                cat_all_pass = False
                if rom_pass or ram_pass:
                    cat_any_pass = True

        # Category header
        if cat_all_pass:
            status_color = GREEN
        elif cat_any_pass:
            status_color = YELLOW
        else:
            status_color = RED

        cat_display = cat.replace("_", " ").title()
        print(f"{status_color}{BOLD}{cat_display}{RESET}")

        # Show individual test results
        for test in cat_tests:
            name = os.path.basename(test).replace(".asm", "")
            rom_pass, rom_err = rom_results.get(test, (False, "Not run"))
            ram_pass, ram_err = ram_results.get(test, (False, "Not run"))

            # Track statistics
            total_scenarios += 2
            if rom_pass:
                passed_scenarios += 1
            else:
                failed_details.append((test, "ROM", rom_err))
            if ram_pass:
                passed_scenarios += 1
            else:
                failed_details.append((test, "RAM", ram_err))

            # Format status indicators
            rom_status = f"{GREEN}✓ROM{RESET}" if rom_pass else f"{RED}✗ROM{RESET}"
            ram_status = f"{GREEN}✓RAM{RESET}" if ram_pass else f"{RED}✗RAM{RESET}"

            print(f"  {rom_status} {ram_status}  {name}")

        print()

    # Summary
    failed_scenarios = total_scenarios - passed_scenarios
    total_tests = len(all_tests)

    print(f"{'=' * 70}")

    if failed_scenarios == 0:
        print(f"{GREEN}{BOLD}All {total_tests} tests passed both ROM and RAM!{RESET}")
    else:
        print(
            f"{BOLD}{GREEN}{passed_scenarios} passed{RESET}, "
            f"{BOLD}{RED}{failed_scenarios} failed{RESET} "
            f"(across {total_tests} tests × 2 scenarios)"
        )

        # Show failed test details
        if failed_details:
            print(f"\n{RED}{BOLD}Failed tests:{RESET}")
            for test, mem_type, error in failed_details:
                short_error = error.split("\n")[0][:50] if error else "Unknown error"
                print(f"  {RED}✗{RESET} {test} ({mem_type}): {short_error}")

    print(f"{'=' * 70}\n")

    return failed_scenarios


class ParallelCPUTestRunner:
    """Test runner that executes tests in parallel."""

    # Default number of parallel workers, make sure you have enough RAM if increasing
    # Can be overridden via FPGC_TEST_WORKERS environment variable
    DEFAULT_WORKERS = int(os.environ.get("FPGC_TEST_WORKERS", 4))

    def __init__(self, max_workers: Optional[int] = None):
        """
        Initialize parallel test runner.

        Args:
            max_workers: Maximum number of parallel workers. Defaults to 4.
        """
        self.max_workers = max_workers or self.DEFAULT_WORKERS
        self.config = CPUTestConfig()

    def run_tests_parallel(
        self, use_ram: bool = False
    ) -> tuple[list[str], list[tuple[str, str]]]:
        """
        Run all tests in parallel.

        Args:
            use_ram: Whether to run tests from RAM

        Returns:
            Tuple of (passed_tests, failed_tests_with_errors)
        """
        GREEN = "\033[92m"
        RED = "\033[91m"
        RESET = "\033[0m"

        memory_type = "RAM" if use_ram else "ROM"
        print(
            f"Running CPU tests from {memory_type} in parallel ({self.max_workers} workers)...\n"
        )

        # Create base temp directory
        temp_base_dir = os.path.abspath(self.config.PARALLEL_TMP_DIR)
        os.makedirs(temp_base_dir, exist_ok=True)

        runner = CPUTestRunner()
        tests = runner.get_test_files()
        total = len(tests)

        # Prepare arguments for parallel execution
        test_args = [(test, use_ram, temp_base_dir, i) for i, test in enumerate(tests)]

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

                    # Print progress dot
                    if passed:
                        print(f"{GREEN}.{RESET}", end="", flush=True)
                        passed_tests.append(test_file)
                    else:
                        print(f"{RED}F{RESET}", end="", flush=True)
                        failed_tests.append((test_file, error_msg))

                    # Line break every 50 tests
                    if completed % 50 == 0:
                        print(f" [{completed}/{total}]")

            # Final line break if needed
            if completed % 50 != 0:
                print(f" [{completed}/{total}]")

        finally:
            # Clean up base temp directory if empty
            try:
                if os.path.exists(temp_base_dir) and not os.listdir(temp_base_dir):
                    os.rmdir(temp_base_dir)
            except Exception:
                pass

        return sorted(passed_tests), sorted(failed_tests)

    def run_tests_combined(
        self,
    ) -> tuple[dict[str, tuple[bool, str]], dict[str, tuple[bool, str]]]:
        """
        Run all tests for both ROM and RAM in parallel.

        Returns:
            Tuple of (rom_results, ram_results) where each is a dict mapping
            test path to (passed, error_msg)
        """
        GREEN = "\033[92m"
        RED = "\033[91m"
        RESET = "\033[0m"

        print(
            f"Running CPU tests (ROM + RAM) in parallel ({self.max_workers} workers)...\n"
        )

        # Create base temp directory
        temp_base_dir = os.path.abspath(self.config.PARALLEL_TMP_DIR)
        os.makedirs(temp_base_dir, exist_ok=True)

        runner = CPUTestRunner()
        tests = runner.get_test_files()
        total = len(tests) * 2  # ROM + RAM for each test

        # Prepare arguments for parallel execution - both ROM and RAM
        test_args = []
        for i, test in enumerate(tests):
            test_args.append((test, False, temp_base_dir, i * 2))  # ROM
            test_args.append((test, True, temp_base_dir, i * 2 + 1))  # RAM

        rom_results: dict[str, tuple[bool, str]] = {}
        ram_results: dict[str, tuple[bool, str]] = {}
        completed = 0

        try:
            with ProcessPoolExecutor(max_workers=self.max_workers) as executor:
                futures = {
                    executor.submit(_run_single_test_parallel, args): (args[0], args[1])
                    for args in test_args
                }

                for future in as_completed(futures):
                    test_file, passed, error_msg = future.result()
                    _, use_ram = futures[future]
                    completed += 1

                    # Print progress dot with memory type indicator
                    if passed:
                        print(f"{GREEN}.{RESET}", end="", flush=True)
                    else:
                        print(f"{RED}F{RESET}", end="", flush=True)

                    # Store result
                    if use_ram:
                        ram_results[test_file] = (passed, error_msg)
                    else:
                        rom_results[test_file] = (passed, error_msg)

                    # Line break every 50 tests
                    if completed % 50 == 0:
                        print(f" [{completed}/{total}]")

            # Final line break if needed
            if completed % 50 != 0:
                print(f" [{completed}/{total}]")

        finally:
            # Clean up base temp directory if empty
            try:
                if os.path.exists(temp_base_dir) and not os.listdir(temp_base_dir):
                    os.rmdir(temp_base_dir)
            except Exception:
                pass

        return rom_results, ram_results


def main() -> None:
    """Main entry point for the script."""
    import argparse

    parser = argparse.ArgumentParser(description="Run CPU tests")
    parser.add_argument("--rom", action="store_true", help="Run tests from ROM only")
    parser.add_argument("--ram", action="store_true", help="Run tests from RAM only")
    parser.add_argument(
        "--combined",
        action="store_true",
        help="Run both ROM and RAM tests with combined output (default)",
    )
    parser.add_argument(
        "--sequential",
        action="store_true",
        help="Run tests sequentially instead of in parallel",
    )
    parser.add_argument(
        "--workers",
        type=int,
        default=None,
        help="Number of parallel workers (default: CPU count)",
    )
    parser.add_argument(
        "test_file",
        nargs="?",
        default=None,
        help="Optional: specific test file to run (e.g., 1_load.asm)",
    )

    args = parser.parse_args()

    # If neither --rom nor --ram is specified, use combined mode
    use_combined = args.combined or (not args.rom and not args.ram)
    use_ram = args.ram

    if args.test_file:
        # Run a single test (always sequential)
        runner = CPUTestRunner()
        if use_combined:
            # Run both ROM and RAM for single test
            print(f"Running single CPU test: {args.test_file}")
            rom_passed = True
            ram_passed = True
            try:
                print("  ROM: ", end="")
                runner.run_single_test(args.test_file, use_ram=False)
                print("\033[92m✓ PASS\033[0m")
            except Exception as e:
                print(f"\033[91m✗ FAIL: {e}\033[0m")
                rom_passed = False
            try:
                print("  RAM: ", end="")
                runner.run_single_test(args.test_file, use_ram=True)
                print("\033[92m✓ PASS\033[0m")
            except Exception as e:
                print(f"\033[91m✗ FAIL: {e}\033[0m")
                ram_passed = False
            if not (rom_passed and ram_passed):
                sys.exit(1)
        else:
            memory_type = "RAM" if use_ram else "ROM"
            print(f"Running single CPU test from {memory_type}: {args.test_file}")
            try:
                runner.run_single_test(args.test_file, use_ram=use_ram)
                logger.info(f"PASS: {args.test_file}")
            except Exception as e:
                logger.error(f"FAIL: {args.test_file} -> {e}")
                sys.exit(1)
    elif args.sequential:
        # Run all tests sequentially
        runner = CPUTestRunner()
        if use_ram:
            failed = runner.run_tests_from_ram()
        else:
            failed = runner.run_tests_from_rom()
        if failed:
            sys.exit(1)
    elif use_combined:
        # Run all tests in parallel with combined ROM+RAM output (default)
        runner = ParallelCPUTestRunner(max_workers=args.workers)
        rom_results, ram_results = runner.run_tests_combined()
        failed_count = _display_results_combined(rom_results, ram_results)
        if failed_count > 0:
            sys.exit(1)
    else:
        # Run all tests in parallel (single memory type)
        runner = ParallelCPUTestRunner(max_workers=args.workers)
        memory_type = "RAM" if use_ram else "ROM"
        passed, failed = runner.run_tests_parallel(use_ram=use_ram)
        total = len(passed) + len(failed)
        _display_results_grouped(passed, failed, total, memory_type)
        if failed:
            sys.exit(1)


if __name__ == "__main__":
    main()
