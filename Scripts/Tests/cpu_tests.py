import os
import subprocess
import sys
import logging


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


class CPUTestConfig:
    """Configuration constants for CPU tests."""

    TESTS_DIRECTORY = "Tests/CPU"
    ROM_LIST_PATH = "Hardware/Vivado/FPGC.srcs/simulation/memory/rom.list"
    RAM_LIST_PATH = "Hardware/Vivado/FPGC.srcs/simulation/memory/ram.list"
    MIG7MOCK_LIST_PATH = "Hardware/Vivado/FPGC.srcs/simulation/memory/mig7mock.list"
    BOOTLOADER_ROM_PATH = "Software/BareMetalASM/Simulation/sim_jump_to_ram.asm"
    TESTBENCH_PATH = "Hardware/Vivado/FPGC.srcs/simulation/cpu_tests_tb.v"
    VERILOG_OUTPUT_PATH = "Hardware/Vivado/FPGC.srcs/simulation/output/cpu.out"

    CONVERTER_SCRIPT = "Scripts/Simulation/convert_to_256_bit.py"


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

    def __init__(self, config: CPUTestConfig = None):
        """Initialize the test runner with configuration."""
        self.config = config or CPUTestConfig()

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

        # Find the last occurrence of "reg15 :="
        for line in lines:
            if "reg15 :=" in line:
                last_reg15_line = line

        if last_reg15_line is None:
            raise ResultParsingError("No result found in simulation output")

        try:
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

    def run_tests_from_rom(self) -> None:
        """Run all tests from ROM and display results."""
        _, failed_tests = self.run_tests(use_ram=False)
        self._display_results(failed_tests)

    def run_tests_from_ram(self) -> None:
        """Run all tests from RAM and display results."""
        _, failed_tests = self.run_tests(use_ram=True)
        self._display_results(failed_tests)

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

    runner = CPUTestRunner()
    if len(sys.argv) > 1:
        if sys.argv[1] == "--ram":
            runner.run_tests_from_ram()
        elif sys.argv[1] == "--rom":
            runner.run_tests_from_rom()
        else:
            logger.warning("No --rom or --ram specified, defaulting to rom")
            runner.run_tests_from_rom()
    else:
        logger.warning("No --rom or --ram specified, defaulting to rom")
        runner.run_tests_from_rom()


if __name__ == "__main__":
    main()
