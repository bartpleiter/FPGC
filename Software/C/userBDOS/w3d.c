// w3d.c — Wolfenstein 3D-style raycaster for FPGC BDOS
// Port of bare-metal raycast.c to userBDOS with real keyboard input.
// Replaces the four large rotation LUTs with on-the-fly sin/cos computation.

#define COMMON_STDLIB
#define COMMON_FIXEDMATH
#include "libs/common/common.h"

#define USER_SYSCALL
#include "libs/user/user.h"

// ---- Constants ----

#define MAP_W         18
#define MAP_H         19
#define SCREEN_W      320
#define SCREEN_H      240

// Camera FOV scale (about 0.66 in 16.16 fixed-point, determines horizontal FOV)
#define FOV_SCALE     43254

// Movement tuning
#define MOVE_SPEED    2048    // about 0.03 per frame in fixed-point
#define ROT_SPEED     1       // degrees per frame

// Wall colors (R3G3B2)
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

// ---- Map ----

int worldMap[MAP_W][MAP_H] =
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

// ---- Player state ----

fixed_t posX;
fixed_t posY;
fixed_t dirX;
fixed_t dirY;
fixed_t planeX;
fixed_t planeY;
int rotationAngle;

// ---- Functions ----

// Recompute direction and camera plane vectors from the current rotation angle.
// dirX   = -cos(angle)
// dirY   =  sin(angle)
// planeX =  sin(angle) * FOV_SCALE
// planeY =  cos(angle) * FOV_SCALE
void update_camera()
{
  dirX = -fixed_cos(rotationAngle);
  dirY = fixed_sin(rotationAngle);
  planeX = __multfp(fixed_sin(rotationAngle), FOV_SCALE);
  planeY = __multfp(fixed_cos(rotationAngle), FOV_SCALE);
}

// Clear the entire pixel framebuffer to black.
void clear_framebuffer()
{
  unsigned int *fb;
  int i;
  fb = (unsigned int *)0x7B00000;
  for (i = 0; i < 76800; i++)
  {
    fb[i] = 0;
  }
}

// Assembly-optimized vertical line drawer.
// Draws a full column: ceiling (dark blue-grey), wall (color), floor (brown).
// Writes directly to the VRAMPX framebuffer at 0x7B00000.
// Parameters: x = column, y_start/y_end = wall extent, color = R3G3B2 wall color.
void draw_vertical_line(int x, int y_start, int y_end, int color)
{
  asm(
  "push r1"
  "push r2"
  "push r9"

  "load32 0x7B00000 r9  ; r9 = framebuffer base"
  "add r9 r4 r4         ; r4 = fb base + x (first pixel in column)"

  "multu r5 320 r9      ; r9 = y_start * stride"
  "add r9 r4 r5         ; r5 = fb addr of wall start"

  "multu r6 320 r9      ; r9 = y_end * stride"
  "add r9 r4 r6         ; r6 = fb addr of wall end"

  "load 239 r2"
  "multu r2 320 r9"
  "add r9 r4 r2         ; r2 = fb addr of last scanline"

  "; -- ceiling --"
  "load 0b00011011 r1   ; ceiling color"
  "W3D_ceilLoop:"
  "  write 0 r4 r1"
  "  add r4 320 r4"
  "  blt r4 r5 W3D_ceilLoop"

  "; -- wall --"
  "W3D_wallLoop:"
  "  write 0 r4 r7      ; r7 = color parameter"
  "  add r4 320 r4"
  "  blt r4 r6 W3D_wallLoop"

  "; -- floor --"
  "load 0b11011010 r1   ; floor color"
  "W3D_floorLoop:"
  "  write 0 r4 r1"
  "  add r4 320 r4"
  "  blt r4 r2 W3D_floorLoop"

  "pop r9"
  "pop r2"
  "pop r1"
  );
}

// Render one frame using DDA raycasting.
void render_frame()
{
  int x;

  for (x = 0; x < SCREEN_W; x++)
  {
    // Camera-space X coordinate: maps column to [-1.0, +1.0]
    fixed_t cameraX;
    fixed_t rayDirX;
    fixed_t rayDirY;
    int mapX;
    int mapY;
    fixed_t sideDistX;
    fixed_t sideDistY;
    fixed_t deltaDistX;
    fixed_t deltaDistY;
    fixed_t perpWallDist;
    int stepX;
    int stepY;
    int hit;
    int side;
    int lineHeight;
    int drawStart;
    int drawEnd;
    int color;

    cameraX = __divfp(int2fixed(2 * x), int2fixed(SCREEN_W)) - FIXED_ONE;
    rayDirX = dirX + __multfp(planeX, cameraX);
    rayDirY = dirY + __multfp(planeY, cameraX);

    mapX = fixed2int(posX);
    mapY = fixed2int(posY);

    // Delta distances
    if (rayDirX == 0)
      deltaDistX = 1 << 30;
    else
      deltaDistX = fixed_abs(__divfp(FIXED_ONE, rayDirX));

    if (rayDirY == 0)
      deltaDistY = 1 << 30;
    else
      deltaDistY = fixed_abs(__divfp(FIXED_ONE, rayDirY));

    // Step direction and initial side distances
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

    // DDA
    hit = 0;
    while (hit == 0)
    {
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
      if (worldMap[mapX][mapY] > 0) hit = 1;
    }

    // Perpendicular wall distance (avoids fisheye)
    if (side == 0)
      perpWallDist = sideDistX - deltaDistX;
    else
      perpWallDist = sideDistY - deltaDistY;

    // Wall strip height
    lineHeight = fixed2int(__divfp(int2fixed(SCREEN_H), perpWallDist));

    drawStart = -lineHeight / 2 + SCREEN_H / 2;
    if (drawStart < 0) drawStart = 0;
    drawEnd = lineHeight / 2 + SCREEN_H / 2;
    if (drawEnd >= SCREEN_H) drawEnd = SCREEN_H - 1;

    // Wall color by tile type, darker on Y-side
    switch (worldMap[mapX][mapY])
    {
      case 1:  color = (side == 1) ? COLOR_DARK_RED    : COLOR_RED;    break;
      case 2:  color = (side == 1) ? COLOR_DARK_GREEN  : COLOR_GREEN;  break;
      case 3:  color = (side == 1) ? COLOR_DARK_BLUE   : COLOR_BLUE;   break;
      case 4:  color = (side == 1) ? COLOR_GREY        : COLOR_WHITE;  break;
      default: color = (side == 1) ? COLOR_DARK_YELLOW : COLOR_YELLOW; break;
    }

    draw_vertical_line(x, drawStart, drawEnd, color);
  }
}

// Process keyboard input and update player position/angle.
void process_input()
{
  int keys;
  fixed_t mx;
  fixed_t my;

  keys = sys_get_key_state();

  // Rotation: A/Left = rotate left, D/Right = rotate right
  if (keys & (KEYSTATE_A | KEYSTATE_LEFT))
  {
    rotationAngle -= ROT_SPEED;
    if (rotationAngle < 0) rotationAngle += 360;
    update_camera();
  }
  if (keys & (KEYSTATE_D | KEYSTATE_RIGHT))
  {
    rotationAngle += ROT_SPEED;
    if (rotationAngle >= 360) rotationAngle -= 360;
    update_camera();
  }

  // Forward: W/Up
  if (keys & (KEYSTATE_W | KEYSTATE_UP))
  {
    mx = __multfp(dirX, MOVE_SPEED);
    my = __multfp(dirY, MOVE_SPEED);
    if (worldMap[fixed2int(posX + mx)][fixed2int(posY)] == 0)
      posX += mx;
    if (worldMap[fixed2int(posX)][fixed2int(posY + my)] == 0)
      posY += my;
  }

  // Backward: S/Down
  if (keys & (KEYSTATE_S | KEYSTATE_DOWN))
  {
    mx = __multfp(dirX, MOVE_SPEED);
    my = __multfp(dirY, MOVE_SPEED);
    if (worldMap[fixed2int(posX - mx)][fixed2int(posY)] == 0)
      posX -= mx;
    if (worldMap[fixed2int(posX)][fixed2int(posY - my)] == 0)
      posY -= my;
  }

  // Strafe left: Q (move perpendicular: -dirY, +dirX)
  if (keys & KEYSTATE_Q)
  {
    mx = __multfp(-dirY, MOVE_SPEED);
    my = __multfp(dirX, MOVE_SPEED);
    if (worldMap[fixed2int(posX + mx)][fixed2int(posY)] == 0)
      posX += mx;
    if (worldMap[fixed2int(posX)][fixed2int(posY + my)] == 0)
      posY += my;
  }

  // Strafe right: E (move perpendicular: +dirY, -dirX)
  if (keys & KEYSTATE_E)
  {
    mx = __multfp(dirY, MOVE_SPEED);
    my = __multfp(-dirX, MOVE_SPEED);
    if (worldMap[fixed2int(posX + mx)][fixed2int(posY)] == 0)
      posX += mx;
    if (worldMap[fixed2int(posX)][fixed2int(posY + my)] == 0)
      posY += my;
  }
}

int main()
{
  int keys;

  // Initialize player at map position (8.5, 9.5) facing angle 0 (west)
  posX = int2fixed(8) + FIXED_HALF;
  posY = int2fixed(9) + FIXED_HALF;
  rotationAngle = 0;
  update_camera();

  // Clear tile plane (terminal text) and pixel framebuffer
  sys_term_clear();
  clear_framebuffer();

  // Game loop
  while (1)
  {
    render_frame();

    process_input();

    // Exit on Escape
    keys = sys_get_key_state();
    if (keys & KEYSTATE_ESCAPE)
    {
      break;
    }
  }

  // Restore screen: clear pixel plane and terminal
  clear_framebuffer();
  sys_term_clear();
  return 0;
}
