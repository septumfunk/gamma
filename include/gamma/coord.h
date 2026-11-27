#ifndef COORD_H
#define COORD_H

#include <stddef.h>
#include <math.h>
#include <stdint.h>

#define GM_MIN(a, b) ((a) < (b) ? (a) : (b))
#define GM_MAX(a, b) ((a) > (b) ? (a) : (b))

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

static inline gm_vec3 gm_vec3_add(gm_vec3 a, gm_vec3 b) {
    return (gm_vec3){a.x + b.x, a.y + b.y, a.z + b.z};
}
static inline gm_vec3 gm_vec3_sub(gm_vec3 a, gm_vec3 b) {
    return (gm_vec3){a.x - b.x, a.y - b.y, a.z - b.z};
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

static inline float gm_vec3_dot(gm_vec3 a, gm_vec3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
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

typedef enum {
    GM_HANDLE_MESH,
    GM_HANDLE_INSTANCE,
    GM_HANDLE_CAMERA,
    GM_HANDLE_NULL,
} gm_handlekind;
typedef struct {
    gm_handlekind kind;
    uint32_t id;
    uint32_t generation;
} gm_handle;

typedef struct gm_transform {
    struct gm_transform *parent;
    gm_handle p_handle;

    gm_vec3 position;
    gm_vec3 rotation;
    gm_vec3 scale;
} gm_transform;
#define GM_TRANSFORM_NULL ((gm_transform){NULL, {GM_HANDLE_NULL, 0, 0}, GM_VEC3_NULL, \
    GM_VEC3_NULL, GM_VEC3_NULL})

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

static inline gm_vec3 gm_transform_applyrot(gm_transform *transform, gm_vec3 rot) {
    rot = gm_vec3_rotx(rot, transform->rotation.x);
    rot = gm_vec3_roty(rot, transform->rotation.y);
    rot = gm_vec3_rotz(rot, transform->rotation.z);

    if (transform->parent)
        rot = gm_transform_applyrot(transform->parent, rot);

    return rot;
}

static inline gm_vec3 gm_transform_forward(gm_transform *transform) {
    return gm_transform_applyrot(transform, (gm_vec3){0.0f, 0.0f, 1.0f});
}

static inline float gm_transform_distance(gm_transform *transform, gm_vec3 point) {
    gm_vec3 delta = gm_vec3_sub(point, transform->position);
    return gm_vec3_dot(delta, gm_transform_forward(transform));
}

#endif // COORD_H
