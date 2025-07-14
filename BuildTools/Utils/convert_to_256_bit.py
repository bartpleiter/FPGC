# This script is used mainly to convert the assembly output into a 256-bit format for the MIG7 mock.

import sys
import os

def convert_to_256_bit(input_path, output_path):
    words = []
    with open(input_path, 'r') as infile:
        for line in infile:
            line = line.strip()
            if not line or line.startswith('//'):
                continue
            # Remove comments if present
            if '//' in line:
                line = line.split('//')[0].strip()
            if line:
                words.append(line)
    
    # Group into chunks of 8, pad with zeroes if needed
    chunk_size = 8
    zero_word = '0' * len(words[0]) if words else '0' * 32
    output_lines = []
    for i in range(0, len(words), chunk_size):
        chunk = words[i:i+chunk_size]
        if len(chunk) < chunk_size:
            chunk += [zero_word] * (chunk_size - len(chunk))
        output_lines.append('_'.join(chunk[::-1]))
    with open(output_path, 'w') as outfile:
        for line in output_lines:
            outfile.write(line + '\n')

def main():
    if len(sys.argv) != 3:
        print(f"Usage: {os.path.basename(sys.argv[0])} <input_file> <output_file>")
        sys.exit(1)
    convert_to_256_bit(sys.argv[1], sys.argv[2])

if __name__ == "__main__":
    main()
