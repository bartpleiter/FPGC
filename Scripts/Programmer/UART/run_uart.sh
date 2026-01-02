#!/bin/bash

# Activate the virtual environment
source .venv/bin/activate

# Run code via UART runner
echo "Programming FPGC via UART"
SCRIPT=Scripts/Programmer/UART/uart_programmer.py
ARGS=(-p /dev/ttyUSB0 -f Software/ASM/Output/code.bin -m --monitor-duration 4 -r)

if python3 "$SCRIPT" "${ARGS[@]}"; then
    echo "Programming successful"
else
    echo "Programming failed"
    exit 1
fi

# Deactivate virtual environment
deactivate
