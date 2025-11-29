# GPU (FSX2)

The GPU, called the FSX2 (Frame Synthesizer 2), generates a video signal based on the contents of VRAM. Two types of rendering are supported (simultaneously): tile based rendering and bitmap rendering. The resolutions internally are 320x200 for the tile based rendering, and 320x240 for the bitmap rendering. The tile based rendering consists of two layers, a background layer and a window layer. The output video signal is sent over a 480p TMDS encoded HDMI connection.

The GPU consists of three main parts:

- Timing generator
- Render engines
- Output encoder

## Timing generator

The timing generator generates a 480p video signal with horizontal and vertical counters and sync signals that then can be used by the render engines to draw RGB values for the video output. The timing of the HDMI video signal is as follows:

``` text
H_RES   = 640    // horizontal resolution (pixels)
V_RES   = 480    // vertical resolution (lines)
H_FP    = 16     // horizontal front porch
H_SYNC  = 96     // horizontal sync
H_BP    = 48     // horizontal back porch
V_FP    = 10     // vertical front porch
V_SYNC  = 2      // vertical sync
V_BP    = 33     // vertical back porch
H_POL   = 0      // horizontal sync polarity (0:neg, 1:pos)
V_POL   = 0      // vertical sync polarity
```

## Render engines

The GPU uses render engines to draw different graphic planes, which are then combined in some way and added to the video signal.

### BGWrenderer

The main render engine is the BGWrenderer, which stands for Background and Window plane renderer. It uses a tile based rendering system inspired by the NES PPU in order to prevent having to use a frame buffer and therefore save video RAM. This also makes it easy to draw characters by loading an ASCII tile set into the pattern table, and then just writing the ascii values of the characters you want into the tile table. However, this also makes it impossible to draw individual pixels (you want to use the bitmap renderer for that). The BGWrenderer works as follows:

#### Tile based rendering process

For each tile, the GPU has to read the following tables in order to know which color to draw (in the following order):

- BG Tile table
- Pattern table
- BG Color table
- Palette table
- Window Tile table
- Pattern table
- Window Color table
- Palette table

The Pattern table allows for 256 different tiles on a single screen.

The Palette table allows for 32 different palettes per screen with four colors per palette.

Each address in the tile tables and the color tables is mapped to one tile on screen.

Two layers of tiles are rendered, the background layer which can be scrolled horizontally, and the window layer which draws on top of the background layer and does not scroll.

#### Background layer

The background layer consists of 512x200 pixels. They are indexed by tiles of 8x8 pixels making 64x25 tiles. The background is horizontally scrollable by using the tile offset parameter and fine offset parameter. The tile offset parameter specifies how many tiles the background has to be scrolled to the left. The fine offset parameter specifies how many pixels (ranging from 0 to 7) the background has to be scrolled to the left. The background wraps around horizontally. This means no vertical scrolling (in hardware).

#### Window layer

The window layer consists of 320x200 pixels. They are indexed by tiles of 8x8 pixels making 40x25 tiles. The window is not scrollable and is rendered above the background. When a pixel is fully black, it will not be rendered which makes the background layer visible. There is no support for transparency. The window is especially useful for static UI elements in games like text, score and a life bar, or for text you want to write on top of the bitmap renderer output.

### PixelPlane Engine (bitmap renderer)

The PixelPlane Engine is a very simple rendering engine to allow changing individual pixels, which is also known as bitmap graphics, to overcome the limitation of the BGWrenderer. For every 320x240 pixels there is an address in the pixel table (bitmap) where the 8 bits indicate the color of that pixel. All pixels are sequentially stored in this pixel table, meaning that the second pixel of the second line will be stored in address 321. The rendering engine uses a simple counter that increments when the video signal in in the active area, and resets to 0 at vsync. This counter is used as the address of the pixel table and the resulting data is the RGB value of that pixel.

Because the PixelPlane Engine is very simplistic, the CPU has to do more work for generating graphics. For example, for drawing a line, every pixel has to be calculated and set by the CPU.

!!! info
    Note that bitmap rendering needs relatively many BRAM resources in the FPGA if stored in block RAM. On the custom PCB (Cyclone IV EP4CE40), the framebuffer is stored in external SRAM to free up block RAM. On the Cyclone 10 10CL120 and Artix 7 XC7A75T platforms, the framebuffer is stored in block RAM as these FPGAs have sufficient resources. If you want to run the FPGC on a lower-end FPGA, it should be quite easy to remove this renderer from the design (or reduce the color depth to monochrome), or add external SRAM with arbitration logic.

## Output encoder (HDMI)

To output the video signal, which is in parallel RGBHV + blank format, it needs to be encoded. For VGA this would be very easy, as you only need a DAC to make an analog signal from you R, G and B signals. HDMI, which is smaller, digital, more common nowadays and can be converted externally to VGA, needs some extra work before you can output it. Alternatively, you could use an external digital RGBHV to HDMI/DVI encoder ic. While this is actually a quite viable solution for FPGA's without the TMDS IO standard (like Altera FPGAs), this does cost quite some IO pins depending on the solution.

### Converting from R3G3B2 to R8B8G8

To output a signal over HDMI, the colors need to be (at least) 8 bits per color. This is achieved by repeating the 3 or 2 bits color signal to fill the 8 bits required. This results in an even transition between fully off and fully on. Appending zeroes or ones to the 3 or 2 bits signal does not work well, since that will cause either full white or full black to be impossible.

### TMDS

To output a HDMI signal, the timing signals and R8G8B8 color output have to be TMDS encoded. TMDS minimizes the number of transitions between 1 and 0, resulting in higher signal integrity. Since I do not feel the need to exactly understand the algorithm of TMDS encoding, I used existing Verilog code from [this blog post on msjeemjdo.com](https://mjseemjdo.com/2021/04/02/tutorial-6-hdmi-display-output/)

### Outputting TMDS

After generating the TMDS registers for the clock, R, G and B (+sync) signals, it is required to serialize them and create differential signals. The method for doing this varies depending on the FPGA platform:

#### Xilinx Artix 7 (PZ-A75T StarLite)

Xilinx FPGAs like the Artix 7 have native TMDS support, containing serializers, differential output buffers and hardware outputs for TMDS signals at 3.3V. This makes HDMI output relatively straightforward.

#### Intel/Altera Cyclone IV and Cyclone 10 (Custom PCB and QMTECH module)

The Cyclone IV (used on the custom PCB) and Cyclone 10 LP (used on the QMTECH module) do not support the TMDS IO standard natively. Adding an HDMI encoder IC is expensive, more difficult to solder, increases PCB complexity and costs more I/O pins. At the fixed output resolution of the FPGC (480p), the clock speeds are low enough to use regular output pins with some serialization logic in the FPGA, in combination with AC coupling capacitors (100nF). This works perfect for the two different monitors I tested on.
