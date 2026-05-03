#!/usr/bin/env python3
"""
Write files from a host directory into a BRFS v2 filesystem on an
FPGC SD card, replacing all existing contents.

Usage:
    python3 Scripts/BDOS/sd_write_brfs.py /dev/sda
    python3 Scripts/BDOS/sd_write_brfs.py /dev/sda -i /path/to/source

The script reads the existing superblock to obtain the FS geometry
(total_blocks, words_per_block), then re-creates the FAT and data
region from the host directory tree.
"""

import argparse
import os
import struct
import sys

# ── BRFS constants (must match brfs.h) ──────────────────────────────

BRFS_MAGIC              = 0x32465242
BRFS_VERSION            = 2
BRFS_SUPERBLOCK_SIZE    = 16           # words
BRFS_DIR_ENTRY_SIZE     = 8            # words per dir entry
BRFS_MAX_FILENAME_LEN   = 16           # bytes (4 words × 4 bytes)

BRFS_FAT_FREE           = 0x00000000
BRFS_FAT_EOF            = 0xFFFFFFFF

BRFS_FLAG_DIRECTORY     = 0x01

# Byte offsets of on-disk regions
FLASH_SUPERBLOCK_ADDR   = 0x00000
FLASH_FAT_ADDR          = 0x01000
FLASH_DATA_ADDR         = 0x10000


# ── Helpers ──────────────────────────────────────────────────────────

def _word_fmt(n, big_endian=False):
    return ('>%dI' if big_endian else '<%dI') % n


def read_words(f, byte_addr, n_words, big_endian=False):
    """Read *n_words* 32-bit words from *byte_addr*."""
    f.seek(byte_addr)
    raw = f.read(n_words * 4)
    if len(raw) < n_words * 4:
        raise IOError(f'Short read at 0x{byte_addr:x}')
    return list(struct.unpack(_word_fmt(n_words, big_endian), raw))


def write_words(f, byte_addr, words, big_endian=False):
    """Write a list of 32-bit word values at *byte_addr*."""
    f.seek(byte_addr)
    f.write(struct.pack(_word_fmt(len(words), big_endian), *words))


def compress_string(s):
    """Pack a string into 4 words using FPGC's big-endian char packing.

    Matches brfs_compress_string(): char[i] is placed at
    bits (24 - (i%4)*8) of the corresponding word.
    """
    words = [0, 0, 0, 0]
    for i, ch in enumerate(s[:BRFS_MAX_FILENAME_LEN]):
        wi = i // 4
        bi = i % 4
        words[wi] |= (ord(ch) & 0xFF) << (24 - bi * 8)
    return words


def bytes_to_data_words(data, words_per_block):
    """Convert raw file bytes into a list of 32-bit words (LE packing).

    File data on the FPGC uses native LE byte addressing, so byte[0]
    goes to the LSB of word[0].  Pad to words_per_block with zeros.
    """
    # Pad to word boundary
    padded = data + b'\x00' * ((4 - len(data) % 4) % 4)
    n = len(padded) // 4
    words = list(struct.unpack('<%dI' % n, padded))
    # Pad to full block
    while len(words) < words_per_block:
        words.append(0)
    return words


# ── BRFS writer ──────────────────────────────────────────────────────

class BRFSWriter:
    def __init__(self, path):
        self.path = path
        self.f = open(path, 'r+b')
        self.big_endian = False
        self._detect_endianness()
        self._read_superblock()
        # In-memory FAT and data blocks
        self.fat = [BRFS_FAT_FREE] * self.total_blocks
        self.data = {}  # block_idx → list of words (words_per_block)
        self.next_free = 1  # block 0 = root directory

    def _detect_endianness(self):
        self.f.seek(FLASH_SUPERBLOCK_ADDR)
        raw4 = self.f.read(4)
        le = struct.unpack('<I', raw4)[0]
        be = struct.unpack('>I', raw4)[0]
        if le == BRFS_MAGIC:
            self.big_endian = False
        elif be == BRFS_MAGIC:
            self.big_endian = True
        else:
            raise ValueError(
                f'BRFS magic not found (got LE 0x{le:08x}, BE 0x{be:08x})')

    def _rw(self, addr, n):
        return read_words(self.f, addr, n, self.big_endian)

    def _ww(self, addr, words):
        write_words(self.f, addr, words, self.big_endian)

    def _read_superblock(self):
        w = self._rw(FLASH_SUPERBLOCK_ADDR, BRFS_SUPERBLOCK_SIZE)
        self.total_blocks    = w[1]
        self.words_per_block = w[2]
        self.bytes_per_block = self.words_per_block * 4
        self.label_words     = w[3:13]
        self.version         = w[13]
        self.sb_words        = w  # keep for re-writing

    def _alloc_block(self):
        """Allocate a free block, return its index."""
        while self.next_free < self.total_blocks:
            if self.fat[self.next_free] == BRFS_FAT_FREE:
                idx = self.next_free
                self.next_free += 1
                return idx
            self.next_free += 1
        raise RuntimeError('BRFS: no free blocks')

    def _alloc_chain(self, n_blocks):
        """Allocate a chain of *n_blocks*, return the first block index."""
        if n_blocks == 0:
            return None
        blocks = [self._alloc_block() for _ in range(n_blocks)]
        for i, b in enumerate(blocks):
            if i + 1 < len(blocks):
                self.fat[b] = blocks[i + 1]
            else:
                self.fat[b] = BRFS_FAT_EOF
        return blocks[0]

    def _make_dir_entry(self, name, fat_idx, filesize, flags):
        """Build an 8-word directory entry."""
        fn = compress_string(name)
        return fn + [0, flags, fat_idx, filesize]

    def _write_file_data(self, data):
        """Write raw file bytes into allocated blocks. Return first FAT idx."""
        if len(data) == 0:
            # Even empty files get one block
            blk = self._alloc_block()
            self.fat[blk] = BRFS_FAT_EOF
            self.data[blk] = [0] * self.words_per_block
            return blk

        n_blocks = (len(data) + self.bytes_per_block - 1) // self.bytes_per_block
        first = self._alloc_chain(n_blocks)

        cur = first
        offset = 0
        while cur != BRFS_FAT_EOF:
            chunk = data[offset:offset + self.bytes_per_block]
            self.data[cur] = bytes_to_data_words(chunk, self.words_per_block)
            offset += self.bytes_per_block
            cur = self.fat[cur]

        return first

    def _build_directory(self, host_dir, parent_fat_idx):
        """Recursively build a BRFS directory from a host directory.

        Returns the FAT index of this directory's block.
        """
        dir_block = self._alloc_block() if parent_fat_idx != -1 else 0
        self.fat[dir_block] = BRFS_FAT_EOF

        max_entries = self.words_per_block // BRFS_DIR_ENTRY_SIZE
        entries = []

        # "." and ".." entries
        dot_entry = self._make_dir_entry('.', dir_block, 0, BRFS_FLAG_DIRECTORY)
        parent_idx = parent_fat_idx if parent_fat_idx >= 0 else dir_block
        dotdot_entry = self._make_dir_entry('..', parent_idx, 0, BRFS_FLAG_DIRECTORY)
        entries.append(dot_entry)
        entries.append(dotdot_entry)

        # Scan host directory
        try:
            items = sorted(os.listdir(host_dir))
        except PermissionError:
            items = []

        for item in items:
            if len(entries) >= max_entries:
                print(f'  WARNING: directory full, skipping {item}')
                break

            full_path = os.path.join(host_dir, item)
            name = item[:BRFS_MAX_FILENAME_LEN - 1]  # leave room for null

            if os.path.isdir(full_path):
                sub_fat = self._build_directory(full_path, dir_block)
                entry = self._make_dir_entry(name, sub_fat, 0, BRFS_FLAG_DIRECTORY)
                entries.append(entry)
                print(f'  DIR  {os.path.relpath(full_path, self._src_root)}/')
            elif os.path.isfile(full_path):
                with open(full_path, 'rb') as fp:
                    file_data = fp.read()
                file_fat = self._write_file_data(file_data)
                entry = self._make_dir_entry(name, file_fat, len(file_data), 0)
                entries.append(entry)
                print(f'  FILE {os.path.relpath(full_path, self._src_root)}'
                      f'  ({len(file_data)} bytes)')

        # Build directory block (zero-filled, then entries)
        block = [0] * self.words_per_block
        for i, e in enumerate(entries):
            off = i * BRFS_DIR_ENTRY_SIZE
            block[off:off + BRFS_DIR_ENTRY_SIZE] = e
        self.data[dir_block] = block

        return dir_block

    def write(self, src_dir):
        """Build the filesystem from *src_dir* and write to the device."""
        self._src_root = src_dir

        # Reserve block 0 for root directory
        self.fat[0] = BRFS_FAT_EOF

        # Build directory tree
        self._build_directory(src_dir, -1)

        # Write FAT
        print(f'\nWriting FAT ({self.total_blocks} entries)...')
        self._ww(FLASH_FAT_ADDR, self.fat)

        # Write data blocks
        used = len(self.data)
        print(f'Writing {used} data blocks...')
        for blk_idx, words in self.data.items():
            addr = FLASH_DATA_ADDR + blk_idx * self.bytes_per_block
            self._ww(addr, words)

        # Zero out unused blocks that may have had old data
        zero_block = [0] * self.words_per_block
        zeroed = 0
        for blk_idx in range(self.total_blocks):
            if blk_idx not in self.data:
                addr = FLASH_DATA_ADDR + blk_idx * self.bytes_per_block
                self._ww(addr, zero_block)
                zeroed += 1
                if zeroed % 100 == 0:
                    sys.stdout.write(f'\r  Clearing unused blocks... {zeroed}')
                    sys.stdout.flush()
        if zeroed > 0:
            print(f'\r  Cleared {zeroed} unused blocks.      ')

        self.f.flush()

    def close(self):
        self.f.close()

    def print_info(self):
        used = sum(1 for b in self.fat if b != BRFS_FAT_FREE)
        free = sum(1 for b in self.fat if b == BRFS_FAT_FREE)
        print(f'  {used} blocks used, {free} blocks free')


# ── main ─────────────────────────────────────────────────────────────

def main():
    parser = argparse.ArgumentParser(
        description='Write files to an FPGC BRFS v2 filesystem on an SD card')
    parser.add_argument(
        'device',
        help='Block device or image file  (e.g. /dev/sda)')
    parser.add_argument(
        '-i', '--input',
        default=os.path.join(os.path.dirname(__file__), '..', '..', 'Files',
                             'BRFS-sd-transfer'),
        help='Source directory  (default: Files/BRFS-sd-transfer)')
    args = parser.parse_args()

    if not os.path.exists(args.device):
        print(f'Error: {args.device} not found', file=sys.stderr)
        sys.exit(1)

    src = os.path.normpath(args.input)
    if not os.path.isdir(src):
        print(f'Error: {src} is not a directory', file=sys.stderr)
        sys.exit(1)

    print(f'Reading superblock from {args.device}...')
    try:
        writer = BRFSWriter(args.device)
    except (IOError, ValueError) as e:
        print(f'Error: {e}', file=sys.stderr)
        sys.exit(1)

    print(f'BRFS v{writer.version}: {writer.total_blocks} blocks × '
          f'{writer.bytes_per_block} bytes/block')
    print(f'\nWriting contents of {src}/ to {args.device}...')
    writer.write(src)
    writer.print_info()
    writer.close()
    print('Done.')


if __name__ == '__main__':
    main()
