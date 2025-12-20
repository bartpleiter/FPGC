# Memory Map

The FPGC uses a memory map to map all CPU-accessible memory and I/O to the available address range.
This map is implemented in the Address Decoder of the CPU, and the Memory Unit (MU) for I/O devices.

Additionally, as the GPU also uses multiple types of (VRAM) memory, it also has a memory map. This serves more as an explanation on what is located in which type of memory at which addresses. These addresses are GPU only (the GPU side of the dual port VRAM memory).

## Features

There were certain goals when designing the memory map:

* Discontinuous addresses for more efficient addr comparison with less logic (no very wide AND gates)
* SDRAM starts at address 0 to make software compilation easier
* All addresses (of up to 32 bit words) fit within 27 bits, which means the max address is 0x7_FFF_FFF. This is mainly because the jump instruction has 27 bits as argument, although this should not matter for data memory. In practice 27 bits is more than enough for basically any use case of the FPGC.

## CPU memory map

This is the memory map for the CPU. Note that only SDRAM and ROM can be used as instruction memory.

```text
SDRAM
$0000000    +------------------------+
            |                        |
            |   SDRAM Main Memory    |
            |       (112MiW)         | $6FFFFFF
            +------------------------+

I/O
$7000000    +------------------------+
            |                        | 
            |          I/O           |
            |                        |
            | UART0 TX (USB)     $00 |
            | UART0 RX (USB)     $01 |
            | Timer1_val         $02 |
            | Timer1_ctrl        $03 |
            | Timer2_val         $04 |
            | Timer2_ctrl        $05 |
            | Timer3_val         $06 |
            | Timer3_ctrl        $07 |
            | SPI0_data (Flash1) $08 |
            | SPI0_CS            $09 |
            | SPI1_data (Flash2) $0A |
            | SPI1_CS            $0B |
            | SPI2_data (USB H1) $0C |
            | SPI2_CS            $0D |
            | Reserved           $0E |
            | SPI3_data (USB H2) $0F |
            | SPI3_CS            $10 |
            | Reserved           $11 |
            | SPI4_data (Eth)    $12 |
            | SPI4_CS            $13 |
            | Reserved           $14 |
            | SPI5_data (SD)     $15 |
            | SPI5_CS            $16 |
            | GPIO_mode          $17 |
            | GPIO_state         $18 |
            | Boot_mode          $19 |
            | Micros             $1A | $700001A
            +------------------------+

Single Cycle Memory
$7800000    +------------------------+
            |                        |
            |          ROM           |
            |         (1KiW)         | $78003FF
$7900000    +------------------------+
            |                        |
            |         VRAM32         |
            |                        |
            | $000                   |
            |     Pattern Table      |
            |                   $3FF |
            |                        |
            | $400                   |
            |     Palette Table      |
            |                   $41F |
            |                        | $790041F
$7A00000    +------------------------+
            |                        |
            |         VRAM8          |
            |                        |
            | $000                   |
            |      BG Tile Table     |
            |                  $7FF  |
            |                        |
            | $800                   |
            |     BG Color Table     |
            |                  $FFF  |
            |                        |
            | $1000                  |
            |    Window Tile Table   |
            |                  $17FF |
            |                        |
            | $1800                  |
            |    Window Color Table  |
            |                  $1FFF |
            | $2000                  |
            |       Parameters       |
            |                  $2001 |
            |                        | $7A02001
$7B00000    +------------------------+
            |                        |
            |       VRAMpixel        |
            |    (8bit -> 75 KiB)    | $7B12BFF
            +------------------------+
```

## GPU memory map

This memory map is only relevant for the GPU logic.

```text
VRAM32
$000  +------------------------+
      |                        |
      |     Pattern Table      |
      |                        | $3FF
$400  +------------------------+
      |                        |
      |     Palette Table      |
      |                        | $41F
      +------------------------+

VRAM8
$000  +------------------------+
      |                        |
      |     BG Tile Table      |
      |                        | $7FF
$800  +------------------------+
      |                        |
      |     BG Color Table     |
      |                        | $FFF
$1000 +------------------------+
      |                        |
      |   Window Tile Table    |
      |                        | $17FF
$1800 +------------------------+
      |                        |
      |   Window Color Table   |
      |                        | $1FFF
$2000 +------------------------+
      |                        |
      |       Parameters       |
      |                        | $2001
      +------------------------+

PixelVRAM
$000  +------------------------+
      |                        |
      |   8-bit color value    |
      |                        | $12BFF
      +------------------------+
```
