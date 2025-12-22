#define COMMON_STDLIB
#define COMMON_STRING
#define COMMON_FIXEDMATH
#include "libs/common/common.h"

#define KERNEL_GPU_FB
#define KERNEL_GPU_DATA_ASCII
#define KERNEL_UART
#define KERNEL_TIMER
#include "libs/kernel/kernel.h"


#define mapWidth      24
#define mapHeight     24
#define screenWidth   320
#define screenHeight  240

#define COLOR_RED         0b11100000
#define COLOR_DARK_RED    0b01100000
#define COLOR_GREEN       0b00011100
#define COLOR_DARK_GREEN  0b00001100
#define COLOR_BLUE        0b00000011
#define COLOR_DARK_BLUE   0b00000010
#define COLOR_WHITE       0b11111111
#define COLOR_GREY        0b10110110
#define COLOR_YELLOW      0b11111100
#define COLOR_DARK_YELLOW 0b10010000


int worldMap[mapWidth][mapHeight]=
{
  {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
  {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
  {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
  {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
  {1,0,0,0,0,0,2,2,2,2,2,0,0,0,0,3,0,3,0,3,0,0,0,1},
  {1,0,0,0,0,0,2,0,0,0,2,0,0,0,0,0,0,0,0,0,0,0,0,1},
  {1,0,0,0,0,0,2,0,0,0,2,0,0,0,0,3,0,0,0,3,0,0,0,1},
  {1,0,0,0,0,0,2,0,0,0,2,0,0,0,0,0,0,0,0,0,0,0,0,1},
  {1,0,0,0,0,0,2,2,0,2,2,0,0,0,0,3,0,3,0,3,0,0,0,1},
  {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
  {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
  {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
  {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
  {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
  {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
  {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
  {1,4,4,4,4,4,4,4,4,0,0,0,0,0,0,0,5,0,0,0,0,0,0,1},
  {1,4,0,4,0,0,0,0,4,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
  {1,4,0,0,0,0,5,0,4,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
  {1,4,0,4,0,0,0,0,4,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
  {1,4,0,4,4,4,4,4,4,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
  {1,4,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
  {1,4,4,4,4,4,4,4,4,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
  {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1}
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

void draw_vertical_line(int x, int y_start, int y_end, int color)
{
  int y;

  if (y_start < 0) y_start = 0;
  if (y_end >= screenHeight) y_end = screenHeight - 1;

  if (y_start > y_end)
  {
    for (y = 0; y < screenHeight; y++) gpu_set_pixel(x, y, 0);
    return;
  }

  for (y = 0; y < y_start; y++) gpu_set_pixel(x, y, 0);
  for (y = y_start; y <= y_end; y++) gpu_set_pixel(x, y, color);
  for (y = y_end + 1; y < screenHeight; y++) gpu_set_pixel(x, y, 0);
}

int main()
{
  init();

  // X and Y start position in the map
  fixed_t posX = int2fixed(15);
  fixed_t posY = int2fixed(11) + FIXED_HALF;

  // Initial direction vector
  fixed_t dirX = -FIXED_ONE;
  fixed_t dirY = 0;

  // 2D raycaster version of camera plane
  fixed_t planeX = 0;
  fixed_t planeY = 43690; // Two thirds of FIXED_ONE

  // Game loop
  while (1)
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
      fixed_t deltaDistX = (rayDirX == 0) ? 1 << 30 : fixed_abs(__divfp(int2fixed(FIXED_ONE), rayDirX));
      fixed_t deltaDistY = (rayDirY == 0) ? 1 << 30 : fixed_abs(__divfp(int2fixed(FIXED_ONE), rayDirY));
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
