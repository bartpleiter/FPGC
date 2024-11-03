# Memory map

The FPGC uses a memory map to map all CPU-accessible memory and I/O to the 27-bit address range (512MiB).
Currently, the map is hardcoded in the instruction and data memory unit of the CPU, and the Memory Unit (MU).

Additionally, as the GPU also uses multiple types of (VRAM) memory, it also has a memory map. This serves more as an explanation on what is located in which type of memory at which addresses. These addresses are GPU only (the GPU side of the dual port VRAM memory).

## Requirements

* Discontinuous addresses for more efficient addr comparison with less logic (no very wide AND gates)
* SDRAM starts at address 0 to make software compilation easier
* All addresses (of up to 32 bit words) fit within 27 bits, which means the max address is 0x7_FFF_FFF

## CPU memory map

As of writing, not all memory has been implemented yet, but are mapped in advance in case it will get added in the future. This is the case for VRAMsprite and SD Card Flash. Also, the I/O addresses and devices will change when the new PCB version is finished (currently written for PCB V3).

```text
SDRAM
$0000000    +------------------------+
            |                        |
            |   SDRAM Main Memory    |
            |        (32MiB)         | $07FFFFF
$0800000    +------------------------+
            |                        |
            |   SDRAM BRFS Memory    |
            |        (32MiB)         | $0FFFFFF
            +------------------------+

Single Cycle Memory
$1000000    +------------------------+
            |                        |
            |          ROM           |
            |         (4KiB)         | $10001FF
$1100000    +------------------------+
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
            |                        | $110041F
$1200000    +------------------------+
            |                        |
            |         VRAM8          |
            |                        |
            | $0420                  |
            |    BG Pattern Table    |
            |                  $07FF |
            |                        |
            | $0800                  |
            |    BG Palette Table    |
            |                  $0FFF |
            |                        |
            | $1000                  |
            |  Window Pattern Table  |
            |                  $17FF |
            |                        |
            | $1800                  |
            |  Window Palette Table  |
            |                  $1FFF |
            | $2000                  |
            |       Parameters       |
            |                  $2001 |
            |                        | $1202001
$1300000    +------------------------+
            |                        |
            |       VRAMsprite       |
            |                        | $13000FF
$1400000    +------------------------+
            |                        |
            |       VRAMpixel        |
            |    (8 bit -> 75 KiB)   | $1412BFF
            +------------------------+

SPI Flash
$2000000    +------------------------+
            |                        |
            |       SPI Flash        |
            |        (64 MiB)        | $2FFFFFF
            +------------------------+

I/O
$3000000    +------------------------+
            |                        | 
            |          I/O           |
            |        (PCB V3)        | 
            |                        |
            | UART0 RX (MAIN)    $00 |
            | UART0 TX (MAIN)    $01 |
            | Unused             $02 |
            | Unused             $03 |
            | UART2 RX (EXT)     $04 |
            | UART2 TX (EXT)     $05 |
            | SPI0   (FLASH)     $06 |
            | SPI0_CS            $07 |
            | SPI0_ENABLE        $08 |
            | SPI1 (CH376_0)     $09 |
            | SPI1_CS            $0A |
            | SPI1_nINT          $0B |
            | SPI2 (CH376_1)     $0C |
            | SPI2_CS            $0D |
            | SPI2_nINT          $0E |
            | SPI3   (W5500)     $0F |
            | SPI3_CS            $10 |
            | SPI3_INT           $11 |
            | SPI4     (EXT)     $12 |
            | SPI4_CS            $13 |
            | SPI4_GP            $14 |
            | GPIO               $15 |
            | GPIO_DIR           $16 |
            | Timer1_val         $17 |
            | Timer1_ctrl        $18 |
            | Timer2_val         $19 |
            | Timer2_ctrl        $1A |
            | Timer3_val         $1B |
            | Timer3_ctrl        $1C |
            | Unused             $1D |
            | PS/2 Keyboard      $1E |
            | BOOT_MODE          $1F |
            | FP_div_writea      $20 |
            | FP_div_divb        $21 |
            | I_div_writea       $22 |
            | I_div_divbs        $23 |
            | I_div_divbu        $24 |
            | I_div_mods         $25 |
            | I_div_modu         $26 |
            | halfRes            $27 |
            | millis             $28 | $3000028
            +------------------------+

SD Card Flash
$4000000    +------------------------+
            |                        |
            |     SD Card Flash      |
            |       (256 MiB)        | $7FFFFFF
            +------------------------+
```

## GPU memory map

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

SpriteVRAM
$00   +------------------------+
      |                        |
      |    %0: X pos           |
      |    %1: Y pos           |
      |    %2: Tile            |
      |    %3: Palette+flags   |
      |                        | $FF
      +------------------------+

PixelVRAM
$000  +------------------------+
      |                        |
      |   8-bit color value    |
      |                        | $12BFF
      +------------------------+
```
