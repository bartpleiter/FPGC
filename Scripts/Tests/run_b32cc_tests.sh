#!/bin/bash

# Activate the virtual environment
source .venv/bin/activate

# Run the B32CC test suite
# If a test file is provided as argument, run only that test
python3 Scripts/Tests/b32cc_tests.py "$@"

# Deactivate virtual environment
deactivate
