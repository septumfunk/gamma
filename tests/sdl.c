#include "gamma/render.h"
#include <SDL2/SDL.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static inline SDL_Texture *fb_texture(SDL_Renderer *ren, gm_framebuffer *fb) {
    SDL_Texture *tex = SDL_CreateTexture(
        ren,
        SDL_PIXELFORMAT_RGBA32,
        SDL_TEXTUREACCESS_STREAMING,
        (int)fb->size.x,
        (int)fb->size.y
    );
    SDL_SetTextureScaleMode(tex, SDL_ScaleModeNearest);

    return tex;
}

static inline void fb_draw(gm_framebuffer *fb, SDL_Texture *tex, SDL_Renderer *ren) {
    SDL_UpdateTexture(
        tex,
        NULL,
        fb->pixels,
        (int)(fb->size.x * sizeof(gm_pcolor))
    );

    SDL_RenderClear(ren);
    SDL_RenderCopy(ren, tex, NULL, NULL);
    SDL_RenderPresent(ren);
}

int sdl(void) {
    gm_camera cam = gm_camera_perspective((gm_vec2u){320, 240}, (gm_color){0, 0, 0, 0}, 90.0f, 0.1f, 10.0f);
    cam.transform.position.z = -10.0f;

    if (SDL_Init(SDL_INIT_VIDEO) != 0)
        return 1;

    SDL_Window *window = SDL_CreateWindow(
        "Gamma Test",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        (int)cam.view.x,
        (int)cam.view.y,
        0
    );
    if (window == NULL) {
        SDL_Quit();
        return 1;
    }

    SDL_SetHint(SDL_HINT_RENDER_DRIVER, "opengl");
    SDL_Renderer *renderer = SDL_CreateRenderer(
        window, -1, SDL_RENDERER_ACCELERATED
    );
    if (renderer == NULL) {
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    SDL_Texture *tex = fb_texture(renderer, &cam.fb);
    if (tex == NULL) {
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    gm_tri3 cube[] = {
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
    (void)cube;

    gm_transform ctrans = GM_TRANSFORM_NULL;

    bool running = true;
    SDL_Event event;

    double fps_buf[10] = {0};
    const size_t fps_size = sizeof(fps_buf) / sizeof(*fps_buf);

    uint64_t now = SDL_GetPerformanceCounter();
    uint64_t last = 0;
    while (running) {
        last = now;
        now = SDL_GetPerformanceCounter();
        double delta_time = (double)(now - last) / (double)SDL_GetPerformanceFrequency();

        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_QUIT:
                    running = false;
                    break;
                case SDL_KEYDOWN:
                    if (event.key.keysym.sym == SDLK_ESCAPE)
                        running = false;
                    break;
            }
            if (!running) break;
        }
        if (!running) break;
        const Uint8 *keys = SDL_GetKeyboardState(NULL);

        float mod = keys[SDL_SCANCODE_LSHIFT] ? 0.3f : 0.1f;

        if (keys[SDL_SCANCODE_RIGHT])
            ctrans.rotation.y += mod * 0.75f;
        else if (keys[SDL_SCANCODE_LEFT])
            ctrans.rotation.y -= mod * 0.75f;
        if (keys[SDL_SCANCODE_UP])
            ctrans.rotation.x += mod * 0.75f;
        else if (keys[SDL_SCANCODE_DOWN])
            ctrans.rotation.x -= mod * 0.75f;

        if (keys[SDL_SCANCODE_L])
            cam.transform.rotation.y += mod * 0.25f;
        else if (keys[SDL_SCANCODE_J])
            cam.transform.rotation.y -= mod * 0.25f;
        if (keys[SDL_SCANCODE_K])
            cam.transform.rotation.x = GM_MIN(cam.transform.rotation.x + mod * 0.25f, 180.0f);
        else if (keys[SDL_SCANCODE_I])
            cam.transform.rotation.x = GM_MAX(cam.transform.rotation.x - mod * 0.25f, -180.0f);

        if (keys[SDL_SCANCODE_W])
            cam.transform.position.z += mod * 0.5f;
        else if (keys[SDL_SCANCODE_S])
            cam.transform.position.z -= mod * 0.5f;
        if (keys[SDL_SCANCODE_D])
            cam.transform.position.x += mod * 0.5f;
        else if (keys[SDL_SCANCODE_A])
            cam.transform.position.x -= mod * 0.5f;

        if (keys[SDL_SCANCODE_E])
            cam.transform.position.y += mod * 0.25f;
        else if (keys[SDL_SCANCODE_Q])
            cam.transform.position.y -= mod * 0.25f;
        
        fb_draw(&cam.fb, tex, renderer);

        double total = 0.0f;
        memcpy(fps_buf, fps_buf + 1, (fps_size - 1) * sizeof(*fps_buf));
        fps_buf[fps_size - 1] =  1.0 / delta_time;

        for (size_t i = 0; i < fps_size; ++i)
            total += fps_buf[i];
        printf("Avg FPS: %.2f\n", total / fps_size);
    }

    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}