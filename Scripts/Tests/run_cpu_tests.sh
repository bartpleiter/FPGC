#!/bin/bash

# Exit immediately if a command exits with a non-zero status
set -e

# Activate the virtual environment
source .venv/bin/activate

# Track overall failure status
FAILED=0

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
    python3 Scripts/Tests/cpu_tests.py --rom $SEQUENTIAL $WORKERS "$TEST_FILE" || FAILED=1
    echo
    python3 Scripts/Tests/cpu_tests.py --ram $SEQUENTIAL $WORKERS "$TEST_FILE" || FAILED=1
else
    python3 Scripts/Tests/cpu_tests.py --rom $SEQUENTIAL $WORKERS || FAILED=1
    echo
    python3 Scripts/Tests/cpu_tests.py --ram $SEQUENTIAL $WORKERS || FAILED=1
fi

# Deactivate virtual environment
deactivate

# Exit with failure if any test failed
exit $FAILED
