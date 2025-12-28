#!/bin/bash

# Activate the virtual environment
source .venv/bin/activate

# Flash code via UART flasher
echo "Flashing via UART"
SCRIPT=Scripts/Programmer/UART/uart_flasher.py
ARGS=(-p /dev/ttyUSB0 -f Software/ASM/Output/code.bin -m --monitor-duration 4)

if python3 "$SCRIPT" "${ARGS[@]}"; then
    echo "Flashing successful"
else
    echo "Flashing failed"
    exit
fi


# Deactivate virtual environment
deactivate
