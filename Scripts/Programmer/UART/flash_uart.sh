#!/bin/bash

# Activate the virtual environment
source .venv/bin/activate

# Flash code via UART flasher
echo "Flashing via UART"
if python3 Scripts/Programmer/UART/uart_flasher.py -p /dev/ttyUSB0 -f Software/BareMetalASM/Output/code.bin
then
    echo "Flashing successful"
else
    echo "Flashing failed"
    exit
fi


# Deactivate virtual environment
deactivate
