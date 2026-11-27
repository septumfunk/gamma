#include "gamma/scene.h"
#include "gamma/coord.h"
#include "gamma/render.h"
#include <stdio.h>
#include <stdlib.h>

gm_scene gm_scene_new(gm_color sky_color) {
    return (gm_scene) {
        NULL, 0, 0,
        NULL, 0, 0,
        NULL, 0, 0,

        sky_color
    };
}
void gm_scene_free(gm_scene *scene) {
    if (scene->meshes)
        free(scene->meshes);
    scene->meshes = NULL;
    scene->mesh_c = 0;
    scene->mesh_cap = 0;

    if (scene->instances)
        free(scene->instances);
    scene->instances = NULL;
    scene->inst_c = 0;
    scene->inst_cap = 0;

    if (scene->cameras)
        free(scene->cameras);
    scene->cameras = NULL;
    scene->cam_c = 0;
    scene->cam_cap = 0;
}

gm_handle gm_scene_addmesh(gm_scene *scene, gm_mesh mesh) {
    gm_meshslot slot = {
        1, true, mesh
    };

    if (scene->meshes) {
        if (scene->mesh_c == scene->mesh_cap) {
            uint32_t ncap = (uint32_t)(scene->mesh_cap * SLOT_GFACTOR);

            gm_meshslot *new = realloc(scene->meshes, ncap * sizeof(gm_meshslot));
            if (!new)
                return (gm_handle){GM_HANDLE_NULL, 0, 0};
            scene->meshes = new;

            memset(scene->meshes + scene->mesh_cap, 0, sizeof(gm_meshslot) * (ncap - scene->mesh_cap));
            scene->mesh_cap = ncap;
            scene->meshes[scene->mesh_c] = slot;
            return (gm_handle){GM_HANDLE_MESH, scene->mesh_c++, 1};
        }
        for (gm_meshslot *st = scene->meshes; st < scene->meshes + scene->mesh_cap; ++st) {
            if (!st->active) {
                slot.generation = st->generation + 1;
                *st = slot;
                ++scene->mesh_c;
                return (gm_handle){GM_HANDLE_MESH, (uint32_t)(st - scene->meshes), slot.generation};
            }
        }

        return (gm_handle){GM_HANDLE_NULL, 0, 1};
    }

    scene->meshes = calloc(SLOT_INIT, sizeof(gm_meshslot));
    if (!scene->meshes)
        return (gm_handle){GM_HANDLE_NULL, 0, 0};
    scene->meshes[0] = slot;
    ++scene->mesh_c;
    scene->mesh_cap = SLOT_INIT;

    return (gm_handle){GM_HANDLE_MESH, 0, 1};
}

static inline bool gm_idoob(gm_scene *scene, uint32_t id, gm_handlekind kind) {
    switch (kind) {
        case GM_HANDLE_MESH: return id >= scene->mesh_cap;
        case GM_HANDLE_INSTANCE: return id >= scene->inst_cap;
        case GM_HANDLE_CAMERA: return id >= scene->cam_cap;
        default: return true;
    }
}
static inline void *gm_idlookup(gm_scene *scene, uint32_t id, gm_handlekind kind) {
    if (gm_idoob(scene, id, kind))
        return NULL;

    switch (kind) {
        case GM_HANDLE_MESH: return scene->meshes + id;
        case GM_HANDLE_INSTANCE: return scene->instances + id;
        case GM_HANDLE_CAMERA: return scene->cameras + id;
        default: return NULL;
    }
}

#define gm_handletype(st, K, T) \
    case K: {\
        T *sp = (T *)st; \
        if (!sp->active || sp->generation != handle.generation) \
            return NULL; \
        return sp; \
    }
static inline void *gm_checkhandle(gm_scene *scene, gm_handle handle, gm_handlekind kind) {
    if (handle.kind != kind)
        return NULL;

    void *st = gm_idlookup(scene, handle.id, kind);
    if (!st) return NULL;

    switch (kind) {
        gm_handletype(st, GM_HANDLE_MESH, gm_meshslot);
        gm_handletype(st, GM_HANDLE_INSTANCE, gm_instslot);
        gm_handletype(st, GM_HANDLE_CAMERA, gm_camslot);
        default: return NULL;
    }
}

void gm_scene_delmesh(gm_scene *scene, gm_handle handle) {
    gm_meshslot *st = gm_checkhandle(scene, handle, GM_HANDLE_MESH);
    if (!st) return;

    st->active = false;
    --scene->mesh_c;

    // TODO: gm_mesh_free(&st->data);
}
gm_mesh *gm_scene_peekmesh(gm_scene *scene, gm_handle handle) {
    gm_meshslot *st = gm_checkhandle(scene, handle, GM_HANDLE_MESH);
    return st ? &st->data : NULL;
}

gm_handle gm_scene_addinst_init(gm_scene *scene, gm_handle mesh_id, gm_transform init) {
    if (!gm_checkhandle(scene, mesh_id, GM_HANDLE_MESH))
        return (gm_handle){GM_HANDLE_NULL, 0, 0};
    gm_instslot slot = {
        1, true, {mesh_id, init, UINT32_MAX}
    };

    if (scene->instances) {
        if (scene->inst_c == scene->inst_cap) {
            uint32_t ncap = (uint32_t)(scene->inst_cap * SLOT_GFACTOR);

            gm_instslot *new = realloc(scene->instances, ncap * sizeof(gm_instslot));
            if (!new)
                return (gm_handle){GM_HANDLE_NULL, 0, 0};
            scene->instances = new;

            memset(scene->instances + scene->inst_cap, 0, sizeof(gm_instslot) * (ncap - scene->inst_cap));
            scene->inst_cap = ncap;
            scene->instances[scene->inst_c] = slot;
            return (gm_handle){GM_HANDLE_INSTANCE, scene->inst_c++, 1};
        }
        for (gm_instslot *st = scene->instances; st < scene->instances + scene->inst_cap; ++st) {
            if (!st->active) {
                slot.generation = st->generation + 1;
                *st = slot;
                ++scene->inst_c;

                return (gm_handle){
                    GM_HANDLE_INSTANCE,
                    (uint32_t)(st - scene->instances),
                    slot.generation
                };
            }
        }

        return (gm_handle){GM_HANDLE_NULL, 0, 0};
    }

    scene->instances = calloc(SLOT_INIT, sizeof(gm_instslot));
    if (!scene->instances)
        return (gm_handle){GM_HANDLE_NULL, 0, 0};
    scene->instances[0] = slot;
    ++scene->inst_c;
    scene->inst_cap = SLOT_INIT;

    return (gm_handle){GM_HANDLE_INSTANCE, 0, 1};
}
gm_instance *gm_scene_peekinst(gm_scene *scene, gm_handle handle) {
    gm_instslot *st = gm_checkhandle(scene, handle, GM_HANDLE_INSTANCE);
    return st ? &st->data : NULL;
}
void gm_scene_hideinst(gm_scene *scene, gm_handle inst, gm_handle camera) {
    gm_instance *is = gm_scene_peekinst(scene, inst);
    if (!is || !gm_checkhandle(scene, camera, GM_HANDLE_CAMERA))
        return;
    is->cameras &= ~GM_CAMERA_BIT(camera.id);
}
void gm_scene_showinst(gm_scene *scene, gm_handle inst, gm_handle camera) {
    gm_instance *is = gm_scene_peekinst(scene, inst);
    if (!is || !gm_checkhandle(scene, camera, GM_HANDLE_CAMERA))
        return;
    is->cameras |= GM_CAMERA_BIT(camera.id);
}

gm_handle gm_scene_addcam(gm_scene *scene, gm_camera camera) {
    if (scene->cam_c >= 32)
        return (gm_handle){GM_HANDLE_NULL, 0, 0};
    gm_camslot slot = {
        1, true, camera
    };

    if (scene->cameras) {
        if (scene->cam_c == scene->cam_cap) {
            uint32_t ncap = (uint32_t)(scene->cam_cap * SLOT_GFACTOR);

            gm_camslot *new = realloc(scene->cameras, ncap * sizeof(gm_camslot));
            if (!new)
                return (gm_handle){GM_HANDLE_NULL, 0, 0};
            scene->cameras = new;

            memset(scene->cameras + scene->cam_cap, 0, sizeof(gm_camslot) * (ncap - scene->cam_cap));
            scene->cam_cap = ncap;
            scene->cameras[scene->cam_c] = slot;
            return (gm_handle){GM_HANDLE_CAMERA, scene->cam_c++, 1};
        }
        for (gm_camslot *st = scene->cameras; st < scene->cameras + scene->cam_cap; ++st) {
            if (!st->active) {
                slot.generation = st->generation + 1;
                *st = slot;
                ++scene->cam_c;

                return (gm_handle){
                    GM_HANDLE_CAMERA,
                    (uint32_t)(st - scene->cameras),
                    slot.generation
                };
            }
        }

        return (gm_handle){GM_HANDLE_NULL, 0, 0};
    }

    scene->cameras = calloc(SLOT_INIT, sizeof(gm_camslot));
    if (!scene->cameras)
        return (gm_handle){GM_HANDLE_NULL, 0, 0};
    scene->cameras[0] = slot;
    ++scene->cam_c;
    scene->cam_cap = SLOT_INIT;

    return (gm_handle){GM_HANDLE_CAMERA, 0, 1};
}

void gm_scene_delcam(gm_scene *scene, gm_handle handle);

gm_camera *gm_scene_peekcam(gm_scene *scene, gm_handle handle) {
    gm_camslot *st = gm_checkhandle(scene, handle, GM_HANDLE_CAMERA);
    return st ? &st->data : NULL;
}

static inline void gm_scene_renderinst(gm_scene *scene, gm_camera *cam, gm_instslot *is, gm_renderpass pass) {
    gm_mesh *mesh = gm_scene_peekmesh(scene, is->data.mesh);
    if (!mesh) {
        fprintf(stderr, "RENDER: Instance refers to invalid mesh\n");
        return;
    }

    if (is->data.transform.p_handle.kind == GM_HANDLE_INSTANCE) {
        gm_instslot *ps = gm_checkhandle(scene, is->data.transform.p_handle,
            GM_HANDLE_INSTANCE);
        is->data.transform.parent = ps ? &ps->data.transform : NULL;
    } else if (is->data.transform.p_handle.kind == GM_HANDLE_CAMERA) {
        gm_camslot *ps = gm_checkhandle(scene, is->data.transform.p_handle,
            GM_HANDLE_CAMERA);
        is->data.transform.parent = ps ? &ps->data.transform : NULL;
    }

    gm_render_mesh(cam, mesh, &is->data.transform, pass);
}
void gm_scene_render(gm_scene *scene) {
    for (gm_camslot *cs = scene->cameras; cs < scene->cameras + scene->cam_cap; ++cs) {
        if (!cs->active) continue;

        gm_camera *cam = &cs->data;
        gm_camera_clear(cam, scene->sky_color);

        static const gm_renderpass p_order[] = {
            GM_PASS_OPAQUE,
            GM_PASS_TRANSPARENT,
            GM_PASS_WIREFRAME
        };
        for (int p = 0; p < (int)(sizeof(p_order) / sizeof(*p_order)); ++p) {
            if (cam->passes & p_order[p])
                for (gm_instslot *is = scene->instances; is < scene->instances + scene->inst_cap; ++is) {
                    if (is->active && (is->data.cameras & GM_CAMERA_BIT(cs - scene->cameras)))
                        gm_scene_renderinst(scene, cam, is, p_order[p]);
                }
        }
        gm_render_composite(cam);
    }
}