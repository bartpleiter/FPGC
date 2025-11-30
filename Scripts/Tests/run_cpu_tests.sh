#!/bin/bash

# Activate the virtual environment
source .venv/bin/activate

# Parse arguments
SEQUENTIAL=""
WORKERS=""
TEST_FILE=""

while [[ $# -gt 0 ]]; do
    case $1 in
        --sequential)
            SEQUENTIAL="--sequential"
            shift
            ;;
        --workers)
            WORKERS="--workers $2"
            shift 2
            ;;
        *)
            TEST_FILE="$1"
            shift
            ;;
    esac
done

# Run the CPU tests
# If a test file is provided as argument, run only that test
if [ -n "$TEST_FILE" ]; then
    python3 Scripts/Tests/cpu_tests.py --rom $SEQUENTIAL $WORKERS "$TEST_FILE"
    echo
    python3 Scripts/Tests/cpu_tests.py --ram $SEQUENTIAL $WORKERS "$TEST_FILE"
else
    python3 Scripts/Tests/cpu_tests.py --rom $SEQUENTIAL $WORKERS
    echo
    python3 Scripts/Tests/cpu_tests.py --ram $SEQUENTIAL $WORKERS
fi

# Deactivate virtual environment
deactivate
