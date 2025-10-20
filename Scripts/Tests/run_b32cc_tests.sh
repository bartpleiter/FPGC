#!/bin/bash

# Activate the virtual environment
source .venv/bin/activate

# Run the B32CC test suite
python3 Scripts/Tests/b32cc_tests.py

# Deactivate virtual environment
deactivate
