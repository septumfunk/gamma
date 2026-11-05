#include "gamma/render.h"
#include <raylib.h>

#define FPS    0
#define WIDTH  4
#define HEIGHT 8
#define SCALE 16

static gm_framebuffer FB;
static Texture2D FB_TEX;
void init_framebuffer(gm_vec2u size, gm_color fill) {
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

void draw_framebuffer(void) {
    UpdateTexture(FB_TEX, FB.pixels);

    BeginDrawing();
    ClearBackground(BLACK);
    DrawTexturePro(FB_TEX, (Rectangle){0, 0, WIDTH, HEIGHT}, (Rectangle){0, 0, WIDTH * SCALE, HEIGHT * SCALE}, (Vector2){0, 0}, 0, WHITE);
    DrawFPS(10, 10);
    EndDrawing();
}

int raylib(void) {
    init_framebuffer((gm_vec2u){WIDTH, HEIGHT}, GM_BLACK);
    gm_camera main_cam = gm_camera_perspective((gm_vec2u){WIDTH, HEIGHT / 2}, GM_DARKBLUE, 90.0f, 0.1f, 10.0f);
    gm_camera second_cam = gm_camera_perspective((gm_vec2u){WIDTH, HEIGHT / 2}, GM_DARKRED, 90.0f, 0.1f, 10.0f);

    main_cam.transform.position.z = -5;
    second_cam.transform.position.z = 5;
    second_cam.transform.rotation.y = 180.0f;

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

    gm_mesh cube1 = gm_mesh_new(12, tris);
    cube1.transform.parent = &main_cam.transform;

    gm_mesh cube2 = gm_mesh_new(12, tris);
    cube2.transform.rotation.y = 90;
    cube2.transform.parent = &second_cam.transform;

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
        main_cam.transform.rotation.y += delta_time * 100.0f * roty1;

        gm_transform_move(&main_cam.transform, move);


        gm_camera_clear(&main_cam, GM_DARKBLUE);
        gm_render_mesh(&main_cam, &cube2);

        gm_camera_clear(&second_cam, GM_DARKRED);
        gm_render_mesh(&second_cam, &cube1);

        gm_draw_cam(&FB, &main_cam, (gm_vec2u){0, 0});
        gm_draw_cam(&FB, &second_cam, (gm_vec2u){0, HEIGHT / 2});

        draw_framebuffer();
    }

    return 0;
}
