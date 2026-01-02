#!/bin/bash

# Exit immediately if a command exits with a non-zero status
set -e

# Activate the virtual environment
source .venv/bin/activate

# Parse arguments
WORKERS=""
TEST_FILE=""
MODE=""

while [[ $# -gt 0 ]]; do
    case $1 in
        --workers)
            WORKERS="--workers $2"
            shift 2
            ;;
        --rom)
            MODE="--rom"
            shift
            ;;
        --ram)
            MODE="--ram"
            shift
            ;;
        *)
            TEST_FILE="$1"
            shift
            ;;
    esac
done

# Run the CPU tests
# Default is combined mode (both ROM and RAM with merged output)
# Use --rom or --ram to run only one memory type
if [ -n "$TEST_FILE" ]; then
    python3 Scripts/Tests/cpu_tests.py $MODE $WORKERS "$TEST_FILE"
else
    python3 Scripts/Tests/cpu_tests.py $MODE $WORKERS
fi

# Deactivate virtual environment
deactivate
