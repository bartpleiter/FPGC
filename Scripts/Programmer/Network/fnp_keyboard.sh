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

# Build --mac flag if FNP_TARGET_MAC is set (for device selection)
MAC_FLAG=""
if [ -n "$FNP_TARGET_MAC" ]; then
    MAC_FLAG="--mac $FNP_TARGET_MAC"
fi

python3 "$FNP_TOOL" $MAC_FLAG keyboard

deactivate
