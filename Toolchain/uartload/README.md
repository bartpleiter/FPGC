# uartload

Loads a program onto the [FPGC](https://fpgc.bartpleiter.com) over UART, and runs it. The program is a binary image from [b32asm](../b32asm), assembled for address 0.

```bash
uv tool install Toolchain/uartload    # or use it from the FPGC repository's venv
b32asm hello.asm hello.bin
uartload hello.bin
uartload hello.bin --port /dev/ttyUSB1
```

uartload resets the board, waits for the boot ROM's `boot: ready`, and sends the program with its length and CRC-32. The boot ROM writes it to SDRAM, checks the CRC, and runs it. If the CRC is wrong, uartload tries again, up to 3 times. Then it shows what the board sends, until the program returns to the boot ROM, or Ctrl-C.

The protocol is described on the website's [boot ROM](https://fpgc.bartpleiter.com/boot/) page.

Run the tests with `pytest Toolchain/uartload`.
