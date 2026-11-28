#include "gamma/coord.h"
#include "gamma/render.h"
#include "gamma/scene.h"
#include <raylib.h>
#include <stdio.h>
#include <string.h>

#ifdef __APPLE__
#include <pthread.h>
#include <sys/qos.h>
#endif

#define FPS        0
#define FPS_SAMPLE 255
#define WIDTH      320
#define HEIGHT     240
#define SCALE      4

static gm_framebuffer FB;
static Texture2D FB_TEX;
static uint32_t fps_sample[FPS_SAMPLE] = {0};
static uint32_t fps_index = 0;
static char fps_text[16] = {0};
static char passes[64] = {0};
static char debug_text[64] = {0};

static void init_framebuffer(gm_vec2u size, gm_color fill) {
    #ifdef __APPLE__
    pthread_set_qos_class_self_np(QOS_CLASS_USER_INTERACTIVE, 0);
    #endif
    InitWindow(WIDTH * SCALE, HEIGHT * SCALE, "Gamma (Raylib)");
    if (FPS > 0) SetTargetFPS(FPS);

    FB = gm_framebuffer_alloc(size, fill);
    Image img = {
        .data = FB.pixels,
        .width = (int)size.x,
        .height = (int)size.y,
        .mipmaps = 1,
        .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8,
    };
    FB_TEX = LoadTextureFromImage(img);
}

static void draw_framebuffer(void) {
    UpdateTexture(FB_TEX, FB.pixels);

    BeginDrawing();
    ClearBackground(BLACK);
    DrawTexturePro(FB_TEX, (Rectangle){0, 0, WIDTH, HEIGHT}, (Rectangle){0, 0, WIDTH * SCALE, HEIGHT * SCALE}, (Vector2){0, 0}, 0, WHITE);
    DrawText(fps_text, 12, 8, 20, GREEN);
    DrawText(passes, 12, 32, 20, RED);
    DrawText(debug_text, 12, 56, 20, BLUE);
    EndDrawing();
}

#define CHUNK_X 16
#define CHUNK_Y 16
#define CHUNK_Z 16

gm_color block_colors[] = {
    {0, 0, 0, 0}, // air
    GM_LIGHTGREEN, // grass
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

#define VEC_NAME gm_tris
#define VEC_T gm_tri3
#define VSIZE_T uint32_t
#define VSIZE_MAX UINT32_MAX
#include <sf/containers/vec.h>

typedef enum {
    UP, DOWN, LEFT, RIGHT,
    FRONT, BACK,
} blockface;
static void generate_face(gm_tris *tris, blockface face, gm_vec3i pos, gm_color color) {
    switch (face) {
        case UP:
            gm_tris_push(tris, (gm_tri3) {
                {(float)pos.x - 0.5f, (float)pos.y + 0.5f, (float)pos.z - 0.5f},
                {(float)pos.x - 0.5f, (float)pos.y + 0.5f, (float)pos.z + 0.5f},
                {(float)pos.x + 0.5f, (float)pos.y + 0.5f, (float)pos.z + 0.5f},
                color, color, color
            });
            gm_tris_push(tris, (gm_tri3) {
                {(float)pos.x - 0.5f, (float)pos.y + 0.5f, (float)pos.z - 0.5f},
                {(float)pos.x + 0.5f, (float)pos.y + 0.5f, (float)pos.z + 0.5f},
                {(float)pos.x + 0.5f, (float)pos.y + 0.5f, (float)pos.z - 0.5f},
                color, color, color
            });
            break;
        case DOWN:
            gm_tris_push(tris, (gm_tri3) {
                {(float)pos.x - 0.5f, (float)pos.y - 0.5f, (float)pos.z - 0.5f},
                {(float)pos.x + 0.5f, (float)pos.y - 0.5f, (float)pos.z - 0.5f},
                {(float)pos.x + 0.5f, (float)pos.y - 0.5f, (float)pos.z + 0.5f},
                color, color, color
            });
            gm_tris_push(tris, (gm_tri3) {
                {(float)pos.x - 0.5f, (float)pos.y - 0.5f, (float)pos.z - 0.5f},
                {(float)pos.x + 0.5f, (float)pos.y - 0.5f, (float)pos.z + 0.5f},
                {(float)pos.x - 0.5f, (float)pos.y - 0.5f, (float)pos.z + 0.5f},
                color, color, color
            });
            break;
        case LEFT:
            gm_tris_push(tris, (gm_tri3) {
                {(float)pos.x - 0.5f, (float)pos.y - 0.5f, (float)pos.z - 0.5f},
                {(float)pos.x - 0.5f, (float)pos.y - 0.5f, (float)pos.z + 0.5f},
                {(float)pos.x - 0.5f, (float)pos.y + 0.5f, (float)pos.z + 0.5f},
                color, color, color
            });
            gm_tris_push(tris, (gm_tri3) {
                {(float)pos.x - 0.5f, (float)pos.y - 0.5f, (float)pos.z - 0.5f},
                {(float)pos.x - 0.5f, (float)pos.y + 0.5f, (float)pos.z + 0.5f},
                {(float)pos.x - 0.5f, (float)pos.y + 0.5f, (float)pos.z - 0.5f},
                color, color, color
            });
            break;
        case RIGHT:
            gm_tris_push(tris, (gm_tri3) {
                {(float)pos.x + 0.5f, (float)pos.y - 0.5f, (float)pos.z - 0.5f},
                {(float)pos.x + 0.5f, (float)pos.y + 0.5f, (float)pos.z - 0.5f},
                {(float)pos.x + 0.5f, (float)pos.y + 0.5f, (float)pos.z + 0.5f},
                color, color, color
            });
            gm_tris_push(tris, (gm_tri3) {
                {(float)pos.x + 0.5f, (float)pos.y - 0.5f, (float)pos.z - 0.5f},
                {(float)pos.x + 0.5f, (float)pos.y + 0.5f, (float)pos.z + 0.5f},
                {(float)pos.x + 0.5f, (float)pos.y - 0.5f, (float)pos.z + 0.5f},
                color, color, color
            });
            break;
        case FRONT:
            gm_tris_push(tris, (gm_tri3) {
                {(float)pos.x - 0.5f, (float)pos.y - 0.5f, (float)pos.z + 0.5f},
                {(float)pos.x + 0.5f, (float)pos.y - 0.5f, (float)pos.z + 0.5f},
                {(float)pos.x + 0.5f, (float)pos.y + 0.5f, (float)pos.z + 0.5f},
                color, color, color
            });
            gm_tris_push(tris, (gm_tri3) {
                {(float)pos.x - 0.5f, (float)pos.y - 0.5f, (float)pos.z + 0.5f},
                {(float)pos.x + 0.5f, (float)pos.y + 0.5f, (float)pos.z + 0.5f},
                {(float)pos.x - 0.5f, (float)pos.y + 0.5f, (float)pos.z + 0.5f},
                color, color, color
            });
            break;
        case BACK:
            gm_tris_push(tris, (gm_tri3) {
                {(float)pos.x - 0.5f, (float)pos.y - 0.5f, (float)pos.z - 0.5f},
                {(float)pos.x + 0.5f, (float)pos.y + 0.5f, (float)pos.z - 0.5f},
                {(float)pos.x + 0.5f, (float)pos.y - 0.5f, (float)pos.z - 0.5f},
                color, color, color
            });
            gm_tris_push(tris, (gm_tri3) {
                {(float)pos.x - 0.5f, (float)pos.y - 0.5f, (float)pos.z - 0.5f},
                {(float)pos.x - 0.5f, (float)pos.y + 0.5f, (float)pos.z - 0.5f},
                {(float)pos.x + 0.5f, (float)pos.y + 0.5f, (float)pos.z - 0.5f},
                color, color, color
            });
            break;
    }
}

static gm_mesh build_chunk(chunkdata chunk) {
    gm_tris tris = gm_tris_new();
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
                    generate_face(&tris, UP, pos, color);
                if (down != block && block_colors[down].a != 255)
                    generate_face(&tris, DOWN, pos, color);
                if (left != block && block_colors[left].a != 255)
                    generate_face(&tris, LEFT, pos, color);
                if (right != block && block_colors[right].a != 255)
                    generate_face(&tris, RIGHT, pos, color);
                if (front != block && block_colors[front].a != 255)
                    generate_face(&tris, FRONT, pos, color);
                if (back != block && block_colors[back].a != 255)
                    generate_face(&tris, BACK, pos, color);
            }
        }
    }

    gm_mesh mesh = gm_mesh_new(tris.count, tris.data);
    gm_tris_free(&tris);

    return mesh;
}

int minecraft(void) {
    init_framebuffer((gm_vec2u){WIDTH, HEIGHT}, GM_BLACK);
    gm_scene scene = gm_scene_new(GM_DARKBLUE);

    gm_camera ncam = gm_camera_perspective((gm_vec2u){WIDTH, HEIGHT}, GM_DARKBLUE, 90.0f, 0.1f, 30.0f);
    ncam.transform.position = (gm_vec3){0, 10, -20};
    ncam.transform.rotation.x = 22.5;
    gm_handle cam = gm_scene_addcam(&scene, ncam);

    gm_mesh chunk_mesh = build_chunk(display_chunk);
    gm_handle chunk_m = gm_scene_addmesh(&scene, chunk_mesh);
    gm_scene_addinst(&scene, chunk_m);
    gm_scene_addinst_init(&scene, chunk_m, (gm_transform){
        NULL, {.kind = GM_HANDLE_NULL},
        (gm_vec3){16, 0, 0}, GM_VEC3_NULL, GM_VEC3_NULL
    });
    gm_scene_addinst_init(&scene, chunk_m, (gm_transform){
        NULL, {.kind = GM_HANDLE_NULL},
        (gm_vec3){0, 0, 16}, GM_VEC3_NULL, GM_VEC3_NULL
    });
    gm_scene_addinst_init(&scene, chunk_m, (gm_transform){
        NULL, {.kind = GM_HANDLE_NULL},
        (gm_vec3){16, 0, 16}, GM_VEC3_NULL, GM_VEC3_NULL
    });
    gm_scene_addinst_init(&scene, chunk_m, (gm_transform){
        NULL, {.kind = GM_HANDLE_NULL},
        (gm_vec3){-16, 0, 0}, GM_VEC3_NULL, GM_VEC3_NULL
    });
    gm_scene_addinst_init(&scene, chunk_m, (gm_transform){
        NULL, {.kind = GM_HANDLE_NULL},
        (gm_vec3){0, 0, -16}, GM_VEC3_NULL, GM_VEC3_NULL
    });
    gm_scene_addinst_init(&scene, chunk_m, (gm_transform){
        NULL, {.kind = GM_HANDLE_NULL},
        (gm_vec3){-16, 0, -16}, GM_VEC3_NULL, GM_VEC3_NULL
    });
    gm_scene_addinst_init(&scene, chunk_m, (gm_transform){
        NULL, {.kind = GM_HANDLE_NULL},
        (gm_vec3){16, 0, -16}, GM_VEC3_NULL, GM_VEC3_NULL
    });
    gm_scene_addinst_init(&scene, chunk_m, (gm_transform){
        NULL, {.kind = GM_HANDLE_NULL},
        (gm_vec3){-16, 0, 16}, GM_VEC3_NULL, GM_VEC3_NULL
    });

    while (!WindowShouldClose()) {
        float delta_time = GetFrameTime();

        gm_vec3 move = GM_VEC3_NULL;
        float movex1 = IsKeyDown(KEY_D) - IsKeyDown(KEY_A);
        move.x += delta_time * 15.0f * movex1;
        float movey1 = IsKeyDown(KEY_UP) - IsKeyDown(KEY_DOWN);
        move.y += delta_time * 15.0f * movey1;
        float movez1 = IsKeyDown(KEY_W) - IsKeyDown(KEY_S);
        move.z += delta_time * 15.0f * movez1;

        float roty1 = IsKeyDown(KEY_RIGHT) - IsKeyDown(KEY_LEFT);

        gm_camera *main_cam = gm_scene_peekcam(&scene, cam);
        main_cam->transform.rotation.y += delta_time * 100.0f * roty1;
        gm_transform_move(&main_cam->transform, move);

        if (IsKeyPressed(KEY_ONE))
            main_cam->passes ^= GM_PASS_OPAQUE;
        if (IsKeyPressed(KEY_TWO))
            main_cam->passes ^= GM_PASS_TRANSPARENT;
        if (IsKeyPressed(KEY_THREE))
            main_cam->passes ^= GM_PASS_WIREFRAME;

        memset(passes, 0, sizeof(passes));
        memcpy(passes, "PASS: ", 6);
        if (main_cam->passes & GM_PASS_OPAQUE)
            strcat(passes, " OPAQUE");
        if (main_cam->passes & GM_PASS_TRANSPARENT)
            strcat(passes, " TRANS");
        if (main_cam->passes & GM_PASS_WIREFRAME)
            strcat(passes, " WIRE");

        gm_renderinfo info = {0};
        gm_scene_render_debug(&scene, &info);
        memset(debug_text, 0, sizeof(debug_text));
        snprintf(debug_text, sizeof(debug_text), "Tri: %lu\nVert: %lu\nTrans: %lu", 
            info.tris_accum, info.verts_accum, info.transparent_accum
        );

        gm_draw_cam(&FB, main_cam, (gm_vec2u){0, 0});

        float dt = GetFrameTime();
        if (fps_index == FPS_SAMPLE - 1) {
            memmove(fps_sample, fps_sample + 1, sizeof(uint32_t) * (FPS_SAMPLE - 1));
            fps_sample[fps_index] = (uint32_t)roundf(1.0f / dt);
        } else fps_sample[fps_index] = (uint32_t)roundf(1.0f / dt);

        uint32_t total = 0;
        for (uint32_t i = 0; i < fps_index; ++i)
            total += fps_sample[i];
        snprintf(fps_text, sizeof(fps_text), "%u", total / fps_index);
        if (fps_index != FPS_SAMPLE - 1) ++fps_index;

        draw_framebuffer();
    }

    return 0;
}
