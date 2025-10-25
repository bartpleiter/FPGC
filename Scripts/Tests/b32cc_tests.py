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


class B32CCTestConfig:
    """Configuration constants for B32CC tests."""

    TESTS_DIRECTORY = "Tests/C"
    TMP_DIRECTORY = "Tests/C/tmp"
    ROM_LIST_PATH = "Hardware/FPGA/Verilog/Simulation/MemoryLists/rom.list"
    RAM_LIST_PATH = "Hardware/FPGA/Verilog/Simulation/MemoryLists/ram.list"
    MIG7MOCK_LIST_PATH = "Hardware/FPGA/Verilog/Simulation/MemoryLists/mig7mock.list"
    BOOTLOADER_ROM_PATH = "Software/BareMetalASM/Simulation/sim_jump_to_ram.asm"
    TESTBENCH_PATH = "Hardware/FPGA/Verilog/Simulation/cpu_tests_tb.v"
    VERILOG_OUTPUT_PATH = "Hardware/FPGA/Verilog/Simulation/Output/cpu.out"

    COMPILER_PATH = "BuildTools/B32CC/output/b32cc"
    CONVERTER_SCRIPT = "Scripts/Simulation/convert_to_256_bit.py"
    ROM_OFFSET = "0x7800000"


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

    def __init__(self, config: B32CCTestConfig = None):
        """Initialize the test runner with configuration."""
        self.config = config or B32CCTestConfig()

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

        # Look for "uart_tx byte 0xXX" pattern
        uart_pattern = re.compile(r"uart_tx byte\s+(0x[0-9a-fA-F]+)")

        for line in lines:
            match = uart_pattern.search(line)
            if match:
                try:
                    # Extract the hex value and convert to int
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
        # Compile testbench
        compile_cmd = f"iverilog -o {self.config.VERILOG_OUTPUT_PATH} {self.config.TESTBENCH_PATH}"
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


def main() -> None:
    """Main entry point for the script."""
    runner = B32CCTestRunner()
    _, failed_tests = runner.run_tests()
    runner._display_results(failed_tests)

    # Exit with non-zero code if tests failed
    if failed_tests:
        sys.exit(1)


if __name__ == "__main__":
    main()
