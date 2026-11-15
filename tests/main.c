#include "gamma/render.h"

#include <stdint.h>
#include <string.h>
#include <allegro.h>

#define WIDTH       320
#define HEIGHT      240
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
     * Convert to the screen depth and double each pixel.
     */
    const uint32_t *src = (const uint32_t *)FB.pixels;
    const unsigned row_bytes = WINDOW_W * ((gfx_depth + 7) / 8);

    for (unsigned y = 0; y < HEIGHT; ++y) {
        uint32_t *dst = (uint32_t *)FB_BMP->line[y * 2];

        if (gfx_depth == 32) {
            for (unsigned x = 0; x < WIDTH; ++x) {
                uint32_t p = *src++;

                uint32_t c =
                    rtab[p & 0xFF] |
                    gtab[(p >> 8) & 0xFF] |
                    btab[(p >> 16) & 0xFF];

                dst[x * 2] = c;
                dst[x * 2 + 1] = c;
            }
        } else {
            for (unsigned x = 0; x < WIDTH; ++x) {
                uint32_t p = *src++;

                uint32_t c =
                    rtab[p & 0xFF] |
                    gtab[(p >> 8) & 0xFF] |
                    btab[(p >> 16) & 0xFF];

                dst[x] = c | (c << 16);
            }
        }

        memcpy(FB_BMP->line[y * 2 + 1], dst, row_bytes);
    }

    vsync();

    blit(FB_BMP, screen, 0, 0, 0, 0, WINDOW_W, WINDOW_H);
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

    gm_camera main_cam = gm_camera_perspective(
        (gm_vec2u){WIDTH, HEIGHT / 2},
        GM_DARKBLUE, 90.0f, 0.1f, 10.0f
    );

    gm_camera second_cam = gm_camera_perspective(
        (gm_vec2u){WIDTH, HEIGHT / 2},
        GM_DARKRED, 90.0f, 0.1f, 10.0f
    );

    main_cam.transform.position.z = -5;

    second_cam.transform.position.z = 5;
    second_cam.transform.rotation.y = 180.0f;

    gm_tri3 tris[] = {
        // Back face
        {{-1,-1,-1}, { 1, 1,-1}, { 1,-1,-1},
            GM_LIGHTBLUE, GM_LIGHTBLUE, GM_LIGHTBLUE},
        {{-1,-1,-1}, {-1, 1,-1}, { 1, 1,-1},
            GM_LIGHTBLUE, GM_LIGHTBLUE, GM_LIGHTBLUE},

        // Front face
        {{-1,-1, 1}, { 1,-1, 1}, { 1, 1, 1},
            GM_LIGHTRED, GM_LIGHTRED, GM_LIGHTRED},
        {{-1,-1, 1}, { 1, 1, 1}, {-1, 1, 1},
            GM_LIGHTRED, GM_LIGHTRED, GM_LIGHTRED},

        // Bottom face
        {{-1,-1,-1}, { 1,-1,-1}, { 1,-1, 1},
            GM_BLACK, GM_BLACK, GM_BLACK},
        {{-1,-1,-1}, { 1,-1, 1}, {-1,-1, 1},
            GM_BLACK, GM_BLACK, GM_BLACK},

        // Top face
        {{-1, 1,-1}, {-1, 1, 1}, { 1, 1, 1},
            GM_RED, GM_RED, GM_RED},
        {{-1, 1,-1}, { 1, 1, 1}, { 1, 1,-1},
            GM_RED, GM_RED, GM_RED},

        // Left face
        {{-1,-1,-1}, {-1,-1, 1}, {-1, 1, 1},
            GM_LIGHTGREEN, GM_LIGHTGREEN, GM_LIGHTGREEN},
        {{-1,-1,-1}, {-1, 1, 1}, {-1, 1,-1},
            GM_LIGHTGREEN, GM_LIGHTGREEN, GM_LIGHTGREEN},

        // Right face
        {{ 1,-1,-1}, { 1, 1,-1}, { 1, 1, 1},
            GM_LIGHTORANGE, GM_LIGHTORANGE, GM_LIGHTORANGE},
        {{ 1,-1,-1}, { 1, 1, 1}, { 1,-1, 1},
            GM_LIGHTORANGE, GM_LIGHTORANGE, GM_LIGHTORANGE},
    };

    gm_mesh cube1 = gm_mesh_new(12, tris);
    cube1.transform.parent = &main_cam.transform;

    gm_mesh cube2 = gm_mesh_new(12, tris);
    cube2.transform.rotation.y = 90;
    cube2.transform.parent = &second_cam.transform;

    unsigned long previous_ticks = timer_ticks;

    while (!key[KEY_ESC]) {
        unsigned long current_ticks = timer_ticks;

        float delta_time =
            (float)(current_ticks - previous_ticks) / TIMER_HZ;

        if (delta_time > 0.1f)
            delta_time = 0.1f;

        previous_ticks = current_ticks;

        gm_vec3 move = GM_VEC3_NULL;

        float movex =
            (float)(key[KEY_A] - key[KEY_D]);

        float movey =
            (float)(key[KEY_DOWN] - key[KEY_UP]);

        float movez =
            (float)(key[KEY_S] - key[KEY_W]);

        move.x += delta_time * 15.0f * movex;
        move.y += delta_time * 15.0f * movey;
        move.z += delta_time * 15.0f * movez;

        float roty =
            (float)(key[KEY_LEFT] - key[KEY_RIGHT]);

        main_cam.transform.rotation.y +=
            delta_time * 100.0f * roty;

        if (main_cam.transform.rotation.y >= 360.0f)
            main_cam.transform.rotation.y -= 360.0f;
        else if (main_cam.transform.rotation.y < 0.0f)
            main_cam.transform.rotation.y += 360.0f;

        gm_transform_move(&main_cam.transform, move);

        gm_camera_clear(&main_cam, GM_DARKBLUE);
        gm_render_mesh(&main_cam, &cube2);

        gm_camera_clear(&second_cam, GM_DARKRED);
        gm_render_mesh(&second_cam, &cube1);

        gm_draw_cam(&FB, &main_cam, (gm_vec2u){0, 0});

        gm_draw_cam(
            &FB,
            &second_cam,
            (gm_vec2u){0, HEIGHT / 2}
        );

        draw_framebuffer();
    }

    destroy_bitmap(FB_BMP);

    remove_int(timer_tick);

    allegro_exit();

    return 0;
}

END_OF_MAIN()