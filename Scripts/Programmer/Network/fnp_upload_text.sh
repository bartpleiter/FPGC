#!/bin/bash
# Upload a text file to the FPGC via FNP (auto-detects Ethernet interface)
#
# Usage: ./fnp_upload_text.sh <local_file> <fpgc_path>
# Example: ./fnp_upload_text.sh readme.txt /user/readme.txt

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"
FNP_TOOL="$SCRIPT_DIR/fnp_tool.py"

if [ $# -lt 2 ]; then
    echo "Usage: $0 <local_file> <fpgc_path>"
    echo "Example: $0 readme.txt /user/readme.txt"
    exit 1
fi

LOCAL_FILE="$1"
FPGC_PATH="$2"

if [ ! -f "$LOCAL_FILE" ]; then
    echo "Error: file not found: $LOCAL_FILE"
    exit 1
fi

# Activate virtual environment for Python
source "$PROJECT_ROOT/.venv/bin/activate"

python3 "$FNP_TOOL" upload -t "$LOCAL_FILE" "$FPGC_PATH"

deactivate
