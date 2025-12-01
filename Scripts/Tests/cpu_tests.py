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
        Get a sorted list of test files.

        Returns:
            List of test file names

        Raises:
            CPUTestError: If tests directory cannot be read
        """
        try:
            tests = []
            for file in os.listdir(self.config.TESTS_DIRECTORY):
                if file.endswith(".asm"):
                    tests.append(file)
            return sorted(tests)
        except FileNotFoundError:
            raise CPUTestError(
                f"Tests directory not found: {self.config.TESTS_DIRECTORY}"
            )
        except Exception as e:
            raise CPUTestError(f"Failed to read tests directory: {e}")

    def run_tests(self, use_ram: bool = False) -> tuple[list[str], list[str]]:
        """
        Run all tests.

        Args:
            use_ram: Whether to run tests from RAM

        Returns:
            Tuple of (passed_tests, failed_tests)
        """
        memory_type = "RAM" if use_ram else "ROM"
        logger.info(f"Running CPU tests from {memory_type}...")

        tests = self.get_test_files()
        passed_tests = []
        failed_tests = []

        for test in tests:
            try:
                self.run_single_test(test, use_ram=use_ram)
                logger.info(f"PASS: {test}")
                passed_tests.append(test)
            except Exception as e:
                logger.error(f"FAIL: {test} -> {e}")
                failed_tests.append(test)

        return passed_tests, failed_tests

    def run_tests_from_rom(self) -> list[str]:
        """Run all tests from ROM and display results.

        Returns:
            List of failed test names.
        """
        _, failed_tests = self.run_tests(use_ram=False)
        self._display_results(failed_tests)
        return failed_tests

    def run_tests_from_ram(self) -> list[str]:
        """Run all tests from RAM and display results.

        Returns:
            List of failed test names.
        """
        _, failed_tests = self.run_tests(use_ram=True)
        self._display_results(failed_tests)
        return failed_tests

    def _display_results(self, failed_tests: list[str]) -> None:
        """Display test results summary."""
        print("--------------------")
        if failed_tests:
            print("Failed tests:")
            for test in failed_tests:
                print(test)
        else:
            print("All tests passed")


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

    def run_tests_parallel(self, use_ram: bool = False) -> tuple[list[str], list[str]]:
        """
        Run all tests in parallel.

        Args:
            use_ram: Whether to run tests from RAM

        Returns:
            Tuple of (passed_tests, failed_tests)
        """
        memory_type = "RAM" if use_ram else "ROM"
        logger.info(
            f"Running CPU tests from {memory_type} in parallel ({self.max_workers} workers)..."
        )

        # Create base temp directory
        temp_base_dir = os.path.abspath(self.config.PARALLEL_TMP_DIR)
        os.makedirs(temp_base_dir, exist_ok=True)

        runner = CPUTestRunner()
        tests = runner.get_test_files()

        # Prepare arguments for parallel execution
        test_args = [(test, use_ram, temp_base_dir, i) for i, test in enumerate(tests)]

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
        print("--------------------")
        if failed_tests:
            print("Failed tests:")
            for test in failed_tests:
                print(test)
        else:
            print("All tests passed")


def main() -> None:
    """Main entry point for the script."""
    import argparse

    parser = argparse.ArgumentParser(description="Run CPU tests")
    parser.add_argument("--rom", action="store_true", help="Run tests from ROM")
    parser.add_argument("--ram", action="store_true", help="Run tests from RAM")
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

    use_ram = args.ram

    if args.test_file:
        # Run a single test (always sequential)
        runner = CPUTestRunner()
        memory_type = "RAM" if use_ram else "ROM"
        logger.info(f"Running single CPU test from {memory_type}: {args.test_file}")
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
            failed_tests = runner.run_tests_from_ram()
        else:
            failed_tests = runner.run_tests_from_rom()
        if failed_tests:
            sys.exit(1)
    else:
        # Run all tests in parallel (default)
        runner = ParallelCPUTestRunner(max_workers=args.workers)
        _, failed_tests = runner.run_tests_parallel(use_ram=use_ram)
        runner._display_results(failed_tests)
        if failed_tests:
            sys.exit(1)


if __name__ == "__main__":
    main()
