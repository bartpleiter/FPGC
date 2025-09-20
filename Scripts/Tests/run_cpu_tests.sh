#!/bin/bash

# Activate the virtual environment
source .venv/bin/activate

# Run the CPU tests
python3 Tests/CPU/cpu_tests.py

# Deactivate virtual environment
deactivate
