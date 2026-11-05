#ifndef COORD_H
#define COORD_H

#include <stddef.h>
#include <math.h>
#include <stdint.h>

#define GM_MIN(a, b) ((a) < (b) ? (a) : (b))
#define GM_MAX(a, b) ((a) > (b) ? (a) : (b))
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static inline float gm_rad2deg(float rad) {
    return rad * (180.0f / (float)M_PI);
}
static inline float gm_deg2rad(float deg) {
    return deg / (180.0f / (float)M_PI);
}

typedef struct {
    float x;
    float y;
} gm_vec2;
#define GM_VEC2_NULL ((gm_vec2){0.0f, 0.0f})

static inline gm_vec2 gm_vec2_rotate(gm_vec2 p, float radians) {
    float c = cosf(radians);
    float s = sinf(radians);
    return (gm_vec2) {
        p.x * c - p.y * s,
        p.x * s + p.y * c
    };
}
static inline gm_vec2 gm_vec2_rotate_around(gm_vec2 p, gm_vec2 pivot, float radians) {
    p.x -= pivot.x;
    p.y -= pivot.y;

    p = gm_vec2_rotate(p, radians);

    p.x += pivot.x;
    p.y += pivot.y;

    return p;
}

typedef struct {
    float x;
    float y;
    float z;
} gm_vec3;
#define GM_VEC3_NULL ((gm_vec3){0.0f, 0.0f, 0.0f})

static inline gm_vec3 gm_vec3_inverse(gm_vec3 point) {
    return (gm_vec3){-point.x, -point.y, -point.z};
}

static inline gm_vec3 gm_vec3_rotx(gm_vec3 p, float euler) {
    euler = gm_deg2rad(euler);
    float c = cosf((euler)), s = sinf(euler);

    return (gm_vec3) {
        p.x,
        p.y * c - p.z * s,
        p.y * s + p.z * c
    };;
}
static inline gm_vec3 gm_vec3_roty(gm_vec3 p, float euler) {
    euler = gm_deg2rad(euler);
    float c = cosf(euler), s = sinf(euler);

    return (gm_vec3) {
        p.x * c + p.z * s,
        p.y,
        -p.x * s + p.z * c
    };
}
static inline gm_vec3 gm_vec3_rotz(gm_vec3 p, float euler) {
    euler = gm_deg2rad(euler);
    float c = cosf(euler), s = sinf(euler);

    return (gm_vec3) {
        p.x * c - p.y * s,
        p.x * s + p.y * c,
        p.z
    };
}

typedef struct {
    int32_t x;
    int32_t y;
} gm_vec2i;
#define GM_VEC2I_NULL ((gm_vec2i){0, 0})
typedef struct {
    int32_t x;
    int32_t y;
    int32_t z;
} gm_vec3i;
#define GM_VEC3I_NULL ((gm_vec3i){0, 0, 0})

typedef struct {
    uint32_t x;
    uint32_t y;
} gm_vec2u;
#define GM_VEC2U_NULL ((gm_vec2u){0, 0})
typedef struct {
    uint32_t x;
    uint32_t y;
    uint32_t z;
} gm_vec3u;
#define GM_VEC3U_NULL ((gm_vec3u){0, 0, 0})

typedef struct gm_transform {
    struct gm_transform *parent;

    gm_vec3 position;
    gm_vec3 rotation;
    gm_vec3 scale;
} gm_transform;
#define GM_TRANSFORM_NULL ((gm_transform){NULL, GM_VEC3_NULL, GM_VEC3_NULL, GM_VEC3_NULL})

static inline gm_vec3 gm_transform_point(gm_vec3 point, gm_transform *transform) {
    point = gm_vec3_rotx(point, transform->rotation.x);
    point = gm_vec3_roty(point, transform->rotation.y);
    point = gm_vec3_rotz(point, transform->rotation.z);

    point.x += transform->position.x;
    point.y += transform->position.y;
    point.z += transform->position.z;

    if (transform->parent)
        point = gm_transform_point(point, transform->parent);

    return point;
}

static inline void gm_transform_move(gm_transform *transform, gm_vec3 movement) {
    movement = gm_vec3_rotx(movement, transform->rotation.x);
    movement = gm_vec3_roty(movement, transform->rotation.y);
    movement = gm_vec3_rotz(movement, transform->rotation.z);

    transform->position.x += movement.x;
    transform->position.y += movement.y;
    transform->position.z += movement.z;
}

#endif // COORD_H
