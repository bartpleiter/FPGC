// w3d.c — Wolfenstein 3D-style raycaster for FPGC BDOS
// Phase 1-4: userBDOS port, LUT-free rotation, textured walls, assembly renderer.

#define COMMON_STDLIB
#define COMMON_FIXEDMATH
#define COMMON_TIME
#include "libs/common/common.h"

#define USER_SYSCALL
#include "libs/user/user.h"

// ---- Constants ----

#define MAP_W         18
#define MAP_H         19
#define SCREEN_W      320
#define SCREEN_H      240
#define TEX_SIZE      64
#define TEX_PIXELS    4096

// Camera FOV scale (about 0.66 in 16.16 fixed-point)
#define FOV_SCALE     43254

// Movement tuning (base speeds, scaled by delta time)
#define MOVE_SPEED    4096    // about 0.06 per frame at 30 FPS
#define ROT_SPEED     2       // degrees per frame at 30 FPS
#define TARGET_DT_MS  33      // 30 FPS in milliseconds

// Ceiling and floor colors (R3G3B2)
#define CEIL_COLOR    0x1B
#define FLOOR_COLOR   0xDA

// Number of procedural textures
#define NUM_TEXTURES  5

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

// ---- Texture data (heap-allocated) ----

unsigned int *textures;

// ---- Globals for assembly renderer ----
// These are set by C before calling the assembly column renderer.

int g_texBase;
int g_texStep;
int g_texPos;

// ---- Functions ----

void update_camera()
{
  dirX = -fixed_cos(rotationAngle);
  dirY = fixed_sin(rotationAngle);
  planeX = __multfp(fixed_sin(rotationAngle), FOV_SCALE);
  planeY = __multfp(fixed_cos(rotationAngle), FOV_SCALE);
}

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

// Generate procedural test textures in heap memory.
void generate_textures()
{
  int x;
  int y;
  int idx;
  int v;
  int r;
  int g;
  int b;

  textures = (unsigned int *)sys_heap_alloc(NUM_TEXTURES * TEX_PIXELS);

  for (y = 0; y < TEX_SIZE; y++)
  {
    for (x = 0; x < TEX_SIZE; x++)
    {
      idx = y * TEX_SIZE + x;

      // Texture 0: Red brick pattern
      v = 0;
      if ((y & 15) == 0)
        v = 1;
      else if ((x & 31) == 0 && (y & 16))
        v = 1;
      else if (((x + 16) & 31) == 0 && ((y & 16) == 0))
        v = 1;
      if (v)
        textures[0 * TEX_PIXELS + idx] = 0x49;
      else
        textures[0 * TEX_PIXELS + idx] = 0xA0 | (((x ^ y) & 1) ? 0x20 : 0);

      // Texture 1: Green stone
      v = ((x ^ y) * 3) & 31;
      g = 2 + (v >> 3);
      if (g > 7) g = 7;
      textures[1 * TEX_PIXELS + idx] = (g << 2);

      // Texture 2: Blue tile
      v = 0;
      if ((x & 31) == 0 || (y & 31) == 0)
        v = 1;
      if (v)
        textures[2 * TEX_PIXELS + idx] = 0x01;
      else
        textures[2 * TEX_PIXELS + idx] = 0x02 | (((x + y) & 1) ? 0x01 : 0);

      // Texture 3: White/grey checkerboard stone
      v = ((x >> 3) ^ (y >> 3)) & 1;
      r = v ? 7 : 5;
      g = v ? 7 : 5;
      b = v ? 3 : 2;
      textures[3 * TEX_PIXELS + idx] = (r << 5) | (g << 2) | b;

      // Texture 4: Yellow wood grain
      v = (x + (y >> 2)) & 15;
      r = (v < 8) ? 6 : 7;
      g = (v < 8) ? 4 : 6;
      b = (v < 4) ? 1 : 0;
      textures[4 * TEX_PIXELS + idx] = (r << 5) | (g << 2) | b;
    }
  }
}

// Assembly-optimized textured column renderer.
// Draws a full column: ceiling, textured wall, floor.
// Args: x (r4), drawStart (r5), drawEnd (r6), side (r7).
// Reads globals: g_texBase, g_texStep, g_texPos.
void draw_textured_column(int x, int drawStart, int drawEnd, int side)
{
  asm(
  "; save registers"
  "push r1"
  "push r2"
  "push r3"
  "push r8"
  "push r9"
  "push r10"
  "push r11"
  "push r12"

  "; load constants"
  "load 63 r12                  ; texY mask"

  "; load globals"
  "addr2reg Label_g_texBase r11"
  "read 0 r11 r11              ; r11 = texBase pointer"
  "addr2reg Label_g_texStep r10"
  "read 0 r10 r10              ; r10 = texStep"
  "addr2reg Label_g_texPos r9"
  "read 0 r9 r9                ; r9 = texPos"

  "; setup framebuffer addresses"
  "load32 0x7B00000 r1"
  "add r1 r4 r1                ; r1 = fb + x (column top, y=0)"

  "multu r5 320 r2"
  "add r2 r1 r5                ; r5 = fb addr of wall start"

  "multu r6 320 r2"
  "add r2 r1 r6                ; r6 = fb addr of wall end"

  "load 239 r2"
  "multu r2 320 r2"
  "add r2 r1 r8                ; r8 = fb addr of last scanline"

  "; -- ceiling --"
  "load 27 r3                  ; 0x1B ceiling color"
  "W3D_T_ceil:"
  "  write 0 r1 r3"
  "  add r1 320 r1"
  "  blt r1 r5 W3D_T_ceil"

  "; -- textured wall: branch on side --"
  "beq r7 r0 W3D_T_bright"

  "; DARK wall (side == 1)"
  "load 109 r4                 ; 0x6D darken mask"
  "W3D_T_dark:"
  "  shiftr r9 16 r3           ; texY = texPos >> 16"
  "  and r3 r12 r3             ; texY &= 63"
  "  shiftl r3 6 r3            ; texY *= 64"
  "  add r3 r11 r3             ; addr = texBase + texY * 64"
  "  read 0 r3 r3              ; color = texture pixel"
  "  shiftr r3 1 r3            ; color >>= 1"
  "  and r3 r4 r3              ; color &= 0x6D"
  "  write 0 r1 r3             ; write to framebuffer"
  "  add r1 320 r1             ; next row"
  "  add r9 r10 r9             ; texPos += texStep"
  "  blt r1 r6 W3D_T_dark"
  "  jump W3D_T_floor"

  "; BRIGHT wall (side == 0)"
  "W3D_T_bright:"
  "  shiftr r9 16 r3           ; texY = texPos >> 16"
  "  and r3 r12 r3             ; texY &= 63"
  "  shiftl r3 6 r3            ; texY *= 64"
  "  add r3 r11 r3             ; addr = texBase + texY * 64"
  "  read 0 r3 r3              ; color = texture pixel"
  "  write 0 r1 r3             ; write to framebuffer"
  "  add r1 320 r1             ; next row"
  "  add r9 r10 r9             ; texPos += texStep"
  "  blt r1 r6 W3D_T_bright"

  "; -- floor --"
  "W3D_T_floor:"
  "  load 218 r3               ; 0xDA floor color"
  "W3D_T_floor_loop:"
  "  write 0 r1 r3"
  "  add r1 320 r1"
  "  blt r1 r8 W3D_T_floor_loop"

  "; restore registers"
  "pop r12"
  "pop r11"
  "pop r10"
  "pop r9"
  "pop r8"
  "pop r3"
  "pop r2"
  "pop r1"
  );
}

// Render one frame using DDA raycasting with textured walls.
void render_frame()
{
  int x;

  for (x = 0; x < SCREEN_W; x++)
  {
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
    int unclampedStart;

    fixed_t wallX;
    int texX;
    int texIndex;
    fixed_t texStep;
    fixed_t texPos;

    // ---- DDA raycasting ----

    cameraX = __divfp(int2fixed(2 * x), int2fixed(SCREEN_W)) - FIXED_ONE;
    rayDirX = dirX + __multfp(planeX, cameraX);
    rayDirY = dirY + __multfp(planeY, cameraX);

    mapX = fixed2int(posX);
    mapY = fixed2int(posY);

    if (rayDirX == 0)
      deltaDistX = 1 << 30;
    else
      deltaDistX = fixed_abs(__divfp(FIXED_ONE, rayDirX));

    if (rayDirY == 0)
      deltaDistY = 1 << 30;
    else
      deltaDistY = fixed_abs(__divfp(FIXED_ONE, rayDirY));

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

    if (side == 0)
      perpWallDist = sideDistX - deltaDistX;
    else
      perpWallDist = sideDistY - deltaDistY;

    // ---- Wall strip geometry ----

    lineHeight = fixed2int(__divfp(int2fixed(SCREEN_H), perpWallDist));
    if (lineHeight > 10000) lineHeight = 10000;

    unclampedStart = -lineHeight / 2 + SCREEN_H / 2;
    drawStart = unclampedStart;
    if (drawStart < 0) drawStart = 0;
    drawEnd = lineHeight / 2 + SCREEN_H / 2;
    if (drawEnd >= SCREEN_H) drawEnd = SCREEN_H - 1;

    // ---- Texture coordinates ----

    if (side == 0)
      wallX = posY + __multfp(perpWallDist, rayDirY);
    else
      wallX = posX + __multfp(perpWallDist, rayDirX);
    wallX = fixed_frac(wallX);

    texX = fixed2int(__multfp(wallX, int2fixed(TEX_SIZE)));
    if (texX >= TEX_SIZE) texX = TEX_SIZE - 1;

    if (side == 0 && rayDirX > 0) texX = TEX_SIZE - 1 - texX;
    if (side == 1 && rayDirY < 0) texX = TEX_SIZE - 1 - texX;

    texIndex = worldMap[mapX][mapY] - 1;
    if (texIndex < 0) texIndex = 0;
    if (texIndex >= NUM_TEXTURES) texIndex = 0;

    texStep = __divfp(int2fixed(TEX_SIZE), int2fixed(lineHeight));
    texPos = __multfp(int2fixed(drawStart - unclampedStart), texStep);

    // ---- Set globals and call assembly renderer ----

    g_texBase = (int)(textures + texIndex * TEX_PIXELS + texX);
    g_texStep = texStep;
    g_texPos = texPos;

    draw_textured_column(x, drawStart, drawEnd, side);
  }
}

// Drain the HID key event FIFO to prevent overflow.
void drain_key_fifo()
{
  while (sys_key_available())
  {
    sys_read_key();
  }
}

// Process keyboard input and update player position/angle.
// dt_ms: delta time in milliseconds since last frame.
void process_input(int dt_ms)
{
  int keys;
  fixed_t mx;
  fixed_t my;
  fixed_t frameScale;
  fixed_t scaledMove;
  int scaledRot;

  // Scale movement by delta time (relative to 30 FPS target)
  // Use milliseconds to avoid int2fixed overflow (33ms fits, 33333us overflows)
  frameScale = __divfp(int2fixed(dt_ms), int2fixed(TARGET_DT_MS));
  scaledMove = __multfp(MOVE_SPEED, frameScale);
  scaledRot = fixed2int(__multfp(int2fixed(ROT_SPEED), frameScale));
  if (scaledRot < 1) scaledRot = 1;

  keys = sys_get_key_state();

  if (keys & (KEYSTATE_A | KEYSTATE_LEFT))
  {
    rotationAngle -= scaledRot;
    if (rotationAngle < 0) rotationAngle += 360;
    update_camera();
  }
  if (keys & (KEYSTATE_D | KEYSTATE_RIGHT))
  {
    rotationAngle += scaledRot;
    if (rotationAngle >= 360) rotationAngle -= 360;
    update_camera();
  }

  if (keys & (KEYSTATE_W | KEYSTATE_UP))
  {
    mx = __multfp(dirX, scaledMove);
    my = __multfp(dirY, scaledMove);
    if (worldMap[fixed2int(posX + mx)][fixed2int(posY)] == 0)
      posX += mx;
    if (worldMap[fixed2int(posX)][fixed2int(posY + my)] == 0)
      posY += my;
  }

  if (keys & (KEYSTATE_S | KEYSTATE_DOWN))
  {
    mx = __multfp(dirX, scaledMove);
    my = __multfp(dirY, scaledMove);
    if (worldMap[fixed2int(posX - mx)][fixed2int(posY)] == 0)
      posX -= mx;
    if (worldMap[fixed2int(posX)][fixed2int(posY - my)] == 0)
      posY -= my;
  }

  if (keys & KEYSTATE_Q)
  {
    mx = __multfp(-dirY, scaledMove);
    my = __multfp(dirX, scaledMove);
    if (worldMap[fixed2int(posX + mx)][fixed2int(posY)] == 0)
      posX += mx;
    if (worldMap[fixed2int(posX)][fixed2int(posY + my)] == 0)
      posY += my;
  }

  if (keys & KEYSTATE_E)
  {
    mx = __multfp(dirY, scaledMove);
    my = __multfp(-dirX, scaledMove);
    if (worldMap[fixed2int(posX + mx)][fixed2int(posY)] == 0)
      posX += mx;
    if (worldMap[fixed2int(posX)][fixed2int(posY + my)] == 0)
      posY += my;
  }
}

int main()
{
  int keys;
  unsigned int last_time;
  unsigned int now;
  unsigned int dt_us;
  int dt_ms;

  posX = int2fixed(8) + FIXED_HALF;
  posY = int2fixed(9) + FIXED_HALF;
  rotationAngle = 0;
  update_camera();

  generate_textures();

  sys_term_clear();
  clear_framebuffer();

  last_time = get_micros();

  while (1)
  {
    render_frame();

    // Compute delta time in milliseconds
    now = get_micros();
    dt_us = now - last_time;
    last_time = now;
    dt_ms = dt_us / 1000;
    if (dt_ms < 1) dt_ms = 1;
    if (dt_ms > 200) dt_ms = 200;

    process_input(dt_ms);

    // Drain HID event FIFO to prevent overflow
    drain_key_fifo();

    keys = sys_get_key_state();
    if (keys & KEYSTATE_ESCAPE)
    {
      break;
    }
  }

  clear_framebuffer();
  sys_term_clear();
  return 0;
}
