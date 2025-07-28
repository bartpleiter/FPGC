# Architecture diagrams

This document provides a comprehensive visual representation of the B32P2 CPU architecture, showing the pipeline stages, data flow, and module relationships.

## Complete CPU Architecture

```
                                 B32P2 CPU Architecture
                                   (6-Stage Pipeline)

┌─────────────────────────────────────────────────────────────────────────────────────────┐
│                                    HAZARD CONTROL                                       │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐                     │
│  │flush_FE1    │  │flush_FE2    │  │flush_REG    │  │flush_EXMEM1 │                     │
│  │stall_FE1    │  │stall_FE2    │  │stall_REG    │  │stall_EXMEM1 │                     │
│  └─────────────┘  └─────────────┘  └─────────────┘  └─────────────┘                     │
└─────────────────────────────────────────────────────────────────────────────────────────┘
        │                 │                 │                │
        ▼                 ▼                 ▼                ▼ 
┌─────────────┐    ┌─────────────┐    ┌───────────┐    ┌─────────────┐    ┌─────────────┐    ┌────────────┐
│     FE1     │───▶│     FE2     │───▶│     REG   │───▶│   EXMEM1    │───▶│   EXMEM2    │───▶│     WB     │
│ I-Cache/ROM │    │ I-Cache     │    │ Register  │    │ Execute &   │    │ Multi-cycle │    │ Writeback  │
│ Fetch       │    │ Miss        │    │ Read      │    │ D-Cache     │    │ & D-Cache   │    │            │
│             │    │ Handling    │    │           │    │ Access      │    │ Miss        │    │            │
└─────────────┘    └─────────────┘    └───────────┘    └─────────────┘    └─────────────┘    └────────────┘

    
┌──────────────────────────────────────────────────────────────────────────────────────────────────────────┐
│                                    FORWARDING NETWORK                                                    │
│                                                                                                          │
│  EXMEM2 → EXMEM1: ALU results, Memory read data, Stack pop data, Multi-cycle ALU results                 │
│  WB → EXMEM1: Register writeback data                                                                    │
│  WB → REG: Register writeback data (write forwarding)                                                    │
│                                                                                                          │
└──────────────────────────────────────────────────────────────────────────────────────────────────────────┘

┌──────────────────────────────────────────────────────────────────────────────────────────────────────────┐
│                                  EXTERNAL INTERFACES                                                     │
│                                                                                                          │
│  ROM (Dual Port): Instruction fetch + Data access                                                        │
│  L1i Cache: Instruction cache for read hit detection                                                     │
│  L1d Cache: Data cache for read hit detectio                                                             │
│  Video Memory: VRAM32, VRAM8, VRAMPX for GPU communication                                               │
│  Cache Controller: DDR3 SDRAM interface via MIG 7                                                        │
│                                                                                                          │
└──────────────────────────────────────────────────────────────────────────────────────────────────────────┘
```



## Hazard Detection Logic

```
                     ┌─────────────────────────────────────┐
                     │          HAZARD DETECTOR            │
                     └─────────────────────────────────────┘
                                       │
        ┌──────────────────────────────┼──────────────────────────────┐
        │                              │                              │
        ▼                              ▼                              ▼
┌─────────────┐                 ┌─────────────┐                ┌─────────────┐
│   DATA      │                 │  CONTROL    │                │ STRUCTURAL  │
│  HAZARDS    │                 │  HAZARDS    │                │  HAZARDS    │
└─────────────┘                 └─────────────┘                └─────────────┘
        │                              │                              │
        ▼                              ▼                              ▼
┌─────────────┐                 ┌─────────────┐                ┌─────────────┐
│ Forwarding  │                 │Pipeline     │                │Multi-cycle  │
│ Network     │                 │Flush        │                │Stall Logic  │
│ Control     │                 │Control      │                │             │
└─────────────┘                 └─────────────┘                └─────────────┘
```

## Cache Architecture Integration

```
                L1 Instruction Cache                           L1 Data Cache
    ┌─────────────────────────────────────────┐       ┌─────────────────────────────────────────┐
    │            CPU Pipeline                 │       │            CPU Pipeline                 │
    │  ┌─────────┐         ┌─────────┐        │       │  ┌─────────┐         ┌─────────┐        │
    │  │   FE1   │────────▶│   FE2   │        │       │  │ EXMEM1  │────────▶│ EXMEM2  │        │
    │  └─────────┘         └─────────┘        │       │  └─────────┘         └─────────┘        │
    │       │                   │             │       │       │                   │             │
    │       ▼                   ▼             │       │       ▼                   ▼             │
    │  ┌─────────┐         ┌─────────┐        │       │  ┌─────────┐         ┌─────────┐        │
    │  │L1i Cache│         │ Hit/Miss│        │       │  │L1d Cache│         │ Hit/Miss│        │
    │  │ Access  │         │ Detect  │        │       │  │ Access  │         │ Detect  │        │
    │  └─────────┘         └─────────┘        │       │  └─────────┘         └─────────┘        │
    └─────────────────────────────────────────┘       └─────────────────────────────────────────┘
                      │                                                 │
                      ▼                                                 ▼
    ┌───────────────────────────────────────────────────────────────────────────────────────────┐
    │                                     Cache Controller                                      │
    │                                                                                           │
    │  ┌─────────┐    ┌─────────┐    ┌──────┐            ┌─────────┐    ┌─────────┐    ┌──────┐ │
    │  │Miss     │    │ DRAM    │    │Cache │            │Miss     │    │ DRAM    │    │Cache │ │
    │  │Handling │───▶│Interface│───▶│Update│            │Handling │───▶│Interface│───▶│Update│ │
    │  └─────────┘    └─────────┘    └──────┘            └─────────┘    └─────────┘    └──────┘ │
    └───────────────────────────────────────────────────────────────────────────────────────────┘
                                                 │
                                                 ▼
    ┌─────────────────────────────────────────────────────────────────────────────────────────┐
    │                               MIG 7 DDR3 Controller                                     │
    │Complex Xilinx IP, could be replaced by simpler SDRAM controller for simpler memory types│
    │                                                                                         │
    └─────────────────────────────────────────────────────────────────────────────────────────┘
                                                 │
                                                 ▼
                                           ┌─────────────┐
                                           │   DDR3      │
                                           │   SDRAM     │
                                           │   Memory    │
                                           └─────────────┘
```
