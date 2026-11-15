#include "gamma/render.h"
#include <assert.h>
#include <math.h>
#include <stdbool.h>

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

static float gm_edge(gm_vec3 a, gm_vec3 b, float x, float y) {
    return (x - a.x) * (b.y - a.y)
        - (y - a.y) * (b.x - a.x);
}

static gm_vec3 gm_clip_lerp_z(gm_vec3 a, gm_vec3 b, float z) {
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
    gm_tri3 stri
) {
    float area;
    if (cam == NULL || cam->fb.pixels == NULL ||
        (area = gm_edge(stri.a, stri.b, stri.c.x, stri.c.y)) == 0.0f)
        return;

    float fw = (float)cam->fb.size.x;
    float fh = (float)cam->fb.size.y;

    float fmin_x = floorf(GM_MIN(stri.a.x, GM_MIN(stri.b.x, stri.c.x)));
    float fmin_y = floorf(GM_MIN(stri.a.y, GM_MIN(stri.b.y, stri.c.y)));
    float fmax_x = ceilf(GM_MAX(stri.a.x, GM_MAX(stri.b.x, stri.c.x)));
    float fmax_y = ceilf(GM_MAX(stri.a.y, GM_MAX(stri.b.y, stri.c.y)));

    fmin_x = fmin_x > 0.0f ? fmin_x : 0.0f;
    fmin_x = fmin_x < fw ? fmin_x : fw;
    fmin_y = fmin_y > 0.0f ? fmin_y : 0.0f;
    fmin_y = fmin_y < fh ? fmin_y : fh;
    fmax_x = fmax_x < fw ? fmax_x : fw;
    fmax_x = fmax_x > 0.0f ? fmax_x : 0.0f;
    fmax_y = fmax_y < fh ? fmax_y : fh;
    fmax_y = fmax_y > 0.0f ? fmax_y : 0.0f;

    uint32_t min_x = (uint32_t)fmin_x;
    uint32_t min_y = (uint32_t)fmin_y;
    uint32_t max_x = (uint32_t)fmax_x;
    uint32_t max_y = (uint32_t)fmax_y;

    if (min_x >= max_x || min_y >= max_y)
        return;

    gm_pcolor pcolor = gm_to_pcolor(stri.ac);
    float sign = area > 0.0f ? 1.0f : -1.0f;

    float inv_area = 1.0f / fabsf(area);
    float za = inv_area / stri.a.z;
    float zb = inv_area / stri.b.z;
    float zc = inv_area / stri.c.z;

    float dx_ab = (stri.b.y - stri.a.y) * sign;
    float dy_ab = -(stri.b.x - stri.a.x) * sign;
    float dx_bc = (stri.c.y - stri.b.y) * sign;
    float dy_bc = -(stri.c.x - stri.b.x) * sign;
    float dx_ca = (stri.a.y - stri.c.y) * sign;
    float dy_ca = -(stri.a.x - stri.c.x) * sign;

    float start_x = (float)min_x + 0.5f;
    float start_y = (float)min_y + 0.5f;

    float row_ab = gm_edge(stri.a, stri.b, start_x, start_y) * sign;
    float row_bc = gm_edge(stri.b, stri.c, start_x, start_y) * sign;
    float row_ca = gm_edge(stri.c, stri.a, start_x, start_y) * sign;

    for (uint32_t y = min_y; y < max_y; ++y) {
        gm_pcolor *pix = cam->fb.pixels + (size_t)y * cam->fb.size.x;
        float *db = cam->db + (size_t)y * cam->fb.size.x;

        float w_ab = row_ab;
        float w_bc = row_bc;
        float w_ca = row_ca;

        for (uint32_t x = min_x; x < max_x; ++x) {
            if (w_ab >= 0.0f && w_bc >= 0.0f && w_ca >= 0.0f) {
                float inv_z = w_bc * za + w_ca * zb + w_ab * zc;

                // check if new depth beats out already drawn depth
                if (inv_z > 0.0f && inv_z > db[x]) {
                    db[x] = inv_z;
                    pix[x] = pcolor;
                }
            }

            w_ab += dx_ab;
            w_bc += dx_bc;
            w_ca += dx_ca;
        }

        row_ab += dy_ab;
        row_bc += dy_bc;
        row_ca += dy_ca;
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


void gm_render_mesh(gm_camera *cam, gm_mesh *mesh) {
    for (gm_tri3 *tri = mesh->tris; tri < mesh->tris + mesh->tri_c; ++tri) {
        gm_tri3 rotated = {
            gm_transform_point(tri->a, &mesh->transform),
            gm_transform_point(tri->b, &mesh->transform),
            gm_transform_point(tri->c, &mesh->transform),
            tri->ac, tri->bc, tri->cc
        };

        gm_tri3 screenspace = gm_camera_ctri(rotated, cam);

        // Clip to near plane
        gm_tri3 clipped[2];
        int n = gm_clip_tri_near(screenspace, cam->perspective.near, clipped);
        for (int i = 0; i < n; ++i)
            gm_render_tri3(cam, gm_camera_ptri(clipped[i], cam));
    }
}