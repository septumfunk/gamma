#ifndef RENDER_H
#define RENDER_H

#include "coord.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    gm_vec2 a;
    gm_vec2 b;
    gm_vec2 c;
} gm_tri2;

static inline gm_vec2 gm_tri2_center(gm_tri2 tri) {
    return (gm_vec2){
        (tri.a.x + tri.b.x + tri.c.x) / 3.0f,
        (tri.a.y + tri.b.y + tri.c.y) / 3.0f
    };
}
static inline gm_tri2 gm_tri2_rotate(gm_tri2 tri, float angle) {
    gm_vec2 center = gm_tri2_center(tri);

    tri.a = gm_vec2_rotate_around(tri.a, center, angle);
    tri.b = gm_vec2_rotate_around(tri.b, center, angle);
    tri.c = gm_vec2_rotate_around(tri.c, center, angle);

    return tri;
}

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
} gm_color;
typedef uint32_t gm_pcolor; // pixel color

typedef struct {
    gm_vec3 a, b, c;
    gm_color ac, bc, cc;
} gm_tri3;

#define GM_BLACK ((gm_color){0, 0, 0, 255})
#define GM_RED   ((gm_color){255, 0, 0, 255})
#define GM_GREEN ((gm_color){0, 255, 0, 255})
#define GM_BLUE  ((gm_color){0, 0, 255, 255})

#define GM_DARKBLUE  ((gm_color){75, 75, 100, 255})
#define GM_DARKRED   ((gm_color){100, 75, 75, 255})

#define GM_LIGHTRED   ((gm_color){255, 125, 125, 255})
#define GM_LIGHTGREEN ((gm_color){125, 255, 125, 255})
#define GM_LIGHTBLUE  ((gm_color){125, 125, 255, 255})
#define GM_LIGHTORANGE  ((gm_color){255, 200, 140, 255})


static inline gm_pcolor gm_rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    return ((uint32_t)a << 24)
        | ((uint32_t)b << 16)
        | ((uint32_t)g << 8)
        | r;
}
static inline gm_pcolor gm_to_pcolor(gm_color color) {
    return gm_rgba(color.r, color.g, color.b, color.a);
}
static inline gm_color gm_to_rgba(gm_pcolor pcolor) {
    return (gm_color) {
        pcolor & 0xff,
        (pcolor >> 8) & 0xff,
        (pcolor >> 16) & 0xff,
        0
    };
}

typedef struct {
    gm_pcolor *pixels;
    gm_vec2u size;
    size_t rawsize;
} gm_framebuffer;

gm_framebuffer gm_framebuffer_alloc(gm_vec2u size, gm_color fill);
static inline void gm_framebuffer_free(gm_framebuffer *fb) {
    free(fb->pixels);
    fb->pixels = NULL;
    fb->size = GM_VEC2U_NULL;
}

static inline void gm_framebuffer_clear(gm_framebuffer *fb, gm_color fill) {
    gm_pcolor pixel = gm_to_pcolor(fill);
    for (size_t i = 0; i < fb->rawsize; ++i)
        fb->pixels[i] = pixel;
}

typedef struct {
    gm_vec4 *accum;
    float *revealance;
} gm_oitbuf;

typedef enum gm_type {
    GM_CAMERA_PERSPECTIVE,
} gm_camtype;
typedef struct {
    gm_camtype type;
    gm_transform transform;
    gm_vec2u view;

    union {
        struct {
            float focal;
            float near, far;
        } perspective;
    };

    gm_framebuffer fb;
    float *db;
    bool oit;
    gm_oitbuf oit_buf;
} gm_camera;

gm_camera gm_camera_perspective(gm_vec2u view, gm_color fill, float fov, float near, float far);
static inline void gm_camera_oit(gm_camera *cam, bool toggle) {
    if (toggle != cam->oit) {
        cam->oit = toggle;
        if (toggle) {
            cam->oit_buf.accum = calloc(cam->view.x * cam->view.y, sizeof(gm_vec4));
            cam->oit_buf.revealance = calloc(cam->view.x * cam->view.y, sizeof(float));
        } else {
            free(cam->oit_buf.accum);
            free(cam->oit_buf.revealance);
            cam->oit_buf.accum = NULL;
            cam->oit_buf.revealance = NULL;
        }
    }
}

gm_vec3 gm_camera_tvec(gm_vec3 point, gm_camera *cam);
gm_vec3 gm_camera_pvec(gm_vec3 cpoint, gm_camera *cam);

gm_tri3 gm_camera_ctri(gm_tri3 tri, gm_camera *cam);
gm_tri3 gm_camera_ptri(gm_tri3 ctri, gm_camera *cam);

static inline void gm_camera_clear(gm_camera *cam, gm_color fill) {
    gm_framebuffer_clear(&cam->fb, fill);
    memset(cam->db, 0.0f, cam->view.x * cam->view.y * sizeof(float));
    if (cam->oit) {
        memset(cam->oit_buf.accum, 0, sizeof(gm_vec4) * cam->fb.rawsize);
        for (size_t i = 0; i < cam->fb.rawsize; ++i)
            cam->oit_buf.revealance[i] = 1.0f;
    }
}

void gm_render_tri3(
    gm_camera *cam,
    gm_tri3 tri,
    bool trans
);

void gm_draw_cam(gm_framebuffer *dest, gm_camera *cam, gm_vec2u position);

typedef struct {
    gm_tri3 *tris;
    size_t tri_c;
} gm_mesh;

static inline gm_mesh gm_mesh_new(size_t tri_c, gm_tri3 tris[tri_c]) {
    gm_tri3 *owned = malloc(sizeof(gm_tri3) * tri_c);
    memcpy(owned, tris, sizeof(gm_tri3) * tri_c);
    return (gm_mesh) {
        owned,
        tri_c,
    };
}

void gm_render_mesh(
    gm_camera *cam,
    gm_mesh *mesh,
    gm_transform *transform,
    bool trans
);

void gm_render_composite(gm_camera *cam);

#endif // RENDER_H
