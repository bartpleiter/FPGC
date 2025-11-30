#!/bin/bash

# Activate the virtual environment
source .venv/bin/activate

# Run the CPU tests
# If a test file is provided as argument, run only that test
if [ -n "$1" ]; then
    python3 Scripts/Tests/cpu_tests.py --rom "$1"
    echo
    python3 Scripts/Tests/cpu_tests.py --ram "$1"
else
    python3 Scripts/Tests/cpu_tests.py --rom
    echo
    python3 Scripts/Tests/cpu_tests.py --ram
fi

# Deactivate virtual environment
deactivate
