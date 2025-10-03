#!/usr/bin/env python3

# TODO: Rewrite this script, and add features like path arguments, error handling, etc.

import serial
from time import sleep
import sys


testReturnMode = False  # mode where we do not use a serial monitor,
                        # but instead wait for one byte and use it as return code of this program
if len(sys.argv) > 1:
    if (sys.argv[1] == "testMode"):
        testReturnMode = True

port = serial.Serial("/dev/ttyUSB0", baudrate=1000000, timeout=None)

sleep(0.3) # give the FPGC time to reset, even though it also works without this delay

# parse byte file
ba = bytearray()

with open("code.bin", "rb") as f:
    bytes_read = f.read()
for b in bytes_read:
    ba.append(b)

# combine each 4 bytes into a word
n = 4
wordList = [ba[i * n:(i + 1) * n] for i in range((len(ba) + n - 1) // n )]  

# size of program is in address 2
fileSize = bytes(wordList[2])

print(int.from_bytes(fileSize, "big"), flush=True)

# write filesize one byte at a time
for b in fileSize:
    port.write(bytes([b]))

# read four bytes
rcv = port.read(4)

# to verify if communication works
print(rcv, flush=True)


# send all words
doneSending = False

wordCounter = 0

while not doneSending:
    for b in wordList[wordCounter]:
        port.write(bytes([b]))

    wordCounter = wordCounter + 1

    if (wordCounter == int.from_bytes(fileSize, "big")):
        doneSending = True

print("Done programming", flush=True)
x = port.read(1) # should return 'd'
print(x, flush=True)

if testReturnMode:
    rcv = port.read(1)
    retval = int.from_bytes(rcv, "little")
    print("FPGC returned: ", retval)
    sys.exit(retval)

