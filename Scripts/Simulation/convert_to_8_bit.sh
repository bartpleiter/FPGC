#!/bin/bash

# Check if input file is provided
if [ $# -eq 0 ]; then
    echo "Usage: $0 <input_file> [output_file]"
    echo "Example: $0 code.list code_8bit.list"
    exit 1
fi

input_file="$1"
output_file="${2:-${input_file%.*}_8bit.list}"

# Check if input file exists
if [ ! -f "$input_file" ]; then
    echo "Error: Input file '$input_file' not found"
    exit 1
fi

# Process the file
while IFS= read -r line; do
    # Skip empty lines
    if [ -z "$line" ]; then
        continue
    fi
    
    # Check if line starts with binary digits or comments
    if [[ $line =~ ^[01]{32} ]]; then
        # Extract the 32-bit binary number (first 32 characters)
        binary_32bit="${line:0:32}"
        
        # Split into 4 bytes (8 bits each) - MSB first
        byte3="${binary_32bit:0:8}"   # Most significant byte
        byte2="${binary_32bit:8:8}"
        byte1="${binary_32bit:16:8}"
        byte0="${binary_32bit:24:8}"  # Least significant byte
        
        # Output bytes in order (MSB first for big-endian)
        echo "$byte3"
        echo "$byte2"
        echo "$byte1"
        echo "$byte0"
    elif [[ $line =~ ^[01]{32}.*//.*$ ]]; then
        # Handle lines with comments
        binary_32bit="${line:0:32}"
        comment="${line:32}"
        
        # Split into 4 bytes
        byte3="${binary_32bit:0:8}"
        byte2="${binary_32bit:8:8}"
        byte1="${binary_32bit:16:8}"
        byte0="${binary_32bit:24:8}"
        
        # Output bytes with comment only on first byte
        echo "$byte3$comment"
        echo "$byte2"
        echo "$byte1"
        echo "$byte0"
    fi
done < "$input_file" > "$output_file"
