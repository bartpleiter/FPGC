#!/usr/bin/env bash
# test_selfhost.sh — Automated selfhost toolchain test on BDOS
#
# Usage: ./Scripts/Tests/test_selfhost.sh [uart_port] [dev]
#
# Prerequisites:
#   - BDOS must already be booted (UART monitor causes reset)
#   - Test files must be synced: make fnp-sync-files
#
# This script:
#   1. Starts UART monitor in background → log file
#   2. Runs compile commands via fnp-run
#   3. Reads back output files via cat
#   4. Checks results

set -euo pipefail
cd "$(dirname "$0")/../.."  # repo root

UART_PORT="${1:-/dev/ttyUSB0}"
DEV="${2:-1}"
FNP_MAC="02:B4:B4:00:00:0${DEV}"
UART_LOG="tmp/uart_test.log"
FNP=".venv/bin/python3 Scripts/Programmer/Network/fnp_tool.py --mac ${FNP_MAC}"
PASS=0
FAIL=0
TOTAL=0

mkdir -p tmp

# --- helpers ---

fnp_cmd() {
    # Send a shell command to BDOS and wait for it to complete
    local cmd="$1"
    local wait="${2:-3}"  # seconds to wait for output
    echo "  > $cmd"
    $FNP key "$cmd"
    $FNP keycode 0x0A
    sleep "$wait"
}

check_uart_contains() {
    local pattern="$1"
    local desc="$2"
    TOTAL=$((TOTAL + 1))
    if grep -q "$pattern" "$UART_LOG" 2>/dev/null; then
        echo "  PASS: $desc"
        PASS=$((PASS + 1))
    else
        echo "  FAIL: $desc (expected '$pattern' in UART output)"
        FAIL=$((FAIL + 1))
    fi
}

start_uart() {
    echo "Starting UART monitor on $UART_PORT (log: $UART_LOG)"
    echo "  Note: Opening port will reset FPGC. Waiting for boot..."
    > "$UART_LOG"
    .venv/bin/python3 Scripts/Programmer/UART/uart_monitor.py -p "$UART_PORT" >> "$UART_LOG" 2>&1 &
    UART_PID=$!
    echo "  UART PID: $UART_PID"
    echo "  Waiting 9s for BDOS boot + BRFS mount..."
    sleep 9
    echo "  Boot complete."
}

stop_uart() {
    if [[ -n "${UART_PID:-}" ]]; then
        kill "$UART_PID" 2>/dev/null || true
        wait "$UART_PID" 2>/dev/null || true
    fi
}

trap stop_uart EXIT

# --- main ---

echo "=== FPGC Selfhost Toolchain Test ==="
echo "  Device: $DEV ($FNP_MAC)"
echo "  UART: $UART_PORT"
echo ""

# Step 1: Start UART capture (this resets the FPGC, waits for boot)
start_uart

# Step 2: Sync test files (after boot)
echo "Syncing files..."
$FNP sync-files Files/BRFS-init
sleep 3

# Step 3: Test cproc (C → QBE IR)
echo ""
echo "--- Test 1: cproc on a.c (return 7) ---"
> "$UART_LOG"  # clear log
fnp_cmd "cproc -o /a.qbe /a.c" 5
fnp_cmd "cat /a.qbe" 3
check_uart_contains "ret 7" "cproc generates QBE IR with 'ret 7'"

# Step 4: Test QBE (QBE IR → asm)
echo ""
echo "--- Test 2: qbe on a.qbe ---"
> "$UART_LOG"
fnp_cmd "qbe -o /a.asm /a.qbe" 5
fnp_cmd "cat /a.asm" 3
check_uart_contains "load 7 r1" "QBE generates assembly with 'load 7 r1'"

# Step 5: Test cproc + QBE pipeline on x.c (return 42+1)
echo ""
echo "--- Test 3: full pipeline on x.c (42+1) ---"
> "$UART_LOG"
fnp_cmd "cproc -o /x.qbe /x.c" 5
fnp_cmd "qbe -o /x.asm /x.qbe" 5
fnp_cmd "cat /x.asm" 3
check_uart_contains "load 43 r1" "Pipeline produces 'load 43 r1' (constant fold)"

# Step 6: Test with printf (vararg)
echo ""
echo "--- Test 4: printf vararg test ---"
> "$UART_LOG"
fnp_cmd "cproc -o /p.qbe /p.c" 5
fnp_cmd "cat /p.qbe" 3
check_uart_contains "\.\.\." "cproc emits '...' vararg marker"

> "$UART_LOG"
fnp_cmd "qbe -o /p.asm /p.qbe" 5
fnp_cmd "cat /p.asm" 3
check_uart_contains "sub r13" "QBE emits stack adjustment for vararg"

# Step 7: Test loops and function calls (t_loop.c)
echo ""
echo "--- Test 5: loops + function calls (t_loop.c) ---"
> "$UART_LOG"
fnp_cmd "cproc -o /t_loop.qbe /t_loop.c" 8
fnp_cmd "cat /t_loop.qbe" 3
check_uart_contains "fibonacci" "cproc emits fibonacci function"

> "$UART_LOG"
fnp_cmd "qbe -o /t_loop.asm /t_loop.qbe" 10
fnp_cmd "cat /t_loop.asm" 5
check_uart_contains "fibonacci" "QBE emits fibonacci label"

# Step 8: Test structs and pointers (t_ptr.c)
echo ""
echo "--- Test 6: structs + pointers (t_ptr.c) ---"
> "$UART_LOG"
fnp_cmd "cproc -o /t_ptr.qbe /t_ptr.c" 8
fnp_cmd "cat /t_ptr.qbe" 3
check_uart_contains "distance_sq" "cproc emits distance_sq function"

> "$UART_LOG"
fnp_cmd "qbe -o /t_ptr.asm /t_ptr.qbe" 10
fnp_cmd "cat /t_ptr.asm" 5
check_uart_contains "distance_sq" "QBE emits distance_sq label"

# Step 9: Test switch, globals, recursion (t_switch.c)
echo ""
echo "--- Test 7: switch + globals (t_switch.c) ---"
> "$UART_LOG"
fnp_cmd "cproc -o /t_switch.qbe /t_switch.c" 8
fnp_cmd "cat /t_switch.qbe" 3
check_uart_contains "factorial" "cproc emits factorial function"

> "$UART_LOG"
fnp_cmd "qbe -o /t_switch.asm /t_switch.qbe" 10
fnp_cmd "cat /t_switch.asm" 5
check_uart_contains "factorial" "QBE emits factorial label"

# Step 10: Test preprocessed argtest.c (with stdio.h)
echo ""
echo "--- Test 8: preprocessed argtest.c (includes) ---"
> "$UART_LOG"
fnp_cmd "cproc -o /argtest.qbe /argtest.c" 10
fnp_cmd "cat /argtest.qbe" 3
check_uart_contains "argv" "cproc handles preprocessed includes (argv in IR)"

> "$UART_LOG"
fnp_cmd "qbe -o /argtest.asm /argtest.qbe" 10
fnp_cmd "cat /argtest.asm" 5
check_uart_contains "main:" "QBE emits argtest main"
fnp_cmd "cat /p.qbe" 3
check_uart_contains "\.\.\." "cproc emits '...' vararg marker"

> "$UART_LOG"
fnp_cmd "qbe -o /p.asm /p.qbe" 5
fnp_cmd "cat /p.asm" 3
check_uart_contains "sub r13" "QBE emits stack adjustment for vararg"

# --- summary ---
echo ""
echo "=== Results: $PASS/$TOTAL passed, $FAIL failed ==="
stop_uart

if [[ $FAIL -gt 0 ]]; then
    echo ""
    echo "UART log (last 40 lines):"
    tail -40 "$UART_LOG"
    exit 1
fi
exit 0
