import os

TESTS_DIRECTORY = "Software/BareMetalASM/Simulation/tests"
TARGET_ASM_PATH = "BuildTools/ASM/code.asm"
ROM_LIST_PATH = "Hardware/Vivado/FPGC.srcs/simulation/memory/rom.list"
TESTBENCH_PATH = "Hardware/Vivado/FPGC.srcs/simulation/cpu_tb.v"
VERILOG_OUTPUT_PATH = "Hardware/Vivado/FPGC.srcs/simulation/output/cpu.out"

def parse_result(result):
    lines = result.split("\n")
    for line in lines:
        if "reg15 :=" in line:
            return int(line.split("reg15 :=")[1].strip())

    raise Exception("No result found")

def run_simulation():
    os.system(f"iverilog -Dtestbench -o {VERILOG_OUTPUT_PATH} {TESTBENCH_PATH}")
    return os.popen(f"vvp {VERILOG_OUTPUT_PATH}").read()
    
def assemble_code(path):
    os.system(f"cp {path} {TARGET_ASM_PATH}")

    result = os.system("cd BuildTools/ASM && python3 Assembler.py -H > code.list")
    if result == 0:
        # Move to simulation directory
        os.rename("BuildTools/ASM/code.list", ROM_LIST_PATH)
    else:
        # Print the error, which is in code.list
        with open("BuildTools/ASM/code.list", "r") as error_file:
            print(error_file.read())
        raise Exception("Assembler failed")

def get_expected_value(test_lines):
    for line in test_lines:
        if "expected=" in line:
            return int(line.split("expected=")[1].strip())

def run_test(test):
    path = os.path.join(TESTS_DIRECTORY, test)
    with open(path, "r") as file:
        lines = file.readlines()

    expected_value = get_expected_value(lines)

    assemble_code(path)

    result = run_simulation()

    try:
        resulting_value = parse_result(result)
    except Exception as e:
        raise Exception(f"Failed to parse result: {e}")
    
    if resulting_value != expected_value:
        raise Exception(f"Expected {expected_value}, got {resulting_value}")
    

def get_tests():
    tests = []
    for file in os.listdir(TESTS_DIRECTORY):
        if file.endswith(".asm"):
            tests.append(file)
    return tests

def main():
    failed_tests = []
    tests = get_tests()
    for test in tests:
        try:
            run_test(test)
            print(f"PASS: {test}")
        except Exception as e:
            print(f"FAIL: {test} -> {e}")
            failed_tests.append(test)

    if len(failed_tests) > 0:
        print("--------------------")
        print("Failed tests:")
        for test in failed_tests:
            print(test)
    else:
        print("--------------------")
        print("All tests passed")

if __name__ == "__main__":
    main()
