#include "gamma/render.h"
#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>

gm_framebuffer gm_framebuffer_alloc(gm_vec2u size, gm_color fill) {
    size_t rawsize = size.x * size.y;
    gm_framebuffer fb = { malloc(sizeof(gm_pcolor) * rawsize), size,  rawsize};

    if (fill.r == fill.g && fill.g == fill.b && fill.b == fill.a)
        memset(fb.pixels, fill.r, sizeof(gm_pcolor) * rawsize);
    else {
        gm_pcolor pixel = gm_to_pcolor(fill);
        for (size_t i = 0; i < fb.rawsize; ++i)
            fb.pixels[i] = pixel;
    }

    return fb;
}

static inline float gm_fov_to_focal(uint32_t screen_width, float fov_deg) {
    // f =    W / 2
    //     tan(fov / 2)
    float half = fov_deg * (float)M_PI / 360.0f;
    return ((float)screen_width / 2.0f) / tanf(half);
}

gm_camera gm_camera_perspective(gm_vec2u view, gm_color fill, float fov, float near, float far) {
    assert(fov > 0.0f && fov < 180.0f);
    assert(near > 0.0f && far > near);

    return (gm_camera) {
        GM_CAMERA_PERSPECTIVE,
        GM_TRANSFORM_NULL,
        view,
        .perspective = {
            gm_fov_to_focal(view.x, fov),
            near, far,
        },
        .fb = gm_framebuffer_alloc(view, fill),
        .db = calloc(view.x * view.y, sizeof(float)),

        .oit = true,
        .oit_buf = {
            calloc(view.x * view.y, sizeof(gm_vec4)),
            calloc(view.x * view.y, sizeof(float)),
        }
    };
}

gm_vec3 gm_camera_tvec(gm_vec3 point, gm_camera *cam) {
    point.x -= cam->transform.position.x;
    point.y -= cam->transform.position.y;
    point.z -= cam->transform.position.z;

    point = gm_vec3_rotz(point, -cam->transform.rotation.z);
    point = gm_vec3_roty(point, -cam->transform.rotation.y);
    point = gm_vec3_rotx(point, -cam->transform.rotation.x);

    return point;
}

gm_vec3 gm_camera_pvec(gm_vec3 cpoint, gm_camera *cam) {
    return (gm_vec3) {
        (float)cam->view.x * 0.5f
           + cpoint.x * cam->perspective.focal / cpoint.z,
        (float)cam->view.y * 0.5f
           - cpoint.y * cam->perspective.focal / cpoint.z,
        cpoint.z,
    };
}

gm_tri3 gm_camera_ctri(gm_tri3 tri, gm_camera *cam) {
    tri.a = gm_camera_tvec(tri.a, cam);
    tri.b = gm_camera_tvec(tri.b, cam);
    tri.c = gm_camera_tvec(tri.c, cam);

    return tri;
}

gm_tri3 gm_camera_ptri(gm_tri3 ctri, gm_camera *cam) {
    ctri.a = gm_camera_pvec(ctri.a, cam);
    ctri.b = gm_camera_pvec(ctri.b, cam);
    ctri.c = gm_camera_pvec(ctri.c, cam);

    return ctri;
}

static inline float gm_edge(gm_vec3 a, gm_vec3 b, float x, float y) {
    return (x - a.x) * (b.y - a.y)
        - (y - a.y) * (b.x - a.x);
}
static inline bool gm_topleft(gm_vec3 a, gm_vec3 b, float sign) {
    float nx = (b.y - a.y) * sign;
    float ny = -(b.x - a.x) * sign;
    return nx > 0.0f || (nx == 0.0f && ny > 0.0f);
}

static inline gm_vec3 gm_clip_lerp_z(gm_vec3 a, gm_vec3 b, float z) {
    float t = (z - a.z) / (b.z - a.z);
    return (gm_vec3) {
        a.x + (b.x - a.x) * t,
        a.y + (b.y - a.y) * t,
        z,
    };
}

// clips a camera-space triangle against z = near, returns triangle count (0-2)
static int gm_clip_tri_near(gm_tri3 tri, float near, gm_tri3 out[2]) {
    gm_vec3 v[3] = { tri.a, tri.b, tri.c };
    gm_vec3 poly[4];
    int pc = 0;

    for (int i = 0; i < 3; ++i) {
        gm_vec3 cur = v[i];
        gm_vec3 nxt = v[(i + 1) % 3];
        bool cin = cur.z >= near;
        bool nin = nxt.z >= near;

        if (cin)
            poly[pc++] = cur;
        if (cin != nin)
            poly[pc++] = gm_clip_lerp_z(cur, nxt, near);
    }

    if (pc < 3)
        return 0;

    out[0] = (gm_tri3) { poly[0], poly[1], poly[2], tri.ac, tri.bc, tri.cc };
    if (pc == 4)
        out[1] = (gm_tri3) { poly[0], poly[2], poly[3], tri.ac, tri.bc, tri.cc };

    return pc - 2;
}

void gm_render_tri3(
    gm_camera *cam,
    gm_tri3 stri,
    bool trans
) {
    float area;
    if (cam == NULL || cam->fb.pixels == NULL ||
        (area = gm_edge(stri.a, stri.b, stri.c.x, stri.c.y)) == 0.0f)
        return;
    if (stri.ac.a == 0 ||
        (cam->oit && ((!trans && stri.ac.a != 255) || (trans && stri.ac.a == 255))))
        return;

    float alpha = (float)stri.ac.a / 255.0f;
    uint32_t min_x = (uint32_t)fmaxf(
        0.0f,
        floorf(fminf(stri.a.x, fminf(stri.b.x, stri.c.x)))
    );
    uint32_t min_y = (uint32_t)fmaxf(
        0.0f,
        floorf(fminf(stri.a.y, fminf(stri.b.y, stri.c.y)))
    );
    uint32_t max_x = (uint32_t)fminf(
        (float)cam->fb.size.x,
        ceilf(fmaxf(stri.a.x, fmaxf(stri.b.x, stri.c.x)))
    );
    uint32_t max_y = (uint32_t)fminf(
        (float)cam->fb.size.y,
        ceilf(fmaxf(stri.a.y, fmaxf(stri.b.y, stri.c.y)))
    );

    gm_pcolor pcolor = gm_to_pcolor(stri.ac);
    float sign = area > 0.0f ? 1.0f : -1.0f;
    bool tl_ab = gm_topleft(stri.a, stri.b, sign);
    bool tl_bc = gm_topleft(stri.b, stri.c, sign);
    bool tl_ca = gm_topleft(stri.c, stri.a, sign);

    for (uint32_t y = min_y; y < max_y; ++y) {
        for (uint32_t x = min_x; x < max_x; ++x) {
            float px = (float)x + 0.5f;
            float py = (float)y + 0.5f;

            float w_ab = gm_edge(stri.a, stri.b, px, py) * sign;
            float w_bc = gm_edge(stri.b, stri.c, px, py) * sign;
            float w_ca = gm_edge(stri.c, stri.a, px, py) * sign;

            if (
                (w_ab > 0.0f || (w_ab == 0.0f && tl_ab)) &&
                (w_bc > 0.0f || (w_bc == 0.0f && tl_bc)) &&
                (w_ca > 0.0f || (w_ca == 0.0f && tl_ca))
            ) {
                size_t index =
                    (size_t)y * (size_t)cam->fb.size.x + (size_t)x;

                float inv_area = 1.0f / fabsf(area);
                float inv_z =
                    w_bc * inv_area / stri.a.z +
                    w_ca * inv_area / stri.b.z +
                    w_ab * inv_area / stri.c.z;

                // check if new depth beats out already drawn depth
                if (inv_z > 0.0f && inv_z > cam->db[index]) {
                    if (!trans || !cam->oit) {
                        cam->db[index] = inv_z;
                        cam->fb.pixels[index] = pcolor;
                    } else {
                        float z = 1.0f / inv_z;
                        float w = alpha * fmaxf(
                            1e-2f,
                            fminf(
                                3e3f,
                                10.0f / (1e-5f + powf(z / 5.0f, 2.0f) + powf(z / 200.0f, 6.0f))
                            )
                        );

                        gm_vec4 *acc = cam->oit_buf.accum + index;
                        acc->r += (float)stri.ac.r * alpha * w;
                        acc->g += (float)stri.ac.g * alpha * w;
                        acc->b += (float)stri.ac.b * alpha * w;
                        acc->a += alpha * w;

                        cam->oit_buf.revealance[index] *= 1.0f - alpha;
                    }
                }
            }
        }
    }
}

void gm_draw_cam(gm_framebuffer *dest, gm_camera *cam, gm_vec2u position) {
    if (position.x >= dest->size.x || position.y >= dest->size.y)
        return;
    uint32_t wtodraw = GM_MIN(cam->fb.size.x, dest->size.x - position.x);

    uint32_t i = position.y * dest->size.x + position.x;
    uint32_t line = 0;
    while (i < dest->rawsize && line < cam->fb.size.y) {
        memcpy(
            dest->pixels + i,
            cam->fb.pixels + line * cam->fb.size.x,
            GM_MIN(dest->rawsize - i, wtodraw) * sizeof(gm_pcolor)
        );
        i += dest->size.x;
        ++line;
    }
}


void gm_render_mesh(gm_camera *cam, gm_mesh *mesh, gm_transform *transform, bool trans) {
    for (gm_tri3 *tri = mesh->tris; tri < mesh->tris + mesh->tri_c; ++tri) {
        gm_tri3 rotated = {
            gm_transform_point(tri->a, transform),
            gm_transform_point(tri->b, transform),
            gm_transform_point(tri->c, transform),
            tri->ac, tri->bc, tri->cc
        };

        gm_tri3 screenspace = gm_camera_ctri(rotated, cam);

        // Clip to near plane
        gm_tri3 clipped[2];
        int n = gm_clip_tri_near(screenspace, cam->perspective.near, clipped);
        for (int i = 0; i < n; ++i)
            gm_render_tri3(cam, gm_camera_ptri(clipped[i], cam), trans);
    }
}

void gm_render_composite(gm_camera *cam) {
    if (!cam->oit) return;

    for (size_t i = 0; i < cam->fb.rawsize; ++i) {
        float reveal = cam->oit_buf.revealance[i];
        if (reveal >= 1.0f)
            continue;

        gm_vec4 *acc = cam->oit_buf.accum + i;
        float inv_w = 1.0f / fmaxf(acc->a, 1e-5f);
        float cover = 1.0f - reveal;

        gm_color dst = gm_to_rgba(cam->fb.pixels[i]);
        cam->fb.pixels[i] = gm_rgba(
            (uint8_t)fminf(255.0f, acc->r * inv_w * cover + dst.r * reveal),
            (uint8_t)fminf(255.0f, acc->g * inv_w * cover + dst.g * reveal),
            (uint8_t)fminf(255.0f, acc->b * inv_w * cover + dst.b * reveal),
            255
        );
    }
}
