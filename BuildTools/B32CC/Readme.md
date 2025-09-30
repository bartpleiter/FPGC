# B32CC - Modified SmallerC Compiler

This directory contains a modified version of SmallerC, originally created by Alexey Frunze.

## Original Source

- **Original Project**: SmallerC
- **Original Author**: Alexey Frunze
- **Original License**: BSD 2-Clause (see ORIGINAL_LICENSE.txt)
- **Original Repository**: https://github.com/alexfru/SmallerC

## Modifications

This version has been modified for use in the FPGC project. Key changes include:

- Removing everything that is not needed for targeting the B32P2 ISA
- Modifying the MIPS backend to target B32P2 Assembly
- Modifications to make the compiler run on the FPGC itself under the custom BDOS operating system, while still being able to run on Linux

## Licensing

- The original SmallerC code is licensed under the BSD 2-Clause license
- All modifications and the combined work are licensed under GPL v3 (see LICENSE in project root)
