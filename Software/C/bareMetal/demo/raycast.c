#define COMMON_STDLIB
#define COMMON_STRING
#define COMMON_FIXEDMATH
#include "libs/common/common.h"

#define KERNEL_GPU_FB
#define KERNEL_GPU_DATA_ASCII
#define KERNEL_UART
#define KERNEL_TIMER
#include "libs/kernel/kernel.h"


#define mapWidth      18
#define mapHeight     19
#define screenWidth   320
#define screenHeight  240

#define COLOR_RED         0xE0
#define COLOR_DARK_RED    0x60
#define COLOR_GREEN       0x1C
#define COLOR_DARK_GREEN  0x08
#define COLOR_BLUE        0x03
#define COLOR_DARK_BLUE   0x02
#define COLOR_WHITE       0xFF
#define COLOR_GREY        0xB6
#define COLOR_YELLOW      0xFC
#define COLOR_DARK_YELLOW 0x90


int worldMap[mapWidth][mapHeight]=
{
  {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
  {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
  {1,0,0,2,2,2,2,2,0,0,0,3,0,3,0,3,0,0,1},
  {1,0,0,2,0,0,0,2,0,0,0,0,0,0,0,0,0,0,1},
  {1,0,0,2,0,0,0,2,0,0,0,3,0,0,0,3,0,0,1},
  {1,0,0,2,0,0,0,2,0,0,0,0,0,0,0,0,0,0,1},
  {1,0,0,2,2,0,2,2,0,0,0,0,0,3,0,3,0,0,1},
  {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
  {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
  {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
  {1,4,4,4,0,4,4,4,4,0,0,0,0,0,0,0,2,0,1},
  {1,4,0,4,0,0,0,0,4,0,0,0,0,0,0,2,0,0,1},
  {1,4,0,0,0,0,5,0,0,0,0,0,0,0,0,0,0,0,1},
  {1,4,0,4,0,0,0,0,4,0,0,0,5,0,2,0,0,0,1},
  {1,4,0,4,4,4,4,4,4,0,0,5,5,0,2,0,0,0,1},
  {1,4,0,0,0,0,0,0,4,0,0,0,0,0,2,0,0,0,1},
  {1,4,4,4,4,4,4,4,4,0,0,0,0,0,2,0,0,0,1},
  {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1}
};

// Lookup tables for rotation
fixed_t LUTdirX[360] = {
-65526, -65496, -65446, -65376, -65287, -65177, -65048, -64898, -64729, -64540, -64332, -64104, 
-63856, -63589, -63303, -62997, -62672, -62328, -61966, -61584, -61183, -60764, -60326, -59870, 
-59396, -58903, -58393, -57865, -57319, -56756, -56175, -55578, -54963, -54332, -53684, -53020, 
-52339, -51643, -50931, -50203, -49461, -48703, -47930, -47143, -46341, -45525, -44695, -43852, 
-42995, -42126, -41243, -40348, -39441, -38521, -37590, -36647, -35693, -34729, -33754, -32768, 
-31772, -30767, -29753, -28729, -27697, -26656, -25607, -24550, -23486, -22415, -21336, -20252, 
-19161, -18064, -16962, -15855, -14742, -13626, -12505, -11380, -10252, -9121, -7987, -6850, 
-5712, -4572, -3430, -2287, -1144, 0, 1144, 2287, 3430, 4572, 5712, 6850, 
7987, 9121, 10252, 11380, 12505, 13626, 14742, 15855, 16962, 18064, 19161, 20252, 
21336, 22415, 23486, 24550, 25607, 26656, 27697, 28729, 29753, 30767, 31772, 32768, 
33754, 34729, 35693, 36647, 37590, 38521, 39441, 40348, 41243, 42126, 42995, 43852, 
44695, 45525, 46341, 47143, 47930, 48703, 49461, 50203, 50931, 51643, 52339, 53020, 
53684, 54332, 54963, 55578, 56175, 56756, 57319, 57865, 58393, 58903, 59396, 59870, 
60326, 60764, 61183, 61584, 61966, 62328, 62672, 62997, 63303, 63589, 63856, 64104, 
64332, 64540, 64729, 64898, 65048, 65177, 65287, 65376, 65446, 65496, 65526, 65536, 
65526, 65496, 65446, 65376, 65287, 65177, 65048, 64898, 64729, 64540, 64332, 64104, 
63856, 63589, 63303, 62997, 62672, 62328, 61966, 61584, 61183, 60764, 60326, 59870, 
59396, 58903, 58393, 57865, 57319, 56756, 56175, 55578, 54963, 54332, 53684, 53020, 
52339, 51643, 50931, 50203, 49461, 48703, 47930, 47143, 46341, 45525, 44695, 43852, 
42995, 42126, 41243, 40348, 39441, 38521, 37590, 36647, 35693, 34729, 33754, 32768, 
31772, 30767, 29753, 28729, 27697, 26656, 25607, 24550, 23486, 22415, 21336, 20252, 
19161, 18064, 16962, 15855, 14742, 13626, 12505, 11380, 10252, 9121, 7987, 6850, 
5712, 4572, 3430, 2287, 1144, 0, -1144, -2287, -3430, -4572, -5712, -6850, 
-7987, -9121, -10252, -11380, -12505, -13626, -14742, -15855, -16962, -18064, -19161, -20252, 
-21336, -22415, -23486, -24550, -25607, -26656, -27697, -28729, -29753, -30767, -31772, -32768, 
-33754, -34729, -35693, -36647, -37590, -38521, -39441, -40348, -41243, -42126, -42995, -43852, 
-44695, -45525, -46341, -47143, -47930, -48703, -49461, -50203, -50931, -51643, -52339, -53020, 
-53684, -54332, -54963, -55578, -56175, -56756, -57319, -57865, -58393, -58903, -59396, -59870, 
-60326, -60764, -61183, -61584, -61966, -62328, -62672, -62997, -63303, -63589, -63856, -64104, 
-64332, -64540, -64729, -64898, -65048, -65177, -65287, -65376, -65446, -65496, -65526, -65536
};

fixed_t LUTdirY[360] = {
1144, 2287, 3430, 4572, 5712, 6850, 7987, 9121, 10252, 11380, 12505, 13626, 
14742, 15855, 16962, 18064, 19161, 20252, 21336, 22415, 23486, 24550, 25607, 26656, 
27697, 28729, 29753, 30767, 31772, 32768, 33754, 34729, 35693, 36647, 37590, 38521, 
39441, 40348, 41243, 42126, 42995, 43852, 44695, 45525, 46341, 47143, 47930, 48703, 
49461, 50203, 50931, 51643, 52339, 53020, 53684, 54332, 54963, 55578, 56175, 56756, 
57319, 57865, 58393, 58903, 59396, 59870, 60326, 60764, 61183, 61584, 61966, 62328, 
62672, 62997, 63303, 63589, 63856, 64104, 64332, 64540, 64729, 64898, 65048, 65177, 
65287, 65376, 65446, 65496, 65526, 65536, 65526, 65496, 65446, 65376, 65287, 65177, 
65048, 64898, 64729, 64540, 64332, 64104, 63856, 63589, 63303, 62997, 62672, 62328, 
61966, 61584, 61183, 60764, 60326, 59870, 59396, 58903, 58393, 57865, 57319, 56756, 
56175, 55578, 54963, 54332, 53684, 53020, 52339, 51643, 50931, 50203, 49461, 48703, 
47930, 47143, 46341, 45525, 44695, 43852, 42995, 42126, 41243, 40348, 39441, 38521, 
37590, 36647, 35693, 34729, 33754, 32768, 31772, 30767, 29753, 28729, 27697, 26656, 
25607, 24550, 23486, 22415, 21336, 20252, 19161, 18064, 16962, 15855, 14742, 13626, 
12505, 11380, 10252, 9121, 7987, 6850, 5712, 4572, 3430, 2287, 1144, 0, 
-1144, -2287, -3430, -4572, -5712, -6850, -7987, -9121, -10252, -11380, -12505, -13626, 
-14742, -15855, -16962, -18064, -19161, -20252, -21336, -22415, -23486, -24550, -25607, -26656, 
-27697, -28729, -29753, -30767, -31772, -32768, -33754, -34729, -35693, -36647, -37590, -38521, 
-39441, -40348, -41243, -42126, -42995, -43852, -44695, -45525, -46341, -47143, -47930, -48703, 
-49461, -50203, -50931, -51643, -52339, -53020, -53684, -54332, -54963, -55578, -56175, -56756, 
-57319, -57865, -58393, -58903, -59396, -59870, -60326, -60764, -61183, -61584, -61966, -62328, 
-62672, -62997, -63303, -63589, -63856, -64104, -64332, -64540, -64729, -64898, -65048, -65177, 
-65287, -65376, -65446, -65496, -65526, -65536, -65526, -65496, -65446, -65376, -65287, -65177, 
-65048, -64898, -64729, -64540, -64332, -64104, -63856, -63589, -63303, -62997, -62672, -62328, 
-61966, -61584, -61183, -60764, -60326, -59870, -59396, -58903, -58393, -57865, -57319, -56756, 
-56175, -55578, -54963, -54332, -53684, -53020, -52339, -51643, -50931, -50203, -49461, -48703, 
-47930, -47143, -46341, -45525, -44695, -43852, -42995, -42126, -41243, -40348, -39441, -38521, 
-37590, -36647, -35693, -34729, -33754, -32768, -31772, -30767, -29753, -28729, -27697, -26656, 
-25607, -24550, -23486, -22415, -21336, -20252, -19161, -18064, -16962, -15855, -14742, -13626, 
-12505, -11380, -10252, -9121, -7987, -6850, -5712, -4572, -3430, -2287, -1144, 0
};

fixed_t LUTplaneX[360] = {
755, 1510, 2264, 3017, 3770, 4521, 5271, 6020, 6766, 7511, 8253, 8993, 
9730, 10464, 11195, 11922, 12646, 13366, 14082, 14794, 15501, 16203, 16901, 17593, 
18280, 18961, 19637, 20306, 20970, 21627, 22277, 22921, 23558, 24187, 24809, 25424, 
26031, 26630, 27220, 27803, 28377, 28942, 29499, 30047, 30585, 31114, 31634, 32144, 
32644, 33134, 33614, 34084, 34544, 34993, 35431, 35859, 36276, 36681, 37076, 37459, 
37831, 38191, 38539, 38876, 39201, 39514, 39815, 40104, 40381, 40645, 40897, 41137, 
41364, 41578, 41780, 41969, 42145, 42309, 42459, 42597, 42721, 42833, 42931, 43017, 
43089, 43148, 43194, 43227, 43247, 43254, 43247, 43227, 43194, 43148, 43089, 43017, 
42931, 42833, 42721, 42597, 42459, 42309, 42145, 41969, 41780, 41578, 41364, 41137, 
40897, 40645, 40381, 40104, 39815, 39514, 39201, 38876, 38539, 38191, 37831, 37459, 
37076, 36681, 36276, 35859, 35431, 34993, 34544, 34084, 33614, 33134, 32644, 32144, 
31634, 31114, 30585, 30047, 29499, 28942, 28377, 27803, 27220, 26630, 26031, 25424, 
24809, 24187, 23558, 22921, 22277, 21627, 20970, 20306, 19637, 18961, 18280, 17593, 
16901, 16203, 15501, 14794, 14082, 13366, 12646, 11922, 11195, 10464, 9730, 8993, 
8253, 7511, 6766, 6020, 5271, 4521, 3770, 3017, 2264, 1510, 755, 0, 
-755, -1510, -2264, -3017, -3770, -4521, -5271, -6020, -6766, -7511, -8253, -8993, 
-9730, -10464, -11195, -11922, -12646, -13366, -14082, -14794, -15501, -16203, -16901, -17593, 
-18280, -18961, -19637, -20306, -20970, -21627, -22277, -22921, -23558, -24187, -24809, -25424, 
-26031, -26630, -27220, -27803, -28377, -28942, -29499, -30047, -30585, -31114, -31634, -32144, 
-32644, -33134, -33614, -34084, -34544, -34993, -35431, -35859, -36276, -36681, -37076, -37459, 
-37831, -38191, -38539, -38876, -39201, -39514, -39815, -40104, -40381, -40645, -40897, -41137, 
-41364, -41578, -41780, -41969, -42145, -42309, -42459, -42597, -42721, -42833, -42931, -43017, 
-43089, -43148, -43194, -43227, -43247, -43254, -43247, -43227, -43194, -43148, -43089, -43017, 
-42931, -42833, -42721, -42597, -42459, -42309, -42145, -41969, -41780, -41578, -41364, -41137, 
-40897, -40645, -40381, -40104, -39815, -39514, -39201, -38876, -38539, -38191, -37831, -37459, 
-37076, -36681, -36276, -35859, -35431, -34993, -34544, -34084, -33614, -33134, -32644, -32144, 
-31634, -31114, -30585, -30047, -29499, -28942, -28377, -27803, -27220, -26630, -26031, -25424, 
-24809, -24187, -23558, -22921, -22277, -21627, -20970, -20306, -19637, -18961, -18280, -17593, 
-16901, -16203, -15501, -14794, -14082, -13366, -12646, -11922, -11195, -10464, -9730, -8993, 
-8253, -7511, -6766, -6020, -5271, -4521, -3770, -3017, -2264, -1510, -755, 0
};

fixed_t LUTplaneY[360] = {
43247, 43227, 43194, 43148, 43089, 43017, 42931, 42833, 42721, 42597, 42459, 42309, 
42145, 41969, 41780, 41578, 41364, 41137, 40897, 40645, 40381, 40104, 39815, 39514, 
39201, 38876, 38539, 38191, 37831, 37459, 37076, 36681, 36276, 35859, 35431, 34993, 
34544, 34084, 33614, 33134, 32644, 32144, 31634, 31114, 30585, 30047, 29499, 28942, 
28377, 27803, 27220, 26630, 26031, 25424, 24809, 24187, 23558, 22921, 22277, 21627, 
20970, 20306, 19637, 18961, 18280, 17593, 16901, 16203, 15501, 14794, 14082, 13366, 
12646, 11922, 11195, 10464, 9730, 8993, 8253, 7511, 6766, 6020, 5271, 4521, 
3770, 3017, 2264, 1510, 755, 0, -755, -1510, -2264, -3017, -3770, -4521, 
-5271, -6020, -6766, -7511, -8253, -8993, -9730, -10464, -11195, -11922, -12646, -13366, 
-14082, -14794, -15501, -16203, -16901, -17593, -18280, -18961, -19637, -20306, -20970, -21627, 
-22277, -22921, -23558, -24187, -24809, -25424, -26031, -26630, -27220, -27803, -28377, -28942, 
-29499, -30047, -30585, -31114, -31634, -32144, -32644, -33134, -33614, -34084, -34544, -34993, 
-35431, -35859, -36276, -36681, -37076, -37459, -37831, -38191, -38539, -38876, -39201, -39514, 
-39815, -40104, -40381, -40645, -40897, -41137, -41364, -41578, -41780, -41969, -42145, -42309, 
-42459, -42597, -42721, -42833, -42931, -43017, -43089, -43148, -43194, -43227, -43247, -43254, 
-43247, -43227, -43194, -43148, -43089, -43017, -42931, -42833, -42721, -42597, -42459, -42309, 
-42145, -41969, -41780, -41578, -41364, -41137, -40897, -40645, -40381, -40104, -39815, -39514, 
-39201, -38876, -38539, -38191, -37831, -37459, -37076, -36681, -36276, -35859, -35431, -34993, 
-34544, -34084, -33614, -33134, -32644, -32144, -31634, -31114, -30585, -30047, -29499, -28942, 
-28377, -27803, -27220, -26630, -26031, -25424, -24809, -24187, -23558, -22921, -22277, -21627, 
-20970, -20306, -19637, -18961, -18280, -17593, -16901, -16203, -15501, -14794, -14082, -13366, 
-12646, -11922, -11195, -10464, -9730, -8993, -8253, -7511, -6766, -6020, -5271, -4521, 
-3770, -3017, -2264, -1510, -755, 0, 755, 1510, 2264, 3017, 3770, 4521, 
5271, 6020, 6766, 7511, 8253, 8993, 9730, 10464, 11195, 11922, 12646, 13366, 
14082, 14794, 15501, 16203, 16901, 17593, 18280, 18961, 19637, 20306, 20970, 21627, 
22277, 22921, 23558, 24187, 24809, 25424, 26031, 26630, 27220, 27803, 28377, 28942, 
29499, 30047, 30585, 31114, 31634, 32144, 32644, 33134, 33614, 34084, 34544, 34993, 
35431, 35859, 36276, 36681, 37076, 37459, 37831, 38191, 38539, 38876, 39201, 39514, 
39815, 40104, 40381, 40645, 40897, 41137, 41364, 41578, 41780, 41969, 42145, 42309, 
42459, 42597, 42721, 42833, 42931, 43017, 43089, 43148, 43194, 43227, 43247, 43254
};

void init()
{
  // Reset GPU VRAM
  gpu_clear_vram();

  // Load default pattern and palette tables
  unsigned int* pattern_table = (unsigned int*)&DATA_ASCII_DEFAULT;
  gpu_load_pattern_table(pattern_table + 3); // +3 to skip function prologue

  unsigned int* palette_table = (unsigned int*)&DATA_PALETTE_DEFAULT;
  gpu_load_palette_table(palette_table + 3); // +3 to skip function prologue
}

// Slow version of draw_vertical_line using gpu_write_pixel_data, which causes a lot of overhead
void draw_vertical_line_slow(int x, int y_start, int y_end, int color)
{
  int y;

  if (y_start < 0) y_start = 0;
  if (y_end >= screenHeight) y_end = screenHeight - 1;

  if (y_start > y_end)
  {
    for (y = 0; y < screenHeight; y++) gpu_write_pixel_data(x, y, 0);
    return;
  }

  for (y = 0; y < y_start; y++) gpu_write_pixel_data(x, y, 0);
  for (y = y_start; y <= y_end; y++) gpu_write_pixel_data(x, y, color);
  for (y = y_end + 1; y < screenHeight; y++) gpu_write_pixel_data(x, y, 0);
}

// Assembly optimized version of draw_vertical_line
void draw_vertical_line(int x, int y_start, int y_end, int color)
{
  asm(
  "; backup registers"
  "push r1"
  "push r2"
  "push r9"

  "load32 0x7B00000 r9  ; r9 = framebuffer addr"
  "add r9 r4 r4         ; r4 = first pixel in line"

  "multu r5 320 r9      ; r9 = start with line offset"
  "add r9 r4 r5         ; r5 = fb addr of start"
  
  "multu r6 320 r9      ; r9 = end with line offset"
  "add r9 r4 r6         ; r6 = fb addr of start"

  "load 239 r2          ; r2 = y endloop"
  "multu r2 320 r9      ; r9 = start line offset"
  "add r9 r4 r2         ; r2 = fb addr of final pixel"

  "; draw until start"
  "load 0b00011011 r1 ; ceiling color"
  "RAYFX_drawVlineLoopCeiling:"
  "  write 0 r4 r1     ; write ceiling pixel"
  "  add r4 320 r4     ; go to next line pixel"

  "  blt r4 r5 RAYFX_drawVlineLoopCeiling ; keep looping until reached start"

  "; draw until end"
  "RAYFX_drawVlineLoopWall:"
  "  write 0 r4 r7     ; write color pixel"
  "  add r4 320 r4     ; go to next line pixel"

  "  blt r4 r6 RAYFX_drawVlineLoopWall ; keep looping until reached end"


  "; draw until final pixel"
  "load 0b11011010 r1 ; floor color"
  "RAYFX_drawVlineLoopFloor:"
  "  write 0 r4 r1     ; write floor pixel"
  "  add r4 320 r4     ; go to next line pixel"

  "  blt r4 r2 RAYFX_drawVlineLoopFloor ; keep looping until reached end of screen"

  "; restore registers"
  "pop r9"
  "pop r2"
  "pop r1"
  );

}


int main()
{
  init();

  // X and Y start position in the map
  fixed_t posX = int2fixed(8) + FIXED_HALF;
  fixed_t posY = int2fixed(9) + FIXED_HALF;

  // Initial direction vector
  fixed_t dirX = -FIXED_ONE;
  fixed_t dirY = 0;

  // 2D raycaster version of camera plane
  fixed_t planeX = 0;
  fixed_t planeY = 43690; // Two thirds of FIXED_ONE

  // Movement variables
  fixed_t moveSpeed = FIXED_ONE >> 5;
  int rotationAngle = 0;
  int rotationSpeed = 1; // degrees per frame
  int moveForward = 1;
  int moveBackward = 0;
  int rotateLeft = 0;
  int rotateRight = 1;

  // Demo variables
  int frameCount = 0;
  int framesToRender = 360;
  int inifiniteLoop = 1;

  // Game loop
  int exit = 0;
  while (exit == 0)
  {
    int x;
    for (x = 0; x < screenWidth; x++)
    {
      // Calculate ray position and direction
      fixed_t cameraX = __divfp(int2fixed(2 * x), int2fixed(screenWidth)) - FIXED_ONE; // x-coordinate in camera space
      fixed_t rayDirX = dirX + __multfp(planeX, cameraX);
      fixed_t rayDirY = dirY + __multfp(planeY, cameraX);

      // Which box of the map we're in
      int mapX = fixed2int(posX);
      int mapY = fixed2int(posY);

      // Length of ray from current position to next x or y-side
      fixed_t sideDistX;
      fixed_t sideDistY;

      // Length of ray from one x or y-side to next x or y-side
      fixed_t deltaDistX;
      fixed_t deltaDistY;
      if (rayDirX == 0)
        deltaDistX = 1 << 30;
      else
        deltaDistX = fixed_abs(__divfp(FIXED_ONE, rayDirX));
      
      if (rayDirY == 0)
        deltaDistY = 1 << 30;
      else
        deltaDistY = fixed_abs(__divfp(FIXED_ONE, rayDirY));
      
      fixed_t perpWallDist;

      // What direction to step in x or y-direction (either +1 or -1)
      int stepX;
      int stepY;

      int hit = 0; // Was there a wall hit?
      int side; // Was a NS or a EW wall hit?

      // Calculate step and initial sideDist
      if (rayDirX < 0)
      {
        stepX = -1;
        sideDistX = __multfp(posX - int2fixed(mapX), deltaDistX);
      }
      else
      {
        stepX = 1;
        sideDistX = __multfp(int2fixed(mapX + 1) - posX, deltaDistX);
      }

      if (rayDirY < 0)
      {
        stepY = -1;
        sideDistY = __multfp(posY - int2fixed(mapY), deltaDistY);
      }
      else
      {
        stepY = 1;
        sideDistY = __multfp(int2fixed(mapY + 1) - posY, deltaDistY);
      }

      // Perform DDA
      while (hit == 0)
      {
        // Jump to next map square, either in x-direction, or in y-direction
        if (sideDistX < sideDistY)
        {
          sideDistX += deltaDistX;
          mapX += stepX;
          side = 0;
        }
        else
        {
          sideDistY += deltaDistY;
          mapY += stepY;
          side = 1;
        }
        // Check if ray has hit a wall
        if (worldMap[mapX][mapY] > 0) hit = 1;
      }

      // Calculate distance projected on camera direction (Euclidean distance will give fisheye effect!)
      if (side == 0)
        perpWallDist = (sideDistX - deltaDistX);
      else
        perpWallDist = (sideDistY - deltaDistY);

      // Calculate height of line to draw on screen
      int lineHeight = fixed2int(__divfp(int2fixed(screenHeight), perpWallDist));

      // Calculate lowest and highest pixel to fill in current stripe
      int drawStart = -lineHeight / 2 + screenHeight / 2;
      if (drawStart < 0) drawStart = 0;
      int drawEnd = lineHeight / 2 + screenHeight / 2;
      if (drawEnd >= screenHeight) drawEnd = screenHeight - 1;

      // Choose wall color with x and y sides different brightness
      int color;
      switch(worldMap[mapX][mapY])
      {
        case 1:  color = (side == 1) ? COLOR_DARK_RED    : COLOR_RED;    break;
        case 2:  color = (side == 1) ? COLOR_DARK_GREEN  : COLOR_GREEN;  break;
        case 3:  color = (side == 1) ? COLOR_DARK_BLUE   : COLOR_BLUE;   break;
        case 4:  color = (side == 1) ? COLOR_GREY        : COLOR_WHITE;  break;
        default: color = (side == 1) ? COLOR_DARK_YELLOW : COLOR_YELLOW; break;
      }

      // Draw the pixels of the stripe as a vertical line
      draw_vertical_line(x, drawStart, drawEnd, color);
    }

    // Movement functions
    if (moveForward)
    {
      if (worldMap[fixed2int(posX + __multfp(dirX, moveSpeed))][fixed2int(posY)] == 0)
        posX += __multfp(dirX, moveSpeed);
      if (worldMap[fixed2int(posX)][fixed2int(posY + __multfp(dirY, moveSpeed))] == 0)
        posY += __multfp(dirY, moveSpeed);
    }
    if (moveBackward)
    {
      if (worldMap[fixed2int(posX - __multfp(dirX, moveSpeed))][fixed2int(posY)] == 0)
        posX -= __multfp(dirX, moveSpeed);
      if (worldMap[fixed2int(posX)][fixed2int(posY - __multfp(dirY, moveSpeed))] == 0)
        posY -= __multfp(dirY, moveSpeed);
    }
    if (rotateRight)
    {
      // Both camera direction and camera plane must be rotated
      rotationAngle += rotationSpeed;
      if (rotationAngle >= 360)
      {
        rotationAngle -= 360;
      }
      dirX = LUTdirX[rotationAngle];
      dirY = LUTdirY[rotationAngle];
      planeX = LUTplaneX[rotationAngle];
      planeY = LUTplaneY[rotationAngle];
    }
    if (rotateLeft)
    {
      // Both camera direction and camera plane must be rotated
      rotationAngle -= rotationSpeed;
      if (rotationAngle < 0)
      {
        rotationAngle += 360;
      }
      dirX = LUTdirX[rotationAngle];
      dirY = LUTdirY[rotationAngle];
      planeX = LUTplaneX[rotationAngle];
      planeY = LUTplaneY[rotationAngle];
    }

    frameCount++;
    if (frameCount >= framesToRender)
    {
      if (!inifiniteLoop)
        exit = 1;
    }
  }
  return 0;
}

void interrupt()
{
  int int_id = get_int_id();
  switch (int_id)
  {
    case INTID_TIMER2: // Needed for delay()
      timer_isr_handler(TIMER_2);
      break;
    default:
      break;
  }
}
