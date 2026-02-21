#!/bin/bash
# Start interactive keyboard streaming to the FPGC via FNP.
# Auto-detects the Ethernet interface.
# Press Ctrl+C to exit.
#
# Usage: ./fnp_keyboard.sh

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"
FNP_TOOL="$SCRIPT_DIR/fnp_tool.py"

# Activate virtual environment for Python
source "$PROJECT_ROOT/.venv/bin/activate"

python3 "$FNP_TOOL" keyboard

deactivate
