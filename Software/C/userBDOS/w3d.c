// w3d.c — Wolfenstein 3D-style raycaster for FPGC BDOS
// Uses pixel framebuffer via __builtin_store for inline VRAM writes.

#include <syscall.h>
#include <fixedmath.h>
#include <plot.h>
#include <time.h>

// ---- Constants ----

#define MAP_W         24
#define MAP_H         24
#define SCREEN_W      320
#define SCREEN_H      240
#define TEX_SIZE      64
#define TEX_PIXELS    4096

// Viewport (Doom-style reduced screen area)
#define VIEW_X        32
#define VIEW_Y        20
#define VIEW_W        256   // 320 - 2*32
#define VIEW_H        200   // 240 - 2*20

// Border color
#define BORDER_COLOR  0x49

// Camera FOV scale (about 0.66 in 16.16 fixed-point)
#define FOV_SCALE     43254

// Movement tuning
#define MOVE_SPEED    3072
#define ROT_SPEED     2
#define WALL_MARGIN   8192

// Ceiling and floor colors (R3G3B2)
#define CEIL_COLOR    0x1B
#define FLOOR_COLOR   0xDA

// Number of textures
#define NUM_TEXTURES  8

// Texture data file path on BRFS
#define TEXTURE_FILE  "/data/w3d/textures.dat"

// ---- Map ----

int worldMap[MAP_W][MAP_H] =
{
  {4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 7, 7, 7, 7, 7, 7, 7, 7},
  {4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 7, 0, 0, 0, 0, 0, 0, 7},
  {4, 0, 1, 2, 3, 4, 5, 6, 7, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 7},
  {4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 7},
  {4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 7, 0, 0, 0, 0, 0, 0, 7},
  {4, 0, 0, 0, 0, 0, 0, 2, 2, 2, 2, 2, 2, 2, 2, 2, 7, 7, 0, 7, 7, 7, 7, 7},
  {4, 0, 0, 0, 0, 0, 0, 2, 0, 1, 0, 1, 0, 1, 0, 2, 7, 0, 0, 0, 7, 7, 7, 1},
  {4, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 2, 7, 0, 0, 0, 0, 0, 0, 8},
  {4, 0, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 7, 7, 7, 1},
  {4, 0, 2, 2, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 5, 7, 0, 0, 0, 0, 0, 0, 8},
  {4, 0, 0, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 5, 7, 0, 0, 0, 7, 7, 7, 1},
  {4, 0, 0, 0, 0, 0, 0, 5, 5, 5, 5, 0, 5, 5, 5, 5, 7, 7, 7, 7, 7, 7, 7, 1},
  {6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 0, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6},
  {8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4},
  {6, 6, 6, 6, 6, 6, 0, 6, 6, 6, 6, 0, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6},
  {4, 4, 4, 4, 4, 4, 0, 4, 4, 4, 6, 0, 6, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3},
  {4, 0, 0, 0, 0, 0, 0, 0, 0, 4, 6, 0, 6, 2, 0, 0, 0, 0, 0, 2, 0, 0, 0, 2},
  {4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 6, 2, 0, 0, 5, 0, 0, 2, 0, 0, 0, 2},
  {4, 0, 0, 0, 0, 0, 0, 0, 0, 4, 6, 0, 6, 2, 0, 0, 0, 0, 0, 2, 2, 0, 2, 2},
  {4, 0, 6, 0, 6, 0, 0, 0, 0, 4, 6, 0, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 2},
  {4, 0, 0, 5, 0, 0, 0, 0, 0, 4, 6, 0, 6, 2, 0, 0, 0, 0, 0, 2, 2, 0, 2, 2},
  {4, 0, 6, 0, 6, 0, 0, 0, 0, 4, 6, 0, 6, 2, 0, 0, 5, 0, 0, 2, 0, 0, 0, 2},
  {4, 0, 0, 0, 0, 0, 0, 0, 0, 4, 6, 0, 6, 2, 0, 0, 0, 0, 0, 2, 0, 0, 0, 2},
  {4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 1, 1, 1, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3}
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

// ---- Functions ----

void update_camera(void)
{
  dirX = -fixed_cos(rotationAngle);
  dirY = fixed_sin(rotationAngle);
  planeX = __builtin_multfp(fixed_sin(rotationAngle), FOV_SCALE);
  planeY = __builtin_multfp(fixed_cos(rotationAngle), FOV_SCALE);
}

void clear_framebuffer(void)
{
  int i;
  for (i = 0; i < SCREEN_W * SCREEN_H; i++)
  {
    __builtin_store(PIXEL_FB_ADDR + i * 4, 0);
  }
}

void draw_border(void)
{
  int x;
  int y;

  // Top border
  for (y = 0; y < VIEW_Y; y++)
  {
    for (x = 0; x < SCREEN_W; x++)
    {
      __builtin_store(PIXEL_FB_ADDR + (y * SCREEN_W + x) * 4, BORDER_COLOR);
    }
  }

  // Bottom border
  for (y = VIEW_Y + VIEW_H; y < SCREEN_H; y++)
  {
    for (x = 0; x < SCREEN_W; x++)
    {
      __builtin_store(PIXEL_FB_ADDR + (y * SCREEN_W + x) * 4, BORDER_COLOR);
    }
  }

  // Left and right borders
  for (y = VIEW_Y; y < VIEW_Y + VIEW_H; y++)
  {
    for (x = 0; x < VIEW_X; x++)
    {
      __builtin_store(PIXEL_FB_ADDR + (y * SCREEN_W + x) * 4, BORDER_COLOR);
    }
    for (x = VIEW_X + VIEW_W; x < SCREEN_W; x++)
    {
      __builtin_store(PIXEL_FB_ADDR + (y * SCREEN_W + x) * 4, BORDER_COLOR);
    }
  }
}

int load_textures_from_file(void)
{
  int fd;
  int fsize;
  int expected;
  int words_remaining;
  int chunk;
  int words_read;
  int dest_idx;

  expected = NUM_TEXTURES * TEX_PIXELS;

  fd = sys_fs_open(TEXTURE_FILE);
  if (fd < 0)
  {
    return 0;
  }

  fsize = sys_fs_filesize(fd);
  if (fsize < expected)
  {
    sys_fs_close(fd);
    return 0;
  }

  textures = (unsigned int *)sys_heap_alloc(expected);
  if (textures == (unsigned int *)0)
  {
    sys_fs_close(fd);
    return 0;
  }

  dest_idx = 0;
  words_remaining = expected;
  while (words_remaining > 0)
  {
    chunk = words_remaining;
    if (chunk > 256)
    {
      chunk = 256;
    }
    words_read = sys_fs_read(fd, &textures[dest_idx], chunk);
    if (words_read <= 0)
    {
      break;
    }
    dest_idx = dest_idx + words_read;
    words_remaining = words_remaining - words_read;
  }

  sys_fs_close(fd);
  return (words_remaining == 0) ? 1 : 0;
}

void generate_textures(void)
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

// Draw one textured column — ceiling, wall, floor.
void draw_textured_column(int screenX, int drawStart, int drawEnd, int side,
                          unsigned int *texBase, fixed_t texStep, fixed_t texPos)
{
  int addr;
  int row;
  int texY;
  int color;

  // Base address for this column in the viewport
  addr = PIXEL_FB_ADDR + (VIEW_Y * SCREEN_W + VIEW_X + screenX) * 4;

  // Ceiling
  for (row = 0; row < drawStart; row++)
  {
    __builtin_store(addr, CEIL_COLOR);
    addr += SCREEN_W * 4;
  }

  // Textured wall
  for (row = drawStart; row <= drawEnd; row++)
  {
    texY = (texPos >> 16) & 63;
    color = texBase[texY * TEX_SIZE];
    if (side)
    {
      color = (color >> 1) & 0x6D;
    }
    __builtin_store(addr, color);
    addr += SCREEN_W * 4;
    texPos += texStep;
  }

  // Floor
  for (row = drawEnd + 1; row < VIEW_H; row++)
  {
    __builtin_store(addr, FLOOR_COLOR);
    addr += SCREEN_W * 4;
  }
}

// Render one frame using DDA raycasting with textured walls.
void render_frame(void)
{
  int x;

  for (x = 0; x < VIEW_W; x++)
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

    // Previous workaround for old CPU bug: cameraX = __builtin_divfp((2 * x) << 16, VIEW_W << 16) - FIXED_ONE;
    cameraX = cameraX = __builtin_divfp(int2fixed(2 * x), int2fixed(VIEW_W)) - FIXED_ONE;
    rayDirX = dirX + __builtin_multfp(planeX, cameraX);
    rayDirY = dirY + __builtin_multfp(planeY, cameraX);

    mapX = fixed2int(posX);
    mapY = fixed2int(posY);

    if (rayDirX == 0)
      deltaDistX = 1 << 30;
    else
      deltaDistX = fixed_abs(__builtin_divfp(FIXED_ONE, rayDirX));

    if (rayDirY == 0)
      deltaDistY = 1 << 30;
    else
      deltaDistY = fixed_abs(__builtin_divfp(FIXED_ONE, rayDirY));

    if (rayDirX < 0)
    {
      stepX = -1;
      sideDistX = __builtin_multfp(posX - int2fixed(mapX), deltaDistX);
    }
    else
    {
      stepX = 1;
      sideDistX = __builtin_multfp(int2fixed(mapX + 1) - posX, deltaDistX);
    }

    if (rayDirY < 0)
    {
      stepY = -1;
      sideDistY = __builtin_multfp(posY - int2fixed(mapY), deltaDistY);
    }
    else
    {
      stepY = 1;
      sideDistY = __builtin_multfp(int2fixed(mapY + 1) - posY, deltaDistY);
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

    // ---- Wall strip geometry (viewport-relative) ----

    lineHeight = fixed2int(__builtin_divfp(int2fixed(VIEW_H), perpWallDist));
    if (lineHeight > 10000) lineHeight = 10000;

    unclampedStart = -lineHeight / 2 + VIEW_H / 2;
    drawStart = unclampedStart;
    if (drawStart < 0) drawStart = 0;
    drawEnd = lineHeight / 2 + VIEW_H / 2;
    if (drawEnd >= VIEW_H) drawEnd = VIEW_H - 1;

    // ---- Texture coordinates ----

    if (side == 0)
      wallX = posY + __builtin_multfp(perpWallDist, rayDirY);
    else
      wallX = posX + __builtin_multfp(perpWallDist, rayDirX);
    wallX = fixed_frac(wallX);

    texX = fixed2int(__builtin_multfp(wallX, int2fixed(TEX_SIZE)));
    if (texX >= TEX_SIZE) texX = TEX_SIZE - 1;

    if (side == 0 && rayDirX > 0) texX = TEX_SIZE - 1 - texX;
    if (side == 1 && rayDirY < 0) texX = TEX_SIZE - 1 - texX;

    texIndex = worldMap[mapX][mapY] - 1;
    if (texIndex < 0) texIndex = 0;
    if (texIndex >= NUM_TEXTURES) texIndex = 0;

    texStep = __builtin_divfp(int2fixed(TEX_SIZE), int2fixed(lineHeight));
    texPos = __builtin_multfp(int2fixed(drawStart - unclampedStart), texStep);

    // ---- Draw column ----

    draw_textured_column(x, drawStart, drawEnd, side,
                         textures + texIndex * TEX_PIXELS + texX,
                         texStep, texPos);
  }
}

void drain_key_fifo(void)
{
  while (sys_key_available())
  {
    sys_read_key();
  }
}

void process_input(void)
{
  int keys;
  fixed_t mx;
  fixed_t my;

  keys = sys_get_key_state();

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

  if (keys & (KEYSTATE_W | KEYSTATE_UP))
  {
    mx = __builtin_multfp(dirX, MOVE_SPEED);
    my = __builtin_multfp(dirY, MOVE_SPEED);
    if (worldMap[fixed2int(posX + mx + WALL_MARGIN)][fixed2int(posY)] == 0 &&
        worldMap[fixed2int(posX + mx - WALL_MARGIN)][fixed2int(posY)] == 0)
      posX += mx;
    if (worldMap[fixed2int(posX)][fixed2int(posY + my + WALL_MARGIN)] == 0 &&
        worldMap[fixed2int(posX)][fixed2int(posY + my - WALL_MARGIN)] == 0)
      posY += my;
  }

  if (keys & (KEYSTATE_S | KEYSTATE_DOWN))
  {
    mx = __builtin_multfp(dirX, MOVE_SPEED);
    my = __builtin_multfp(dirY, MOVE_SPEED);
    if (worldMap[fixed2int(posX - mx + WALL_MARGIN)][fixed2int(posY)] == 0 &&
        worldMap[fixed2int(posX - mx - WALL_MARGIN)][fixed2int(posY)] == 0)
      posX -= mx;
    if (worldMap[fixed2int(posX)][fixed2int(posY - my + WALL_MARGIN)] == 0 &&
        worldMap[fixed2int(posX)][fixed2int(posY - my - WALL_MARGIN)] == 0)
      posY -= my;
  }

  if (keys & KEYSTATE_Q)
  {
    mx = __builtin_multfp(-dirY, MOVE_SPEED);
    my = __builtin_multfp(dirX, MOVE_SPEED);
    if (worldMap[fixed2int(posX + mx + WALL_MARGIN)][fixed2int(posY)] == 0 &&
        worldMap[fixed2int(posX + mx - WALL_MARGIN)][fixed2int(posY)] == 0)
      posX += mx;
    if (worldMap[fixed2int(posX)][fixed2int(posY + my + WALL_MARGIN)] == 0 &&
        worldMap[fixed2int(posX)][fixed2int(posY + my - WALL_MARGIN)] == 0)
      posY += my;
  }

  if (keys & KEYSTATE_E)
  {
    mx = __builtin_multfp(dirY, MOVE_SPEED);
    my = __builtin_multfp(-dirX, MOVE_SPEED);
    if (worldMap[fixed2int(posX + mx + WALL_MARGIN)][fixed2int(posY)] == 0 &&
        worldMap[fixed2int(posX + mx - WALL_MARGIN)][fixed2int(posY)] == 0)
      posX += mx;
    if (worldMap[fixed2int(posX)][fixed2int(posY + my + WALL_MARGIN)] == 0 &&
        worldMap[fixed2int(posX)][fixed2int(posY + my - WALL_MARGIN)] == 0)
      posY += my;
  }
}

int main(void)
{
  int keys;
  unsigned int last_time;
  unsigned int now;
  unsigned int dt_us;
  int dt_ms;

  posX = int2fixed(15);
  posY = int2fixed(11) + FIXED_HALF;
  rotationAngle = 0;
  update_camera();

  if (!load_textures_from_file())
  {
    generate_textures();
  }

  sys_term_clear();
  clear_framebuffer();
  draw_border();

  last_time = get_micros();

  while (1)
  {
    render_frame();

    now = get_micros();
    dt_us = now - last_time;
    last_time = now;
    dt_ms = dt_us / 1000;
    if (dt_ms < 1) dt_ms = 1;
    if (dt_ms > 200) dt_ms = 200;

    process_input();

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
