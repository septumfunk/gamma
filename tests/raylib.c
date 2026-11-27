#include "gamma/coord.h"
#include "gamma/render.h"
#include "gamma/scene.h"
#include <raylib.h>
#include <stdio.h>
#include <string.h>

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

static void init_framebuffer(gm_vec2u size, gm_color fill) {
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
    EndDrawing();
}

int raylib(void) {
    init_framebuffer((gm_vec2u){WIDTH, HEIGHT}, GM_BLACK);
    gm_camera main_cam = gm_camera_perspective((gm_vec2u){WIDTH, HEIGHT / 2}, GM_DARKBLUE, 90.0f, 0.1f, 10.0f);
    gm_camera second_cam = gm_camera_perspective((gm_vec2u){WIDTH, HEIGHT / 2}, GM_DARKRED, 90.0f, 0.1f, 10.0f);
    second_cam.passes = GM_PASS_WIREFRAME;

    main_cam.transform.position.z = -5;
    second_cam.transform.position.z = 5;
    second_cam.transform.rotation.y = 180.0f;

    gm_scene scene = gm_scene_new(GM_DARKBLUE);
    gm_handle cam1 = gm_scene_addcam(&scene, main_cam);
    gm_handle cam2 = gm_scene_addcam(&scene, second_cam);

    gm_tri3 tris[] = {
        // Back face
        {{-1, -1, -1}, { 1,  1, -1}, { 1, -1, -1},
            GM_LIGHTBLUE, GM_LIGHTBLUE, GM_LIGHTBLUE},
        {{-1, -1, -1}, {-1,  1, -1}, { 1,  1, -1},
            GM_LIGHTBLUE, GM_LIGHTBLUE, GM_LIGHTBLUE},

        // Front face
        {{-1, -1,  1}, { 1, -1,  1}, { 1,  1,  1},
            GM_LIGHTRED, GM_LIGHTRED, GM_LIGHTRED},
        {{-1, -1,  1}, { 1,  1,  1}, {-1,  1,  1},
            GM_LIGHTRED, GM_LIGHTRED, GM_LIGHTRED},

        // Bottom face
        {{-1, -1, -1}, { 1, -1, -1}, { 1, -1,  1},
            GM_BLACK, GM_BLACK, GM_BLACK},
        {{-1, -1, -1}, { 1, -1,  1}, {-1, -1,  1},
            GM_BLACK, GM_BLACK, GM_BLACK},

        // Top face
        {{-1,  1, -1}, {-1,  1,  1}, { 1,  1,  1},
            GM_RED, GM_RED, GM_RED},
        {{-1,  1, -1}, { 1,  1,  1}, { 1,  1, -1},
            GM_RED, GM_RED, GM_RED},

        // Left face
        {{-1, -1, -1}, {-1, -1,  1}, {-1,  1,  1},
            GM_LIGHTGREEN, GM_LIGHTGREEN, GM_LIGHTGREEN},
        {{-1, -1, -1}, {-1,  1,  1}, {-1,  1, -1},
            GM_LIGHTGREEN, GM_LIGHTGREEN, GM_LIGHTGREEN},

        // Right face
        {{ 1, -1, -1}, { 1,  1, -1}, { 1,  1,  1},
            GM_LIGHTORANGE, GM_LIGHTORANGE, GM_LIGHTORANGE},
        {{ 1, -1, -1}, { 1,  1,  1}, { 1, -1,  1},
            GM_LIGHTORANGE, GM_LIGHTORANGE, GM_LIGHTORANGE},
    };

    for (size_t i = 0; i < sizeof(tris) / sizeof(gm_tri3); ++i)
        tris[i].ac.a = 100;
    gm_handle cube_m = gm_scene_addmesh(&scene, gm_mesh_new(12, tris));

    for (size_t i = 0; i < sizeof(tris) / sizeof(gm_tri3); ++i) {
        tris[i].ac.a = 255;
        tris[i].ac.b = (uint8_t)GM_MIN((int)tris[i].ac.g + 25, 255);

        tris[i].a.x *= 2;
        tris[i].a.y *= 2;
        tris[i].a.z *= 3;
        tris[i].b.x *= 2;
        tris[i].b.y *= 2;
        tris[i].b.z *= 3;
        tris[i].c.x *= 2;
        tris[i].c.y *= 2;
        tris[i].c.z *= 3;
    }
    gm_handle cube_l = gm_scene_addmesh(&scene, gm_mesh_new(12, tris));

    gm_handle cube1 = gm_scene_addinst_init(
        &scene, cube_m,
        (gm_transform) {
            NULL,
            cam1,
            GM_VEC3_NULL,
            GM_VEC3_NULL, GM_VEC3_NULL,
        }
    );
    gm_handle cube2 = gm_scene_addinst_init(
        &scene, cube_m,
        (gm_transform) {
            NULL,
            cam2,
            GM_VEC3_NULL,
            (gm_vec3){0, 90, 0}, GM_VEC3_NULL,
        }
    );
    gm_scene_addinst_init(
        &scene, cube_l,
        (gm_transform) {
            NULL,
            cam2,
            (gm_vec3){0, 0, -10},
            (gm_vec3){0, 90, 0}, GM_VEC3_NULL,
        }
    );

    gm_scene_hideinst(&scene, cube1, cam1);
    gm_scene_hideinst(&scene, cube2, cam2);

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

        gm_camera *main_cam = gm_scene_peekcam(&scene, cam1);
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

        gm_scene_render(&scene);

        gm_draw_cam(&FB, main_cam, (gm_vec2u){0, 0});
        gm_draw_cam(
            &FB,
            gm_scene_peekcam(&scene, cam2),
            (gm_vec2u){0, HEIGHT / 2}
        );

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
