#ifndef SCENE_H
#define SCENE_H

#include "coord.h"
#include "render.h"
#include <stdbool.h>

typedef struct {
    gm_handle mesh;
    gm_transform transform;
    uint32_t cameras;
} gm_instance;
#define GM_CAMERA_BIT(i) (1u << (i))

typedef struct {
    uint32_t id;
    uint32_t generation;
    float distance;
} gm_sort;

#define SLOT_INIT 4
#define SLOT_GFACTOR 1.5

typedef struct {
    uint32_t generation;
    bool active;
    gm_mesh data;
} gm_meshslot;
typedef struct {
    uint32_t generation;
    bool active;
    gm_instance data;
} gm_instslot;

typedef struct {
    uint32_t generation;
    bool active;
    gm_camera data;
} gm_camslot;

typedef struct {
    gm_meshslot *meshes;
    uint32_t mesh_c, mesh_cap;

    gm_instslot *instances;
    uint32_t inst_c, inst_cap;

    gm_camslot *cameras;
    uint32_t cam_c, cam_cap;

    gm_color sky_color;
} gm_scene;

gm_scene gm_scene_new(gm_color sky_color);
void gm_scene_free(gm_scene *scene);

gm_handle gm_scene_addmesh(gm_scene *scene, gm_mesh mesh);
void gm_scene_delmesh(gm_scene *scene, gm_handle handle);
gm_mesh *gm_scene_peekmesh(gm_scene *scene, gm_handle handle);

gm_handle gm_scene_addinst_init(gm_scene *scene, gm_handle mesh_id, gm_transform init);
static inline gm_handle gm_scene_addinst(gm_scene *scene, gm_handle mesh_id) {
    return gm_scene_addinst_init(scene, mesh_id, GM_TRANSFORM_NULL);
}
gm_instance *gm_scene_peekinst(gm_scene *scene, gm_handle handle);
void gm_scene_hideinst(gm_scene *scene, gm_handle inst, gm_handle camera);
void gm_scene_showinst(gm_scene *scene, gm_handle inst, gm_handle camera);

gm_handle gm_scene_addcam(gm_scene *scene, gm_camera camera);
void gm_scene_delcam(gm_scene *scene, gm_handle handle);
gm_camera *gm_scene_peekcam(gm_scene *scene, gm_handle handle);

void gm_scene_render(gm_scene *scene);

#endif // SCENE_H
