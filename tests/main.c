#include "gamma/render.h"
#include "gamma/scene.h"

#include <stdint.h>
#include <string.h>
#include <allegro.h>

#define WIDTH       80
#define HEIGHT      60
#define WINDOW_W    640
#define WINDOW_H    480
#define TIMER_HZ    100

static gm_framebuffer FB;
static BITMAP *FB_BMP;

static uint32_t rtab[256];
static uint32_t gtab[256];
static uint32_t btab[256];

static int gfx_depth;

static volatile unsigned long timer_ticks = 0;

static void timer_tick(void) {
    ++timer_ticks;
}
END_OF_FUNCTION(timer_tick)

static int init_framebuffer(gm_vec2u size, gm_color fill) {
    FB = gm_framebuffer_alloc(size, fill);

    if (!FB.pixels)
        return 0;

    FB_BMP = create_bitmap(WINDOW_W, WINDOW_H);

    if (!FB_BMP)
        return 0;

    for (int i = 0; i < 256; ++i) {
        rtab[i] = makecol(i, 0, 0);
        gtab[i] = makecol(0, i, 0);
        btab[i] = makecol(0, 0, i);
    }

    return 1;
}

static int set_mode(void) {
    static const int depths[] = {16, 15, 32};

    for (int i = 0; i < 3; ++i) {
        set_color_depth(depths[i]);

        if (set_gfx_mode(GFX_AUTODETECT, WINDOW_W, WINDOW_H, 0, 0) == 0)
            return depths[i];
    }

    return 0;
}

static void draw_framebuffer(void) {
    /*
     * Gamma's framebuffer uses RGBA byte ordering.
     * Convert to the screen depth and scale each pixel 8x.
     */
    const uint32_t *src = (const uint32_t *)FB.pixels;
    const unsigned row_bytes = WINDOW_W * ((gfx_depth + 7) / 8);

    for (unsigned y = 0; y < HEIGHT; ++y) {
        uint32_t *dst = (uint32_t *)FB_BMP->line[y * 8];

        if (gfx_depth == 32) {
            for (unsigned x = 0; x < WIDTH; ++x) {
                uint32_t p = *src++;

                uint32_t c =
                    rtab[p & 0xFF] |
                    gtab[(p >> 8) & 0xFF] |
                    btab[(p >> 16) & 0xFF];

                for (unsigned i = 0; i < 8; ++i)
                    dst[x * 8 + i] = c;
            }
        } else {
            for (unsigned x = 0; x < WIDTH; ++x) {
                uint32_t p = *src++;

                uint32_t c =
                    rtab[p & 0xFF] |
                    gtab[(p >> 8) & 0xFF] |
                    btab[(p >> 16) & 0xFF];

                uint32_t packed = c | (c << 16);
                for (unsigned i = 0; i < 4; ++i)
                    dst[x * 4 + i] = packed;
            }
        }

        for (unsigned i = 1; i < 8; ++i)
            memcpy(FB_BMP->line[y * 8 + i], dst, row_bytes);
    }

    vsync();
    blit(FB_BMP, screen, 0, 0, 0, 0, WINDOW_W, WINDOW_H);
}


#define CHUNK_X 16
#define CHUNK_Y 16
#define CHUNK_Z 16

gm_color block_colors[] = {
    {0, 0, 0, 0}, // air
    {85, 255, 85, 255}, // grass
    {100, 100, 255, 80}, // water
    {180, 180, 180, 255}, // stone
};
// YZX
typedef uint8_t chunkdata[CHUNK_Y][CHUNK_Z][CHUNK_Z];
chunkdata display_chunk = {
    {
        {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
        {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
        {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
        {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
        {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
        {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
        {1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1},
        {1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1},
        {1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1},
        {1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1},
        {1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1},
        {1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1},
        {1, 1, 2, 2, 2, 2, 1, 2, 2, 2, 2, 2, 1, 1, 1, 1},
        {1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1},
        {1, 1, 1, 1, 2, 1, 2, 1, 2, 2, 1, 1, 1, 1, 1, 1},
        {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
    },
    {
        {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
        {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
        {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
        {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
        {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
        {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
        {1, 1, 1, 1, 1, 1, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1},
        {1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1},
        {1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1},
        {1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1},
        {1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1},
        {1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1},
        {1, 1, 1, 2, 2, 2, 1, 2, 2, 2, 2, 2, 1, 1, 1, 1},
        {1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1},
        {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
        {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
    },
    {
        {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
        {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
        {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
        {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
        {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
        {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
        {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
        {1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1},
        {1, 1, 1, 1, 2, 2, 2, 1, 1, 1, 2, 2, 1, 1, 1, 1},
        {1, 1, 1, 1, 2, 2, 2, 1, 1, 2, 2, 2, 1, 1, 1, 1},
        {1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1},
        {1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1},
        {1, 1, 1, 1, 2, 2, 1, 2, 2, 1, 1, 1, 1, 1, 1, 1},
        {1, 1, 1, 1, 1, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
        {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
        {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
    },
    {
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 1, 1, 1, 3, 1, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 1, 1, 1, 1, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 1, 1, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 1, 3, 1, 3, 1, 1, 1, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 1, 3, 3, 1, 3, 1, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
    },
    {
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
    },
    {
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
    },
    {
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
    },
    {
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
    },
    {
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
    },
    {
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
    },
    {
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
    },
    {
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
    },
    {
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
    },
    {
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
    },
    {
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
    },
    {
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
    },
};

static gm_tri3 tris[49152] = {0};
static uint32_t tri_c = 0;

typedef enum {
    UP, DOWN, LEFT, RIGHT,
    FRONT, BACK,
} blockface;
static void generate_face(gm_tri3 *tris, blockface face, gm_vec3i pos, gm_color color) {
    switch (face) {
        case UP:
            tris[tri_c++] = (gm_tri3) {
                {(float)pos.x - 0.5f, (float)pos.y + 0.5f, (float)pos.z - 0.5f},
                {(float)pos.x - 0.5f, (float)pos.y + 0.5f, (float)pos.z + 0.5f},
                {(float)pos.x + 0.5f, (float)pos.y + 0.5f, (float)pos.z + 0.5f},
                color, color, color
            };
            tris[tri_c++] = (gm_tri3) {
                {(float)pos.x - 0.5f, (float)pos.y + 0.5f, (float)pos.z - 0.5f},
                {(float)pos.x + 0.5f, (float)pos.y + 0.5f, (float)pos.z + 0.5f},
                {(float)pos.x + 0.5f, (float)pos.y + 0.5f, (float)pos.z - 0.5f},
                color, color, color
            };
            break;
        case DOWN:
            tris[tri_c++] = (gm_tri3) {
                {(float)pos.x - 0.5f, (float)pos.y - 0.5f, (float)pos.z - 0.5f},
                {(float)pos.x + 0.5f, (float)pos.y - 0.5f, (float)pos.z - 0.5f},
                {(float)pos.x + 0.5f, (float)pos.y - 0.5f, (float)pos.z + 0.5f},
                color, color, color
            };
            tris[tri_c++] = (gm_tri3) {
                {(float)pos.x - 0.5f, (float)pos.y - 0.5f, (float)pos.z - 0.5f},
                {(float)pos.x + 0.5f, (float)pos.y - 0.5f, (float)pos.z + 0.5f},
                {(float)pos.x - 0.5f, (float)pos.y - 0.5f, (float)pos.z + 0.5f},
                color, color, color
            };
            break;
        case LEFT:
            tris[tri_c++] = (gm_tri3) {
                {(float)pos.x - 0.5f, (float)pos.y - 0.5f, (float)pos.z - 0.5f},
                {(float)pos.x - 0.5f, (float)pos.y - 0.5f, (float)pos.z + 0.5f},
                {(float)pos.x - 0.5f, (float)pos.y + 0.5f, (float)pos.z + 0.5f},
                color, color, color
            };
            tris[tri_c++] = (gm_tri3) {
                {(float)pos.x - 0.5f, (float)pos.y - 0.5f, (float)pos.z - 0.5f},
                {(float)pos.x - 0.5f, (float)pos.y + 0.5f, (float)pos.z + 0.5f},
                {(float)pos.x - 0.5f, (float)pos.y + 0.5f, (float)pos.z - 0.5f},
                color, color, color
            };
            break;
        case RIGHT:
            tris[tri_c++] = (gm_tri3) {
                {(float)pos.x + 0.5f, (float)pos.y - 0.5f, (float)pos.z - 0.5f},
                {(float)pos.x + 0.5f, (float)pos.y + 0.5f, (float)pos.z - 0.5f},
                {(float)pos.x + 0.5f, (float)pos.y + 0.5f, (float)pos.z + 0.5f},
                color, color, color
            };
            tris[tri_c++] = (gm_tri3) {
                {(float)pos.x + 0.5f, (float)pos.y - 0.5f, (float)pos.z - 0.5f},
                {(float)pos.x + 0.5f, (float)pos.y + 0.5f, (float)pos.z + 0.5f},
                {(float)pos.x + 0.5f, (float)pos.y - 0.5f, (float)pos.z + 0.5f},
                color, color, color
            };
            break;
        case FRONT:
            tris[tri_c++] = (gm_tri3) {
                {(float)pos.x - 0.5f, (float)pos.y - 0.5f, (float)pos.z + 0.5f},
                {(float)pos.x + 0.5f, (float)pos.y - 0.5f, (float)pos.z + 0.5f},
                {(float)pos.x + 0.5f, (float)pos.y + 0.5f, (float)pos.z + 0.5f},
                color, color, color
            };
            tris[tri_c++] = (gm_tri3) {
                {(float)pos.x - 0.5f, (float)pos.y - 0.5f, (float)pos.z + 0.5f},
                {(float)pos.x + 0.5f, (float)pos.y + 0.5f, (float)pos.z + 0.5f},
                {(float)pos.x - 0.5f, (float)pos.y + 0.5f, (float)pos.z + 0.5f},
                color, color, color
            };
            break;
        case BACK:
            tris[tri_c++] = (gm_tri3) {
                {(float)pos.x - 0.5f, (float)pos.y - 0.5f, (float)pos.z - 0.5f},
                {(float)pos.x + 0.5f, (float)pos.y + 0.5f, (float)pos.z - 0.5f},
                {(float)pos.x + 0.5f, (float)pos.y - 0.5f, (float)pos.z - 0.5f},
                color, color, color
            };
            tris[tri_c++] = (gm_tri3) {
                {(float)pos.x - 0.5f, (float)pos.y - 0.5f, (float)pos.z - 0.5f},
                {(float)pos.x - 0.5f, (float)pos.y + 0.5f, (float)pos.z - 0.5f},
                {(float)pos.x + 0.5f, (float)pos.y + 0.5f, (float)pos.z - 0.5f},
                color, color, color
            };
            break;
    }
}

static gm_mesh build_chunk(chunkdata chunk) {
    for (int y = CHUNK_Y - 1; y >= 0; --y) {
        for (int z = 0; z < CHUNK_Z; ++z) {
            for (int x = 0; x < CHUNK_X; ++x) {
                uint8_t block = chunk[y][z][x];
                if (block == 0) continue;

                uint8_t up = (y == 0) ? 0 : chunk[y - 1][z][x];
                uint8_t down = (y == CHUNK_Y - 1) ? 0 : chunk[y + 1][z][x];

                uint8_t left = (x == 0) ? 0 : chunk[y][z][x - 1];
                uint8_t right = (x == CHUNK_X - 1) ? 0 : chunk[y][z][x + 1];

                uint8_t front = (z == CHUNK_Z - 1) ? 0 : chunk[y][z + 1][x];
                uint8_t back = (z == 0) ? 0 : chunk[y][z - 1][x];

                gm_vec3i pos = {x, CHUNK_Y - 1 - y, z};
                gm_color color = block_colors[block];
                if (up != block && block_colors[up].a != 255)
                    generate_face(tris, UP, pos, color);
                if (down != block && block_colors[down].a != 255)
                    generate_face(tris, DOWN, pos, color);
                if (left != block && block_colors[left].a != 255)
                    generate_face(tris, LEFT, pos, color);
                if (right != block && block_colors[right].a != 255)
                    generate_face(tris, RIGHT, pos, color);
                if (front != block && block_colors[front].a != 255)
                    generate_face(tris, FRONT, pos, color);
                if (back != block && block_colors[back].a != 255)
                    generate_face(tris, BACK, pos, color);
            }
        }
    }

    gm_mesh mesh = gm_mesh_new(tri_c, tris);

    return mesh;
}


int main(void) {
    allegro_init();
    install_keyboard();
    install_timer();

    gfx_depth = set_mode();

    if (!gfx_depth) {
        allegro_message("Unable to initialize graphics:\n%s",
            allegro_error);
        allegro_exit();
        return 1;
    }

    LOCK_VARIABLE(timer_ticks);
    LOCK_FUNCTION(timer_tick);

    install_int_ex(timer_tick, BPS_TO_TIMER(TIMER_HZ));

    if (!init_framebuffer(
        (gm_vec2u){WIDTH, HEIGHT},
        GM_BLACK
    )) {
        remove_int(timer_tick);
        allegro_exit();
        return 1;
    }

    gm_scene scene = gm_scene_new(GM_DARKBLUE);

    gm_camera ncam = gm_camera_perspective((gm_vec2u){WIDTH, HEIGHT}, GM_DARKBLUE, 90.0f, 0.1f, 10.0f);
    ncam.transform.position = (gm_vec3){0, 10, -20};
    ncam.transform.rotation.x = 22.5;
    gm_handle cam = gm_scene_addcam(&scene, ncam);

    gm_mesh chunk_mesh = build_chunk(display_chunk);
    gm_handle chunk_m = gm_scene_addmesh(&scene, chunk_mesh);
    gm_scene_addinst(&scene, chunk_m);

    unsigned long previous_ticks = timer_ticks;

    while (!key[KEY_ESC]) {
        unsigned long current_ticks = timer_ticks;

        float delta_time =
            (float)(current_ticks - previous_ticks) / TIMER_HZ;

        if (delta_time > 0.1f)
            delta_time = 0.1f;

        previous_ticks = current_ticks;


        gm_vec3 move = GM_VEC3_NULL;
        float movex1 = key[KEY_A] - key[KEY_D];
        move.x += delta_time * 15.0f * movex1;
        float movey1 = key[KEY_DOWN] - key[KEY_UP];
        move.y += delta_time * 15.0f * movey1;
        float movez1 = key[KEY_S] - key[KEY_W];
        move.z += delta_time * 15.0f * movez1;

        float roty1 = key[KEY_LEFT] - key[KEY_RIGHT];

        gm_camera *main_cam = gm_scene_peekcam(&scene, cam);
        main_cam->transform.rotation.y += delta_time * 100.0f * roty1;
        gm_transform_move(&main_cam->transform, move);

        if (keypressed()) {
            int k = readkey();
            int scancode = k >> 8;
            if (scancode == KEY_1)
                main_cam->passes ^= GM_PASS_OPAQUE;
            if (scancode == KEY_2)
                main_cam->passes ^= GM_PASS_TRANSPARENT;
            if (scancode == KEY_3)
                main_cam->passes ^= GM_PASS_WIREFRAME;

        }

        gm_scene_render(&scene);
        gm_draw_cam(&FB, main_cam, (gm_vec2u){0, 0});
        draw_framebuffer();
    }

    destroy_bitmap(FB_BMP);
    remove_int(timer_tick);
    allegro_exit();

    return 0;
}

END_OF_MAIN()