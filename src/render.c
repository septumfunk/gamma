#include "gamma/render.h"
#include "gamma/coord.h"
#include <assert.h>
#include <math.h>
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

        .passes = GM_PASS_OPAQUE | GM_PASS_TRANSPARENT,
        .accum = calloc(view.x * view.y, sizeof(gm_vec4)),
        .revealance = calloc(view.x * view.y, sizeof(float)),
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
static inline float gm_edge_c(gm_vec3 a, gm_vec3 b, float x, float y) {
    if (a.y < b.y || (a.y == b.y && a.x < b.x))
        return -gm_edge(b, a, x, y);
    return gm_edge(a, b, x, y);
}
static inline bool gm_topleft(gm_vec3 a, gm_vec3 b, float sign) {
    float nx = (b.y - a.y) * sign;
    float ny = -(b.x - a.x) * sign;
    return nx > 0.0f || (nx == 0.0f && ny > 0.0f);
}

enum {
    GM_CLIP_NEAR = 1u << 0,
    GM_CLIP_FAR = 1u << 1,
    GM_CLIP_LEFT = 1u << 2,
    GM_CLIP_RIGHT = 1u << 3,
    GM_CLIP_BOTTOM = 1u << 4,
    GM_CLIP_TOP = 1u << 5,
};
typedef uint32_t gm_ocode;
#define GM_CLIP_MAX 9

typedef struct {
    float near, far;
    float kx, ky; // half width/height
} gm_frustum;

static inline gm_frustum gm_camera_frustum(gm_camera *cam) {
    return (gm_frustum) {
        cam->perspective.near,
        cam->perspective.far,
        (float)cam->view.x * 0.5f / cam->perspective.focal, // kx
        (float)cam->view.y * 0.5f / cam->perspective.focal, // ky
    };
}

static inline float gm_plane_distance(const gm_frustum *frust, gm_vec3 point, gm_ocode plane) {
    switch (plane) {
        case GM_CLIP_NEAR: return point.z - frust->near;
        case GM_CLIP_FAR: return frust->far - point.z;
        case GM_CLIP_LEFT: return point.z * frust->kx + point.x;
        case GM_CLIP_RIGHT: return point.z * frust->kx - point.x;
        case GM_CLIP_BOTTOM: return point.z * frust->ky + point.y;
        case GM_CLIP_TOP: return point.z * frust->ky - point.y;
        default: return 0.0f;
    };
}

static inline gm_vec3 gm_clip_lerp(gm_vec3 a, gm_vec3 b, float t) {
    return (gm_vec3) {
        a.x + (b.x - a.x) * t,
        a.y + (b.y - a.y) * t,
        a.z + (b.z - a.z) * t,
    };
}

static inline gm_ocode gm_outcode(const gm_frustum *frust, gm_vec3 point) {
    gm_ocode planes = 0;
    // Using bitwise to set planes as a 'flag'
    for (gm_ocode p = GM_CLIP_NEAR; p <= GM_CLIP_TOP; p <<= 1)
        if (gm_plane_distance(frust, point, p) < 0.0f)
            planes |= p;
    return planes;
}

// clips a camera-space polygon against given plane,
// returns modified vertex count
static uint32_t gm_clip_plane(
    const gm_vec3 *in, const bool *e_in, uint32_t count,
    gm_vec3 *out, bool *e_out, gm_ocode plane, const gm_frustum *frust
) {
    float dist[GM_CLIP_MAX];
    for (uint32_t i = 0; i < count; ++i)
        dist[i] = gm_plane_distance(frust, in[i], plane);

    uint32_t oc = 0;
    for (uint32_t i = 0; i < count; ++i) {
        uint32_t n = (i + 1) % count; // next
        bool cin = dist[i] >= 0.0f;
        bool nin = dist[n] >= 0.0f;

        if (cin) {
            e_out[oc] = e_in[i];
            out[oc++] = in[i];
        }
        if (cin != nin) {
            e_out[oc] = !cin && e_in[i];
            out[oc++] = cin ?
                gm_clip_lerp(in[i], in[n], dist[i] / (dist[i] - dist[n])) :
                gm_clip_lerp(in[n], in[i], dist[n] / (dist[n] - dist[i]));
        }
    }

    return oc;
}

static uint32_t gm_clip_tri(
    gm_tri3 tri, const gm_frustum *frust,
    gm_vec3 out[GM_CLIP_MAX], bool edge[GM_CLIP_MAX]
) {
    uint32_t o_a = gm_outcode(frust, tri.a);
    uint32_t o_b = gm_outcode(frust, tri.b);
    uint32_t o_c = gm_outcode(frust, tri.c);

    // all outside of one plane
    if (o_a & o_b & o_c)
        return 0;

    // default to in tri
    out[0] = tri.a;
    out[1] = tri.b;
    out[2] = tri.c;
    edge[0] = edge[1] = edge[2] = true;

    uint32_t mask = o_a | o_b | o_c;
    // all outside of no planes
    if (mask == 0)
        return 3;

    gm_vec3 tmp[GM_CLIP_MAX];
    bool e_tmp[GM_CLIP_MAX];
    gm_vec3 *src = out, *dst = tmp;
    bool *e_src = edge, *e_dst = e_tmp;
    uint32_t n = 3;

    for (gm_ocode p = GM_CLIP_NEAR; p <= GM_CLIP_TOP; p <<= 1) {
        if (!(mask & p)) // not outside
            continue;

        n = gm_clip_plane(src, e_src, n, dst, e_dst, p, frust);
        if (n < 3) // Out of view
            return 0;

        gm_vec3 *swap = src;
        src = dst;
        dst = swap;

        bool *e_swap = e_src;
        e_src = e_dst;
        e_dst = e_swap;
    }

    if (src != out) {
        memcpy(out, src, n * sizeof(gm_vec3));
        memcpy(edge, e_src, n * sizeof(bool));
    }

    return n;
}

void gm_render_tri3_debug(
    gm_camera *cam,
    gm_tri3 stri,
    gm_renderpass pass,
    gm_renderinfo *info
) {
    float area;
    if (cam == NULL || cam->fb.pixels == NULL ||
        (area = gm_edge(stri.a, stri.b, stri.c.x, stri.c.y)) == 0.0f
    ) return;
    if (stri.ac.a == 0 ||
        (pass == GM_PASS_OPAQUE && stri.ac.a != 255) ||
        (pass == GM_PASS_TRANSPARENT && stri.ac.a == 255)
    ) return;

    if (info) {
        ++info->tris_accum;
        info->verts_accum += 3;
    }

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
    float inv_area = 1.0f / fabsf(area);

    bool tl_ab = gm_topleft(stri.a, stri.b, sign);
    bool tl_bc = gm_topleft(stri.b, stri.c, sign);
    bool tl_ca = gm_topleft(stri.c, stri.a, sign);

    for (uint32_t y = min_y; y < max_y; ++y) {
        for (uint32_t x = min_x; x < max_x; ++x) {
            float px = (float)x + 0.5f;
            float py = (float)y + 0.5f;

            float w_ab = gm_edge_c(stri.a, stri.b, px, py) * sign;
            float w_bc = gm_edge_c(stri.b, stri.c, px, py) * sign;
            float w_ca = gm_edge_c(stri.c, stri.a, px, py) * sign;

            if (
                (w_ab > 0.0f || (w_ab == 0.0f && tl_ab)) &&
                (w_bc > 0.0f || (w_bc == 0.0f && tl_bc)) &&
                (w_ca > 0.0f || (w_ca == 0.0f && tl_ca))
            ) {
                size_t index =
                    (size_t)y * (size_t)cam->fb.size.x + (size_t)x;

                float inv_z =
                    w_bc * inv_area / stri.a.z +
                    w_ca * inv_area / stri.b.z +
                    w_ab * inv_area / stri.c.z;

                // check if new depth beats out already drawn depth
                if (inv_z > 0.0f && inv_z > cam->db[index]) {
                    switch (pass) {
                        case GM_PASS_OPAQUE:
                        case GM_PASS_WIREFRAME: {
                            cam->db[index] = inv_z;
                            cam->fb.pixels[index] = pcolor;
                            break;
                        }
                        case GM_PASS_TRANSPARENT: {
                            float z = 1.0f / inv_z;
                            float w = alpha * fmaxf(
                                1e-2f,
                                fminf(
                                    3e3f,
                                    10.0f / (1e-5f + powf(z / 5.0f, 2.0f) +
                                    powf(z / 200.0f, 6.0f))
                                )
                            );
                            if (info) ++info->transparent_accum;

                            gm_vec4 *acc = cam->accum + index;
                            acc->r += (float)stri.ac.r * alpha * w;
                            acc->g += (float)stri.ac.g * alpha * w;
                            acc->b += (float)stri.ac.b * alpha * w;
                            acc->a += alpha * w;
                            cam->revealance[index] *= 1.0f - alpha;
                            break;
                        }
                    }
                }
            }
        }
    }
}
void gm_render_tri3(
    gm_camera *cam,
    gm_tri3 stri,
    gm_renderpass pass
) {
    gm_render_tri3_debug(cam, stri, pass, NULL);
}

// adapted from: https://en.wikipedia.org/wiki/Liang–Barsky_algorithm
static inline float gm_maxi_arr(float arr[], int n) {
    float m = 0;
    for (int i = 0; i < n; ++i)
        if (m < arr[i])
            m = arr[i];
    return m;
}
static inline float gm_mini_arr(float arr[], int n) {
    float m = 1;
    for (int i = 0; i < n; ++i)
        if (m > arr[i])
            m = arr[i];
    return m;
}
static inline bool gm_liang_barsky(gm_camera *cam, gm_vec3 *a, gm_vec3 *b) {
    float p1 = -(b->x - a->x);
    float p2 = -p1;
    float p3 = -(b->y - a->y);
    float p4 = -p3;

    float q1 = a->x;
    float q2 = (float)cam->view.x - a->x;
    float q3 = a->y;
    float q4 = (float)cam->view.y - a->y;

    float exitParams[3], entryParams[3];
    int exitIndex = 1, entryIndex = 1;
    exitParams[0] = 1;
    entryParams[0] = 0;

    float inv_za = 1.0f / a->z;
    float inv_zb = 1.0f / b->z;

    if ((p1 == 0 && q1 < 0) ||
        (p2 == 0 && q2 < 0) ||
        (p3 == 0 && q3 < 0) ||
        (p4 == 0 && q4 < 0))
        return false;

    if (p1 != 0) {
        float r1 = q1 / p1;
        float r2 = q2 / p2;
        if (p1 < 0) {
            entryParams[entryIndex++] = r1;
            exitParams[exitIndex++] = r2;
        } else {
            entryParams[entryIndex++] = r2;
            exitParams[exitIndex++] = r1;
        }
    }
    if (p3 != 0) {
        float r3 = q3 / p3;
        float r4 = q4 / p4;
        if (p3 < 0) {
            entryParams[entryIndex++] = r3;
            exitParams[exitIndex++] = r4;
        } else {
            entryParams[entryIndex++] = r4;
            exitParams[exitIndex++] = r3;
        }
    }

    float max, min;
    max = gm_maxi_arr(entryParams, entryIndex);
    min = gm_mini_arr(exitParams, exitIndex);

    // outside clipping window
    if (max > min)
        return false;

    float x1 = a->x, y1 = a->y;
    float x2 = b->x, y2 = b->y;

    a->x = x1 + (x2 - x1) * max;
    a->y = y1 + (y2 - y1) * max;
    b->x = x1 + (x2 - x1) * min;
    b->y = y1 + (y2 - y1) * min;

    a->z = 1.0f / (inv_za + (inv_zb - inv_za) * max);
    b->z = 1.0f / (inv_za + (inv_zb - inv_za) * min);

    return true;
}

void gm_render_line(gm_camera *cam, gm_vec3 a, gm_vec3 b, gm_color color) {
    if (!gm_liang_barsky(cam, &a, &b))
        return;

    const float ax = a.x, ay = a.y;
    const float az = a.z, bz = b.z;
    const float dx = b.x - ax;
    const float dy = b.y - ay;
    const float len_sq = dx * dx + dy * dy;

    size_t steps = (size_t)ceilf(fmaxf(fabsf(dx), fabsf(dy)));
    if (steps == 0)
        steps = 1;

    const float sx = dx / (float)steps;
    const float sy = dy / (float)steps;

    gm_pcolor pcolor = gm_to_pcolor(color);

    for (size_t i = 0; i <= steps; ++i) {
        int32_t px = (int32_t)floorf(ax + sx * (float)i);
        int32_t py = (int32_t)floorf(ay + sy * (float)i);

        if (px < 0 || py < 0 ||
            px >= (int32_t)cam->fb.size.x ||
            py >= (int32_t)cam->fb.size.y)
            continue;

        size_t index = (size_t)py * cam->fb.size.x + (size_t)px;

        float t = len_sq > 0.0f
            ? (((float)px + 0.5f - ax) * dx +
               ((float)py + 0.5f - ay) * dy) / len_sq
            : 0.0f;

        t = fmaxf(0.0f, fminf(1.0f, t));

        float inv_z = (1.0f - t) / az + t / bz + 0.0005f;
        if (inv_z > cam->db[index])
            cam->fb.pixels[index] = pcolor;
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


void gm_render_mesh_raw(gm_camera *cam, gm_mesh *mesh, gm_transform *transform, gm_renderpass pass, gm_color color, gm_renderinfo *info) {
    gm_frustum frust = gm_camera_frustum(cam);
    for (gm_tri3 *tri = mesh->tris; tri < mesh->tris + mesh->tri_c; ++tri) {
        gm_tri3 rotated = {
            gm_transform_point(tri->a, transform),
            gm_transform_point(tri->b, transform),
            gm_transform_point(tri->c, transform),
            tri->ac, tri->bc, tri->cc
        };

        gm_tri3 screenspace = gm_camera_ctri(rotated, cam);

        // Clip to frustum
        gm_vec3 clipped[GM_CLIP_MAX];
        bool edge[GM_CLIP_MAX];
        uint32_t n = gm_clip_tri(screenspace, &frust, clipped, edge);
        if (n == 0) continue;

        // project in place
        for (uint32_t i = 0; i < n; ++i)
            clipped[i] = gm_camera_pvec(clipped[i], cam);

        switch (pass) {
            case GM_PASS_WIREFRAME: {
                for (uint32_t i = 0; i < n; ++i) {
                    if (edge[i])
                        gm_render_line(cam, clipped[i], clipped[(i + 1) % n], color);
                }
                break;
            }
            default: {
                for (uint32_t i = 1; i + 1 < n; ++i) {
                    gm_render_tri3_debug(cam, (gm_tri3) {
                        clipped[0], clipped[i], clipped[i + 1],
                        tri->ac, tri->bc, tri->cc
                    }, pass, info);
                }
                break;
            }
        }
    }
}

void gm_render_mesh(gm_camera *cam, gm_mesh *mesh, gm_transform *transform, gm_renderpass pass) {
    gm_render_mesh_raw(cam, mesh, transform, pass, GM_GREEN, NULL);
}
void gm_render_debug(gm_camera *cam, gm_mesh *mesh, gm_transform *transform, gm_renderpass pass, gm_renderinfo *info) {
    gm_render_mesh_raw(cam, mesh, transform, pass, GM_GREEN, info);
}
void gm_render_wireframe(gm_camera *cam, gm_mesh *mesh, gm_transform *transform, gm_color color) {
    gm_render_mesh_raw(cam, mesh, transform, GM_PASS_WIREFRAME, color, NULL);
}

void gm_render_composite(gm_camera *cam) {
    for (size_t i = 0; i < cam->fb.rawsize; ++i) {
        float reveal = cam->revealance[i];
        if (reveal >= 1.0f)
            continue;

        gm_vec4 *acc = cam->accum + i;
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
