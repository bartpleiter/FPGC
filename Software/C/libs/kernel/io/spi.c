#include "libs/kernel/io/spi.h"

void spi_0_select()
{
    asm(
        "load32 0x7000009 r11 ; r11 = SPI0 cs register"
        "write 0 r11 r0       ; Set cs low"
    );
}

void spi_0_deselect()
{
    asm(
        "load32 0x7000009 r11 ; r11 = SPI0 cs register"
        "load 1 r12           ; r12 = 1"
        "write 0 r11 r12      ; Set cs high"
    );

}

int spi_0_transfer(int data)
{
    int retval = 0;
    asm(
        "load32 0x7000008 r11 ; r11 = SPI0 data register"
        "write 0 r11 r4       ; Write data to SPI0"
        "read 0 r11 r11       ; Read received data from SPI0"
        "write -1 r14 r11     ; Write received data to stack for return"
    );
    return retval;
}

void spi_1_select()
{
    asm(
        "load32 0x700000B r11 ; r11 = SPI1 cs register"
        "write 0 r11 r0       ; Set cs low"
    );
}

void spi_1_deselect()
{
    asm(
        "load32 0x700000B r11 ; r11 = SPI1 cs register"
        "load 1 r12           ; r12 = 1"
        "write 0 r11 r12      ; Set cs high"
    );

}

int spi_1_transfer(int data)
{
    int retval = 0;
    asm(
        "load32 0x700000A r11 ; r11 = SPI1 data register"
        "write 0 r11 r4       ; Write data to SPI1"
        "read 0 r11 r11       ; Read received data from SPI1"
        "write -1 r14 r11     ; Write received data to stack for return"
    );
    return retval;
}

void spi_2_select()
{
    asm(
        "load32 0x700000D r11 ; r11 = SPI2 cs register"
        "write 0 r11 r0       ; Set cs low"
    );
}

void spi_2_deselect()
{
    asm(
        "load32 0x700000D r11 ; r11 = SPI2 cs register"
        "load 1 r12           ; r12 = 1"
        "write 0 r11 r12      ; Set cs high"
    );
}

int spi_2_transfer(int data)
{
    int retval = 0;
    asm(
        "load32 0x700000C r11 ; r11 = SPI2 data register"
        "write 0 r11 r4       ; Write data to SPI2"
        "read 0 r11 r11       ; Read received data from SPI2"
        "write -1 r14 r11     ; Write received data to stack for return"
    );
    return retval;
}

void spi_3_select()
{
    asm(
        "load32 0x7000010 r11 ; r11 = SPI3 cs register"
        "write 0 r11 r0       ; Set cs low"
    );
}

void spi_3_deselect()
{
    asm(
        "load32 0x7000010 r11 ; r11 = SPI3 cs register"
        "load 1 r12           ; r12 = 1"
        "write 0 r11 r12      ; Set cs high"
    );
}

int spi_3_transfer(int data)
{
    int retval = 0;
    asm(
        "load32 0x700000F r11 ; r11 = SPI3 data register"
        "write 0 r11 r4       ; Write data to SPI3"
        "read 0 r11 r11       ; Read received data from SPI3"
        "write -1 r14 r11     ; Write received data to stack for return"
    );
    return retval;
}

void spi_4_select()
{
    asm(
        "load32 0x7000013 r11 ; r11 = SPI4 cs register"
        "write 0 r11 r0       ; Set cs low"
    );
}

void spi_4_deselect()
{
    asm(
        "load32 0x7000013 r11 ; r11 = SPI4 cs register"
        "load 1 r12           ; r12 = 1"
        "write 0 r11 r12      ; Set cs high"
    );
}

int spi_4_transfer(int data)
{
    int retval = 0;
    asm(
        "load32 0x7000012 r11 ; r11 = SPI4 data register"
        "write 0 r11 r4       ; Write data to SPI4"
        "read 0 r11 r11       ; Read received data from SPI4"
        "write -1 r14 r11     ; Write received data to stack for return"
    );
    return retval;
}

void spi_5_select()
{
    asm(
        "load32 0x7000016 r11 ; r11 = SPI5 cs register"
        "write 0 r11 r0       ; Set cs low"
    );
}

void spi_5_deselect()
{
    asm(
        "load32 0x7000016 r11 ; r11 = SPI5 cs register"
        "load 1 r12           ; r12 = 1"
        "write 0 r11 r12      ; Set cs high"
    );
}

int spi_5_transfer(int data)
{
    int retval = 0;
    asm(
        "load32 0x7000015 r11 ; r11 = SPI5 data register"
        "write 0 r11 r4       ; Write data to SPI5"
        "read 0 r11 r11       ; Read received data from SPI5"
        "write -1 r14 r11     ; Write received data to stack for return"
    );
    return retval;
}

int spi_transfer(int spi_id, int data)
{
    int retval = 0;
    switch (spi_id) {
        case 0:
            retval = spi_0_transfer(data);
            break;
        case 1:
            retval = spi_1_transfer(data);
            break;
        case 2:
            retval = spi_2_transfer(data);
            break;
        case 3:
            retval = spi_3_transfer(data);
            break;
        case 4:
            retval = spi_4_transfer(data);
            break;
        case 5:
            retval = spi_5_transfer(data);
            break;
        default:
            // Invalid SPI ID
            break;
    }
    return retval;
}

void spi_select(int spi_id)
{
    switch (spi_id) {
        case 0:
            spi_0_select();
            break;
        case 1:
            spi_1_select();
            break;
        case 2:
            spi_2_select();
            break;
        case 3:
            spi_3_select();
            break;
        case 4:
            spi_4_select();
            break;
        case 5:
            spi_5_select();
            break;
        default:
            // Invalid SPI ID
            break;
    }
}

void spi_deselect(int spi_id)
{
    switch (spi_id) {
        case 0:
            spi_0_deselect();
            break;
        case 1:
            spi_1_deselect();
            break;
        case 2:
            spi_2_deselect();
            break;
        case 3:
            spi_3_deselect();
            break;
        case 4:
            spi_4_deselect();
            break;
        case 5:
            spi_5_deselect();
            break;
        default:
            // Invalid SPI ID
            break;
    }
}
