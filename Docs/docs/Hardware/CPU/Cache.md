# Cache (WIP)

As the main performance bottleneck slowing down the CPU (even at 50MHz) is RAM, a cache is needed to keep the CPU fed with instructions and avoid stalls. Specifically, the FPGC contains a L1 instruction and L1 data cache. This cache is even more needed for newer types of memory like DDR3, which require a burst length of 8.

## Specifications

The L1i and L1d cache each contain:
- 128 lines of 8 words (256 bits) for 4KiB of memory for data
- Direct-mapped structure for simplicity
- 16 bits per line for tag
- 1 valid bit per line
- 1 dirty bit per line (unused in l1i cache)

## Cache strategy

Within the CPU pipeline, there are two stages for each memory type. For L1i, the first stage FE1 directly requests the relevant cache line from the l1i cache SRAM. In the next stage FE2, a check is done for a cache hit or cache miss. In case of a cache hit, the instruction is selected from the cache data and passed to the next stage. In case of a cache miss, the pipeline is stalled until the cache miss is handled and the resulting instruction is passed to the next stage. The same happens for L1d cache, where the cache is requested during the EXE stage, and cache hits or misses are handled in the MEM stage.

### Cache controller

During a cache miss on the L1i cache, or a write on the L1d cache, the cache controller is requested. The cache controller also has direct access to the cache SRAM as it is dual port, and is also connected to the SDRAM controller (MIG7 in case of DDR3 on the Artix 7 board I am currently using). For the L1i cache, in case of a cache miss the cache controller fetches the new cache line with the requested instruction from the SDRAM controller, then writes the new cache line with the valid bit set to the cache SRAM, after which it returns the requested instruction to the CPU pipeline. For a L1d there is an extra step, where the evicted cache line is first written back to SDRAM if the dirty bit is set. For writes on a cache hit, the cache line is read, updated and written back with the dirty bit set (which does mean there is an extra read as the line already has been read during the EXE stage, but this is a lot simpler and only costs a cycle).

!!! note
    Note that there is a separate cache controller for L1i and L1d

### Connection to the SDRAM controller

As the CPU is pipelined, both the L1i and L1d cache controllers could request the SDRAM at the same time. Therefore we need some kind of arbiter. Furtherfore, the MIG7 interface runs at a minimum speed of 100MHz (possibly 75MHz with some configuration change, but that might cause other issues). As the FPGC will likely run at 50MHz, there will also be need for some clock domain crossing. For both of these things a single module can be used as a kind of memory adapter, something that can be updated for different kinds of memory interfaces. This would mean you would only have to change this module if you were to use a different FPGA with a different type of memory

!!! warning
    This memory adapter still has to be written, lets see how well this works out for MIG7 and DDR3l, or if simple SDRAM would be the way to go
