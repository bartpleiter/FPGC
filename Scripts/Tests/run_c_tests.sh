#!/bin/bash

# Activate the virtual environment
source .venv/bin/activate

# Run the modern C test suite
python3 Scripts/Tests/c_tests.py "$@"
RESULT=$?

# Deactivate virtual environment
deactivate

# Exit with the Python script's exit code
exit $RESULT
