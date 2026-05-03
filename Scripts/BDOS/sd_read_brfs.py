#!/usr/bin/env python3
"""
Read a BRFS v2 filesystem from an FPGC SD card and extract all
files and directories to the host filesystem.

Usage:
    python3 Scripts/BDOS/sd_read_brfs.py /dev/sda
    python3 Scripts/BDOS/sd_read_brfs.py /dev/sda -o /tmp/sdcard-dump

The BRFS partition starts at byte 0 of the device with this layout:
    0x00000  superblock  (16 words = 64 bytes)
    0x01000  FAT         (total_blocks words)
    0x10000  data blocks (total_blocks × words_per_block words)

The FPGC B32P3 CPU packs characters into 32-bit words in big-endian
order (char 0 at bits 24-31, char 1 at bits 16-23, etc.) — see
brfs_compress_string() in brfs.c.  Word VALUES on the SD card may be
in either LE or BE byte order depending on the DMA/SPI path; the
script auto-detects this from the BRFS magic word.
"""

import argparse
import os
import shutil
import struct
import sys

# ── BRFS constants (must match brfs.h) ──────────────────────────────

BRFS_MAGIC              = 0x32465242   # 'BRF2' LE
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
        raise IOError(
            f'Short read at 0x{byte_addr:x}: got {len(raw)}, '
            f'expected {n_words * 4}')
    return list(struct.unpack(_word_fmt(n_words, big_endian), raw))


def words_to_bytes(words):
    """Pack word values to bytes using the FPGC's big-endian char packing.

    The FPGC stores char[0] at bits 24-31 (MSB) of each word — see
    brfs_compress_string().  Regardless of how word values are stored
    on the SD card (LE or BE), the logical byte order within a word
    is always big-endian.
    """
    return struct.pack('>%dI' % len(words), *words)


def words_to_string(words):
    """Extract a NUL-terminated ASCII string from packed word array."""
    raw = words_to_bytes(words)
    nul = raw.find(b'\x00')
    if nul >= 0:
        raw = raw[:nul]
    return raw.decode('ascii', errors='replace')


# ── BRFS reader ──────────────────────────────────────────────────────

class BRFSReader:
    def __init__(self, path):
        self.f = open(path, 'rb')
        self.big_endian = False
        self._detect_endianness()
        self._read_superblock()
        self._read_fat()

    # -- low-level --

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
                f'BRFS magic not found (got LE 0x{le:08x}, BE 0x{be:08x}).  '
                f'Is this an FPGC SD card?')

    def _rw(self, addr, n):
        return read_words(self.f, addr, n, self.big_endian)

    # -- superblock / FAT --

    def _read_superblock(self):
        w = self._rw(FLASH_SUPERBLOCK_ADDR, BRFS_SUPERBLOCK_SIZE)
        self.total_blocks    = w[1]
        self.words_per_block = w[2]
        self.label           = words_to_string(w[3:13])
        self.version         = w[13]
        self.bytes_per_block = self.words_per_block * 4

    def _read_fat(self):
        self.fat = self._rw(FLASH_FAT_ADDR, self.total_blocks)

    # -- data blocks --

    def _block_addr(self, blk_idx):
        return FLASH_DATA_ADDR + blk_idx * self.bytes_per_block

    def _read_block_words(self, blk_idx):
        return self._rw(self._block_addr(blk_idx), self.words_per_block)

    def _read_block_bytes(self, blk_idx):
        """Read one data block as raw bytes from the SD card.

        File data uses the CPU's native LE byte addressing, so raw
        bytes on the SD card are already in the correct order.  This
        is different from filenames, which are packed with explicit
        big-endian shifts in brfs_compress_string().
        """
        self.f.seek(self._block_addr(blk_idx))
        return self.f.read(self.bytes_per_block)

    # -- file data --

    def read_file(self, fat_idx, filesize):
        """Follow the FAT chain and return *filesize* bytes of file data."""
        parts = []
        remaining = filesize
        cur = fat_idx
        while cur != BRFS_FAT_EOF and cur != BRFS_FAT_FREE and remaining > 0:
            raw = self._read_block_bytes(cur)
            take = min(len(raw), remaining)
            parts.append(raw[:take])
            remaining -= take
            cur = self.fat[cur]
        return b''.join(parts)

    # -- directory parsing --

    def read_dir(self, fat_idx):
        """Return a list of (name, flags, fat_idx, filesize) for one dir block."""
        words = self._read_block_words(fat_idx)
        n_entries = self.words_per_block // BRFS_DIR_ENTRY_SIZE
        entries = []
        for i in range(n_entries):
            off = i * BRFS_DIR_ENTRY_SIZE
            ew  = words[off:off + BRFS_DIR_ENTRY_SIZE]
            name     = words_to_string(ew[0:4])
            # ew[4] = modify_date (unused here)
            flags    = ew[5]
            f_idx    = ew[6]
            fsize    = ew[7]
            if name and name not in ('.', '..'):
                entries.append((name, flags, f_idx, fsize))
        return entries

    # -- recursive extraction --

    def extract(self, out_dir, fat_idx=0, rel_path=''):
        """Walk the directory tree and write files to *out_dir*."""
        for name, flags, f_idx, fsize in self.read_dir(fat_idx):
            rel  = os.path.join(rel_path, name)
            host = os.path.join(out_dir, rel)

            if flags & BRFS_FLAG_DIRECTORY:
                os.makedirs(host, exist_ok=True)
                print(f'  DIR  {rel}/')
                self.extract(out_dir, f_idx, rel)
            else:
                os.makedirs(os.path.dirname(host) or '.', exist_ok=True)
                data = self.read_file(f_idx, fsize)
                with open(host, 'wb') as fp:
                    fp.write(data)
                print(f'  FILE {rel}  ({fsize} bytes)')

    def close(self):
        self.f.close()

    def print_info(self):
        used = sum(1 for b in self.fat if b != BRFS_FAT_FREE)
        free = sum(1 for b in self.fat if b == BRFS_FAT_FREE)
        total_data = self.total_blocks * self.bytes_per_block
        print(f'BRFS v{self.version}  label="{self.label}"')
        print(f'  {self.total_blocks} blocks × {self.bytes_per_block} '
              f'bytes/block  ({total_data} bytes total)')
        print(f'  {used} blocks used, {free} blocks free')
        endian = 'big-endian' if self.big_endian else 'little-endian'
        print(f'  on-disk word order: {endian}')


# ── main ─────────────────────────────────────────────────────────────

def main():
    parser = argparse.ArgumentParser(
        description='Extract an FPGC BRFS v2 filesystem from an SD card')
    parser.add_argument(
        'device',
        help='Block device or image file  (e.g. /dev/sda)')
    parser.add_argument(
        '-o', '--output',
        default=os.path.join(os.path.dirname(__file__), '..', '..', 'Files',
                             'BRFS-sd-transfer'),
        help='Output directory  (default: Files/BRFS-sd-transfer)')
    args = parser.parse_args()

    if not os.path.exists(args.device):
        print(f'Error: {args.device} not found', file=sys.stderr)
        sys.exit(1)

    try:
        reader = BRFSReader(args.device)
    except (IOError, ValueError) as e:
        print(f'Error: {e}', file=sys.stderr)
        sys.exit(1)

    reader.print_info()

    out = os.path.normpath(args.output)
    if os.path.exists(out):
        shutil.rmtree(out)
    os.makedirs(out)
    print(f'\nExtracting to {out}/ ...')
    reader.extract(out)
    reader.close()

    # Currently commented out as the user is expected to run chown on /dev/sdx instead
    # # Fix ownership when run under sudo
    # sudo_uid = os.environ.get('SUDO_UID')
    # sudo_gid = os.environ.get('SUDO_GID')
    # if sudo_uid and sudo_gid:
    #     uid, gid = int(sudo_uid), int(sudo_gid)
    #     for root, dirs, files in os.walk(out):
    #         os.chown(root, uid, gid)
    #         for d in dirs:
    #             os.chown(os.path.join(root, d), uid, gid)
    #         for f in files:
    #             os.chown(os.path.join(root, f), uid, gid)

    print('Done.')


if __name__ == '__main__':
    main()
