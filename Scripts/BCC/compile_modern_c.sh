#!/bin/bash
# Script to compile C code using the modern toolchain:
#   cproc (C frontend) → QBE (backend) → ASMPY (assembler) → binary
#
# Supports mixed .c and .asm inputs. Assembly files (like crt0 startup code)
# pass through to the linker directly; C files go through cproc → QBE first.
#
# Usage:
#   # Bare metal program (with crt0 startup):
#   ./compile_modern_c.sh Software/ASM/crt0/crt0_baremetal.asm program.c -h -o output.bin
#
#   # UserBDOS program (relocatable):
#   ./compile_modern_c.sh Software/ASM/crt0/crt0_userbdos.asm program.c -h -i -o output.bin
#
#   # BDOS kernel (with syscall vector):
#   ./compile_modern_c.sh Software/ASM/crt0/crt0_bdos.asm src1.c src2.c -h -s -o output.bin
#
#   # Raw C (no startup, e.g. for testing):
#   ./compile_modern_c.sh program.c -o output.bin

set -e
set -o pipefail   # propagate errors from `cpp | cproc | qbe` so a cproc
                  # error doesn't get silently swallowed by qbe's exit 0

# Tool paths (relative to project root)
CPROC="BuildTools/cproc/output/cproc-qbe"
QBE="BuildTools/QBE/output/qbe"
CPP="cpp"

# Libc include path (relative to project root)
LIBC_INCLUDE="Software/C/libc/include"

# Parse arguments
HEADER_FLAG=""
INDEPENDENT_FLAG=""
SYSCALL_FLAG=""
OUTPUT=""
INPUT_FILES=()
INCLUDE_DIRS=()
CPP_DEFINES=()
USE_LIBC=0
OFFSET_ADDR=""

while [[ $# -gt 0 ]]; do
    case "$1" in
        -h|--header)
            HEADER_FLAG="-h"
            shift
            ;;
        -i|--independent)
            INDEPENDENT_FLAG="-i"
            shift
            ;;
        -s|--syscall)
            SYSCALL_FLAG="-s"
            shift
            ;;
        -o|--output)
            OUTPUT="$2"
            shift 2
            ;;
        -I)
            INCLUDE_DIRS+=("$2")
            shift 2
            ;;
        -I*)
            INCLUDE_DIRS+=("${1#-I}")
            shift
            ;;
        --libc)
            USE_LIBC=1
            shift
            ;;
        --offset)
            OFFSET_ADDR="$2"
            shift 2
            ;;
        -D)
            CPP_DEFINES+=("-D$2")
            shift 2
            ;;
        -D*)
            CPP_DEFINES+=("$1")
            shift
            ;;
        *.c|*.asm)
            INPUT_FILES+=("$1")
            shift
            ;;
        *)
            echo "Unknown argument: $1"
            echo "Usage: $0 [crt0.asm] <source.c> [source2.c ...] [-o output.bin] [-h] [-i] [-s] [--libc] [-I dir]"
            exit 1
            ;;
    esac
done

if [ ${#INPUT_FILES[@]} -eq 0 ]; then
    echo "Usage: $0 [crt0.asm] <source.c> [source2.c ...] [-o output.bin] [-h] [-i] [-s] [--libc] [-I dir]"
    exit 1
fi

# Default output
if [ -z "$OUTPUT" ]; then
    OUTPUT="Software/ASM/Output/code.bin"
fi

# Verify tools exist
for tool in "$CPROC" "$QBE"; do
    if [ ! -f "$tool" ]; then
        echo "Error: $tool not found. Run 'make qbe cproc' first."
        exit 1
    fi
done

# Activate virtual environment
source .venv/bin/activate

# Create temp directory for intermediate files
TMPDIR=$(mktemp -d)
trap "rm -rf $TMPDIR" EXIT

# Build cpp include flags
CPP_FLAGS="-nostdinc -P"
if [ "$USE_LIBC" -eq 1 ]; then
    CPP_FLAGS="$CPP_FLAGS -I$LIBC_INCLUDE"
fi
for dir in "${INCLUDE_DIRS[@]}"; do
    CPP_FLAGS="$CPP_FLAGS -I$dir"
done
for def in "${CPP_DEFINES[@]}"; do
    CPP_FLAGS="$CPP_FLAGS $def"
done

ASM_FILES=()
echo "=== Compiling sources ==="
for input_file in "${INPUT_FILES[@]}"; do
    base=$(basename "${input_file%.*}")
    if [[ "$input_file" == *.asm ]]; then
        # Assembly file — copy directly to temp dir (preserves link order)
        asm_file="$TMPDIR/${base}.asm"
        cp "$input_file" "$asm_file"
        echo "  $input_file (assembly, pass-through)"
    elif [[ "$input_file" == *.c ]]; then
        # C file — preprocess through cpp, then compile through cproc → QBE
        asm_file="$TMPDIR/${base}.asm"
        echo "  $input_file → $asm_file"
        "$CPP" $CPP_FLAGS "$input_file" | "$CPROC" -t b32p3 | "$QBE" > "$asm_file"
    fi
    ASM_FILES+=("$asm_file")
done

echo "=== Assembling ==="
LIST_OUTPUT="${OUTPUT%.bin}.list"
mkdir -p "$(dirname "$OUTPUT")"

if [ ${#ASM_FILES[@]} -eq 1 ]; then
    # Single file: direct assembly
    ASMPY_FLAGS=""
    [ -n "$HEADER_FLAG" ] && ASMPY_FLAGS="$ASMPY_FLAGS -h"
    [ -n "$INDEPENDENT_FLAG" ] && ASMPY_FLAGS="$ASMPY_FLAGS -i"
    [ -n "$SYSCALL_FLAG" ] && ASMPY_FLAGS="$ASMPY_FLAGS -s"
    asmpy "${ASM_FILES[0]}" "$LIST_OUTPUT" $ASMPY_FLAGS
else
    # Multi-file: link then assemble
    LINKER_FLAGS=""
    [ -n "$HEADER_FLAG" ] && LINKER_FLAGS="$LINKER_FLAGS -H"
    [ -n "$INDEPENDENT_FLAG" ] && LINKER_FLAGS="$LINKER_FLAGS -i"
    [ -n "$SYSCALL_FLAG" ] && LINKER_FLAGS="$LINKER_FLAGS -s"
    [ -n "$OFFSET_ADDR" ] && LINKER_FLAGS="$LINKER_FLAGS -o $OFFSET_ADDR"
    python -m asmpy.linker "${ASM_FILES[@]}" "$LIST_OUTPUT" $LINKER_FLAGS
fi

echo "=== Converting to binary ==="
perl -ne 'chomp; print pack("V", oct("0b$_"))' < "$LIST_OUTPUT" > "$OUTPUT"

deactivate

echo ""
echo "Compilation complete!"
echo "  Binary output: $OUTPUT ($(wc -c < "$OUTPUT") bytes)"
