"""
B32CC Compiler Test Suite

This script automatically tests the B32CC C compiler by:
1. Compiling all .c files in Tests/C/ to assembly
2. Assembling the code and preparing it for Verilog simulation
3. Running the Verilog simulation
4. Extracting the UART output from the simulation
5. Comparing against the expected value (from // expected=0xXX comments)

The test suite provides a clear overview of passed and failed tests.

Usage:
    python3 Scripts/Tests/b32cc_tests.py

    Or via Makefile:
    make test-b32cc
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
class B32CCTestConfig:
    """Configuration constants for B32CC tests."""

    TESTS_DIRECTORY: str = "Tests/C"
    TMP_DIRECTORY: str = "Tests/C/tmp"
    ROM_LIST_PATH: str = "Hardware/FPGA/Verilog/Simulation/MemoryLists/rom.list"
    RAM_LIST_PATH: str = "Hardware/FPGA/Verilog/Simulation/MemoryLists/ram.list"
    MIG7MOCK_LIST_PATH: str = (
        "Hardware/FPGA/Verilog/Simulation/MemoryLists/mig7mock.list"
    )
    BOOTLOADER_ROM_PATH: str = "Software/BareMetalASM/Simulation/sim_jump_to_ram.asm"
    TESTBENCH_PATH: str = "Hardware/FPGA/Verilog/Simulation/cpu_tests_tb.v"
    VERILOG_OUTPUT_PATH: str = "Hardware/FPGA/Verilog/Simulation/Output/cpu.out"
    MEMORY_LISTS_DIR: str = "Hardware/FPGA/Verilog/Simulation/MemoryLists"

    COMPILER_PATH: str = "BuildTools/B32CC/output/b32cc"
    CONVERTER_SCRIPT: str = "Scripts/Simulation/convert_to_256_bit.py"
    ROM_OFFSET: str = "0x7800000"

    # Parallel test temp directory
    PARALLEL_TMP_DIR: str = "Tests/tmp"


class B32CCTestError(Exception):
    """Custom exception for B32CC test related errors."""

    pass


class CompilerError(B32CCTestError):
    """Exception raised when compiler fails."""

    pass


class AssemblerError(B32CCTestError):
    """Exception raised when assembler fails."""

    pass


class SimulationError(B32CCTestError):
    """Exception raised when simulation fails."""

    pass


class ResultParsingError(B32CCTestError):
    """Exception raised when result parsing fails."""

    pass


class B32CCTestRunner:
    """Main class for running B32CC tests with improved error handling and logging."""

    def __init__(self, config: B32CCTestConfig = None, temp_dir: Optional[str] = None):
        """Initialize the test runner with configuration.

        Args:
            config: Configuration for the test runner
            temp_dir: Optional temporary directory for isolated test execution
        """
        self.config = config or B32CCTestConfig()
        self.temp_dir = temp_dir

        # If using temp_dir, override paths for isolation
        if temp_dir:
            self._setup_temp_paths()

    def _setup_temp_paths(self):
        """Set up paths for isolated execution in temp directory."""
        self.config.TMP_DIRECTORY = self.temp_dir
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
            B32CCTestError: If command execution fails
        """
        logger.debug(f"Running command: {command}")
        try:
            result = subprocess.run(
                command, shell=True, capture_output=True, text=True, timeout=60
            )
            return result.returncode, result.stdout + result.stderr
        except subprocess.TimeoutExpired:
            raise B32CCTestError(f"Command timed out: {description}")
        except Exception as e:
            raise B32CCTestError(f"Failed to execute command '{description}': {e}")

    def _parse_simulation_result(self, result: str) -> int:
        """
        Parse the simulation result to extract the UART transmitted byte.

        Args:
            result: Raw simulation output

        Returns:
            The UART transmitted byte value

        Raises:
            ResultParsingError: If no result is found
        """
        lines = result.split("\n")

        # Look for "UART TX: XX" pattern
        uart_pattern = re.compile(r"UART TX:\s*([0-9a-fA-F]+)")

        for line in lines:
            match = uart_pattern.search(line)
            if match:
                try:
                    hex_value = match.group(1)
                    return int(hex_value, 16)
                except (ValueError, IndexError) as e:
                    raise ResultParsingError(
                        f"Failed to parse UART value from line: {line}"
                    ) from e

        raise ResultParsingError("No UART transmission found in simulation output")

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

    def _compile_c_to_asm(self, c_path: str, asm_path: str) -> None:
        """
        Compile C code to assembly using B32CC.

        Args:
            c_path: Path to C source file
            asm_path: Path to output assembly file

        Raises:
            CompilerError: If compilation fails
        """
        compile_cmd = f"{self.config.COMPILER_PATH} {c_path} {asm_path}"
        exit_code, output = self._run_command(compile_cmd, f"Compiling {c_path}")

        if exit_code != 0:
            raise CompilerError(f"B32CC compilation failed for {c_path}: {output}")

    def _assemble_code(
        self,
        source_path: str,
        output_path: str,
        description: str,
        hex_format: bool = False,
    ) -> None:
        """
        Assemble assembly code to a list file.

        Args:
            source_path: Path to source assembly file
            output_path: Path to output list file
            description: Description for logging
            hex_format: Whether to use hex format (-h flag)

        Raises:
            AssemblerError: If assembly fails
        """
        # Run asmpy assembler
        hex_flag = "-h" if hex_format else ""
        assemble_cmd = f"asmpy {source_path} {output_path} {hex_flag}".strip()
        exit_code, output = self._run_command(assemble_cmd, f"Assembling {description}")

        if exit_code != 0:
            raise AssemblerError(f"Assembler failed for {description}: {output}")

    def _prepare_simulation_environment(self, asm_path: str) -> None:
        """
        Prepare the simulation environment by assembling ROM bootloader and RAM code.

        Args:
            asm_path: Path to the assembly file to run in RAM

        Raises:
            AssemblerError: If assembly fails
        """
        # Compile ROM bootloader to jump to RAM
        assemble_cmd = f"asmpy {self.config.BOOTLOADER_ROM_PATH} {self.config.ROM_LIST_PATH} -o {self.config.ROM_OFFSET}"
        exit_code, output = self._run_command(assemble_cmd, "Assembling ROM bootloader")

        if exit_code != 0:
            raise AssemblerError(f"ROM bootloader assembly failed: {output}")

        # Assemble the test code for RAM
        self._assemble_code(
            asm_path, self.config.RAM_LIST_PATH, "RAM code", hex_format=True
        )

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
            B32CCTestError: If no expected value is found
        """
        # Look for pattern like: // expected=0xXX
        expected_pattern = re.compile(
            r"//\s*expected\s*=\s*(0x[0-9a-fA-F]+)", re.IGNORECASE
        )

        for line in test_lines:
            match = expected_pattern.search(line)
            if match:
                try:
                    return int(match.group(1), 16)
                except (ValueError, IndexError) as e:
                    raise B32CCTestError(
                        f"Invalid expected value format in line: {line}"
                    ) from e

        raise B32CCTestError("No expected value found in test file")

    def run_single_test(self, test_file: str) -> None:
        """
        Run a single test.

        Args:
            test_file: Name of the test file

        Raises:
            B32CCTestError: If test fails
        """
        test_path = os.path.join(self.config.TESTS_DIRECTORY, test_file)

        # Determine temporary assembly file path
        base_name = os.path.splitext(test_file)[0]
        asm_path = os.path.join(self.config.TMP_DIRECTORY, f"{base_name}.asm")

        try:
            with open(test_path, "r") as file:
                lines = file.readlines()
        except FileNotFoundError:
            raise B32CCTestError(f"Test file not found: {test_path}")
        except Exception as e:
            raise B32CCTestError(f"Failed to read test file {test_path}: {e}")

        expected_value = self._get_expected_value(lines)

        # Prepare temp testbench if running in isolated mode
        if self.temp_dir:
            self._prepare_temp_testbench()

        # Compile C to assembly
        self._compile_c_to_asm(test_path, asm_path)

        # Prepare simulation environment (assemble ROM and RAM)
        self._prepare_simulation_environment(asm_path)

        # Run simulation
        simulation_output = self._run_simulation()

        # Parse result
        resulting_value = self._parse_simulation_result(simulation_output)

        if resulting_value != expected_value:
            raise B32CCTestError(
                f"Expected 0x{expected_value:02X}, got 0x{resulting_value:02X}"
            )

    def get_test_files(self) -> list[str]:
        """
        Get a sorted list of test files.

        Returns:
            List of test file names

        Raises:
            B32CCTestError: If tests directory cannot be read
        """
        try:
            tests = []
            for file in os.listdir(self.config.TESTS_DIRECTORY):
                if file.endswith(".c"):
                    tests.append(file)
            return sorted(tests)
        except FileNotFoundError:
            raise B32CCTestError(
                f"Tests directory not found: {self.config.TESTS_DIRECTORY}"
            )
        except Exception as e:
            raise B32CCTestError(f"Failed to read tests directory: {e}")

    def run_tests(self) -> tuple[list[str], list[str]]:
        """
        Run all tests.

        Returns:
            Tuple of (passed_tests, failed_tests)
        """
        logger.info("Running B32CC compiler tests...")

        # Ensure tmp directory exists
        os.makedirs(self.config.TMP_DIRECTORY, exist_ok=True)

        tests = self.get_test_files()
        passed_tests = []
        failed_tests = []

        for test in tests:
            try:
                self.run_single_test(test)
                logger.info(f"PASS: {test}")
                passed_tests.append(test)
            except Exception as e:
                logger.error(f"FAIL: {test} -> {e}")
                failed_tests.append(test)

        return passed_tests, failed_tests

    def _display_results(self, failed_tests: list[str]) -> None:
        """Display test results summary."""
        print("-" * 20)

        if failed_tests:
            print("Failed tests:")
            for test in failed_tests:
                print(f"  {test}")
        else:
            print("All tests passed!")


def _run_single_test_parallel(args: tuple) -> tuple[str, bool, str]:
    """
    Run a single test in isolation for parallel execution.

    Args:
        args: Tuple of (test_file, temp_base_dir, test_index)

    Returns:
        Tuple of (test_file, passed, error_message)
    """
    test_file, temp_base_dir, test_index = args

    # Create a unique temp directory for this test
    temp_dir = os.path.join(temp_base_dir, f"test_{test_index}")
    os.makedirs(temp_dir, exist_ok=True)

    try:
        runner = B32CCTestRunner(temp_dir=temp_dir)
        runner.run_single_test(test_file)
        return (test_file, True, "")
    except Exception as e:
        return (test_file, False, str(e))
    finally:
        # Clean up temp directory
        try:
            shutil.rmtree(temp_dir)
        except Exception:
            pass


class ParallelB32CCTestRunner:
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
        self.config = B32CCTestConfig()

    def run_tests_parallel(self) -> tuple[list[str], list[str]]:
        """
        Run all tests in parallel.

        Returns:
            Tuple of (passed_tests, failed_tests)
        """
        logger.info(
            f"Running B32CC compiler tests in parallel ({self.max_workers} workers)..."
        )

        # Create base temp directory
        temp_base_dir = os.path.abspath(self.config.PARALLEL_TMP_DIR)
        os.makedirs(temp_base_dir, exist_ok=True)

        runner = B32CCTestRunner()
        tests = runner.get_test_files()

        # Prepare arguments for parallel execution
        test_args = [(test, temp_base_dir, i) for i, test in enumerate(tests)]

        passed_tests = []
        failed_tests = []

        try:
            with ProcessPoolExecutor(max_workers=self.max_workers) as executor:
                futures = {
                    executor.submit(_run_single_test_parallel, args): args[0]
                    for args in test_args
                }

                for future in as_completed(futures):
                    test_file, passed, error_msg = future.result()
                    if passed:
                        logger.info(f"PASS: {test_file}")
                        passed_tests.append(test_file)
                    else:
                        logger.error(f"FAIL: {test_file} -> {error_msg}")
                        failed_tests.append(test_file)
        finally:
            # Clean up base temp directory if empty
            try:
                if os.path.exists(temp_base_dir) and not os.listdir(temp_base_dir):
                    os.rmdir(temp_base_dir)
            except Exception:
                pass

        return sorted(passed_tests), sorted(failed_tests)

    def _display_results(self, failed_tests: list[str]) -> None:
        """Display test results summary."""
        print("-" * 20)
        if failed_tests:
            print("Failed tests:")
            for test in failed_tests:
                print(f"  {test}")
        else:
            print("All tests passed!")


def main() -> None:
    """Main entry point for the script."""
    import argparse

    parser = argparse.ArgumentParser(description="B32CC Compiler Test Suite")
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
        help="Specific test file to run (e.g., 3_1_if_statements.c). If not provided, runs all tests.",
    )
    args = parser.parse_args()

    if args.test_file:
        # Run single test (always sequential)
        runner = B32CCTestRunner()
        try:
            os.makedirs(runner.config.TMP_DIRECTORY, exist_ok=True)
            runner.run_single_test(args.test_file)
            logger.info(f"PASS: {args.test_file}")
        except Exception as e:
            logger.error(f"FAIL: {args.test_file} -> {e}")
            sys.exit(1)
    elif args.sequential:
        # Run all tests sequentially
        runner = B32CCTestRunner()
        _, failed_tests = runner.run_tests()
        runner._display_results(failed_tests)
        if failed_tests:
            sys.exit(1)
    else:
        # Run all tests in parallel (default)
        runner = ParallelB32CCTestRunner(max_workers=args.workers)
        _, failed_tests = runner.run_tests_parallel()
        runner._display_results(failed_tests)
        if failed_tests:
            sys.exit(1)


if __name__ == "__main__":
    main()
