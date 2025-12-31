# Cache

As the main performance bottleneck slowing down the CPU is RAM latency, caches are essential to keep the CPU fed with instructions and data. The FPGC contains separate L1 instruction (L1I) and L1 data (L1D) caches. Using cache lines of 8 words aligns well with SDRAM burst access patterns, making memory operations much more efficient - especially since DRAM is very fast for sequential access.

## Specifications

The L1I and L1D caches each contain:

- 128 lines of 8 words (256 bits) for 4KiB or 1024 words of data per cache
- Direct-mapped structure for simplicity
- 14 bits per line for tag (supporting 64MiW / 256MiB SDRAM)
- 1 valid bit per line
- Dual port access for CPU pipeline and cache controller

### Cache Line Format (271 bits total)

```text
[270:15] - Cache line data (256 bits = 8 × 32-bit words)
[14:1]   - Tag (14 bits)
[0]      - Valid bit
```

## Address Mapping

The cache is designed to work with up to 64 MiW (256 MiB) of SDRAM, using 26-bit word addresses. With 8 words per cache line, the line addresses are 23 bits.

### CPU Address Breakdown (32 bits)

- `[31:26]`: High-order bits (beyond SDRAM range)
- `[25:24]`: Upper address bits (beyond current SDRAM)
- `[23:10]`: Cache Tag (14 bits)
- `[9:3]`: Cache Index (7 bits) - selects one of 128 cache lines
- `[2:0]`: Word Offset (3 bits) - selects word within cache line

### Address Mapping Example

For address `0x001234`:
```text
Word offset:  0x001234[2:0]   = 4  → Word 4 in cache line
Cache index:  0x001234[9:3]   = 70 → Cache line 70
Cache tag:    0x001234[23:10] = 4  → Tag to match
```

## Cache Strategy

### L1I Cache (Instruction)

The L1I cache is accessed during the IF (Instruction Fetch) stage:

1. **IF Stage**: The cache line address `PC[9:3]` is sent to the cache BRAM
2. **Next cycle**: Cache tag and data are available for comparison
3. **Hit**: Instruction is extracted using word offset `PC[2:0]` and passed to ID stage
4. **Miss**: Pipeline stalls, cache controller fetches line from SDRAM

The L1I cache uses registered stall signals to break timing critical paths - the tag comparison result is registered before being used as a stall signal.

### L1D Cache (Data)

The L1D cache is accessed during memory operations:

1. **EX Stage**: Memory address is calculated, cache line address sent to cache BRAM for reads
2. **MEM Stage**: Cache hit/miss is determined, read data is available on hit
3. **Write operations**: Go through cache controller to update both cache and SDRAM

A special cache line hazard detection prevents incorrect reads when back-to-back memory operations access different cache lines.

### L1D Cache Read Latency

Since the cache BRAM has 1-cycle read latency, when an instruction first enters the MEM stage, the pipeline must wait one cycle for the cache data to become valid. This is tracked with a `l1d_cache_read_done` flag that ensures proper timing.

## Cache Controller

The cache controller handles cache misses and write operations for both L1I and L1D caches:

### Read Miss Handling

1. Cache miss detected (tag mismatch or invalid line)
2. Pipeline stalls
3. Cache controller requests data from SDRAM controller
4. Full 8-word cache line fetched from SDRAM
5. Cache line written to cache BRAM with valid bit set
6. Requested word returned to CPU
7. Pipeline resumes

### Write Handling

All writes go through the cache controller to ensure cache coherency:

1. Write request sent to cache controller
2. Cache controller writes to SDRAM via SDRAM controller
3. If address is cached, cache line is updated
4. Pipeline stalls until write completes

### Clear Cache Operation

The `CCACHE` instruction triggers a cache clear operation:

1. Cache controller enters clear mode
2. All L1I cache lines have their valid bits cleared
3. All L1D cache lines have their valid bits cleared
4. Pipeline resumes when complete

This is essential when loading programs into RAM, as the L1I cache may contain stale instructions that need to be invalidated.

## Cache Controller State Machine

```text
                     ┌──────────┐
                     │   IDLE   │◀────────────────────┐
                     └────┬─────┘                     │
                          │                          │
            ┌─────────────┴─────────────┐            │
            ▼                           ▼            │
    ┌──────────────┐            ┌──────────────┐     │
    │  L1I Miss    │            │  L1D Miss/   │     │
    │  Handling    │            │  Write       │     │
    └──────┬───────┘            └──────┬───────┘     │
           │                           │            │
           ▼                           ▼            │
    ┌──────────────┐            ┌──────────────┐     │
    │ SDRAM Read   │            │ SDRAM R/W    │     │
    │ Request      │            │ Request      │     │
    └──────┬───────┘            └──────┬───────┘     │
           │                           │            │
           ▼                           ▼            │
    ┌──────────────┐            ┌──────────────┐     │
    │ Update L1I   │            │ Update L1D   │     │
    │ Cache Line   │            │ Cache Line   │     │
    └──────┬───────┘            └──────┬───────┘     │
           │                           │            │
           └───────────────────────────┴────────────┘
```

## Integration with CPU Pipeline

### Stall Signals

The caches generate stall signals that affect the CPU pipeline:

- `cache_stall_if`: L1I cache miss (registered for timing)
- `cache_stall_mem`: L1D cache miss or write in progress
- `backend_stall`: Combined signal including cache stalls, multi-cycle ALU, and Memory Unit

### Cache Line Hazard

When consecutive memory operations target different SDRAM cache lines, a stall is needed:

```text
Instruction N:   READ from address 0x100 (cache line 32)
Instruction N+1: READ from address 0x200 (cache line 64) ← Stall needed
```

This is detected by comparing the cache line indices of EX and MEM stage memory operations. A timing-optimized 10-bit adder calculates the EX stage cache line without the full 32-bit address.

## Performance Considerations

### Cache Hit Latency

- **L1I hit**: 1 cycle (instruction available after tag compare)
- **L1D hit**: 2 cycles (address in EX, data in MEM, result in WB)

### Cache Miss Latency

Cache miss latency depends on the SDRAM controller timing:

- SDRAM burst read: ~8-10 cycles typical
- Additional cycles for cache line update

### Optimization Tips

1. **Sequential access**: SDRAM is fastest for sequential reads within a cache line
2. **Spatial locality**: Keep related data within the same cache line (8 words / 32 bytes)
3. **Loop optimization**: Keep hot loops under 1KiW (4KiB) for full L1I residency
4. **Cache clear timing**: Execute `CCACHE` after loading code to RAM, before jumping to it
