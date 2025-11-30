#!/bin/bash

# Activate the virtual environment
source .venv/bin/activate

# Run the B32CC test suite
# Supports --sequential and --workers options, plus optional test file
python3 Scripts/Tests/b32cc_tests.py "$@"
RESULT=$?

# Deactivate virtual environment
deactivate

# Exit with the Python script's exit code
exit $RESULT
