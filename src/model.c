#include "model.h"

#include "core/platform.h"
#include "core/runtime.h"
#include "core/types.h"

#include <assert.h>
#include <stddef.h>

static AABB_Collider get_vertex_array_aabb_collider(Vertex_Array vertices) {
  Vec3 min = vec3(INF_F32, INF_F32, INF_F32);
  Vec3 max = vec3(-INF_F32, -INF_F32, -INF_F32);
  for (usize i = 0; i < vertices.len; i += 1) {
    min.x = min_f32(min.x, vertices.items[i].position.x);
    min.y = min_f32(min.y, vertices.items[i].position.y);
    min.z = min_f32(min.z, vertices.items[i].position.z);

    max.x = max_f32(max.x, vertices.items[i].position.x);
    max.y = max_f32(max.y, vertices.items[i].position.y);
    max.z = max_f32(max.z, vertices.items[i].position.z);
  }

  return (AABB_Collider){
    .min = min,
    .max = max,
  };
}

Mesh_Primitive_Create_Result
make_mesh_primitive(Mesh_Primitive_Create_Info *info) {
  Mesh_Primitive primitive = (Mesh_Primitive){
    .index_count = info->indices.len,
    .gpu_vertices = gpu_buffer_append(
        info->gpu_allocator,
        info->vertices.items,
        sizeof(info->vertices.items[0]) * info->vertices.len
    ),
    .gpu_indices = gpu_buffer_append(
        info->gpu_allocator,
        info->indices.items,
        sizeof(info->indices.items[0]) * info->indices.len
    ),
  };

  // FIXME(nico): This sucks. With an arena style allocator, we cannot recover
  // from one of the two buffer memory being invalid

  return ok(Mesh_Primitive_Create_Result, primitive);
}

// FIXME(nico): Same problem as before with the arena style. We cannot reclaim
// the gpu memory
Model_Error destroy_mesh_primitive(Mesh_Primitive *primitive) {
  (void)primitive;
  return Model_Error_None;
}

// FIXME(nico): not checking if the primitive buffer is being correctly created
Model_Create_Result make_model(Model_Create_Info *info, Allocator allocator) {
  errdefer_scope;

  if (info->primitive_count == 0) {
    return err(Model_Create_Result, Model_Error_Invalid_Data);
  }

  Model model = {
    .allocator = allocator,
    .primitive_count = info->primitive_count,
  };

  model.primitives = or_return(
      alloc(allocator, sizeof(Mesh_Primitive) * info->primitive_count),
      err(Model_Create_Result, Model_Error_Failed_To_Create)
  );
  errdefer {
    free_(allocator, model.primitives);
  };

  model.default_materials = or_return(
      alloc(allocator, sizeof(Gen_Handle) * info->primitive_count),
      err(Model_Create_Result, Model_Error_Failed_To_Create)
  );
  errdefer {
    free_(allocator, model.default_materials);
  };

  for (usize i = 0; i < info->primitive_count; i += 1) {
    Vertex_Array cpu_vertices = info->cpu_vertices[i];
    Index_Array cpu_indices = info->cpu_indices[i];
    model.primitives[i] = (Mesh_Primitive){
      .index_count = cpu_indices.len,
      .gpu_vertices = gpu_buffer_append(
          info->gpu_allocator,
          cpu_vertices.items,
          sizeof(cpu_vertices.items[0]) * cpu_vertices.len
      ),
      .gpu_indices = gpu_buffer_append(
          info->gpu_allocator,
          cpu_indices.items,
          sizeof(cpu_indices.items[0]) * cpu_indices.len
      ),
      .collider = get_vertex_array_aabb_collider(cpu_vertices),
    };
    model.default_materials[i] = info->default_material[i];
  }

  model.collider.min = vec3(INF_F32, INF_F32, INF_F32);
  model.collider.max = vec3(-INF_F32, -INF_F32, -INF_F32);
  for (usize i = 0; i < model.primitive_count; i += 1) {
    model.collider.min.x =
        min_f32(model.collider.min.x, model.primitives[i].collider.min.x);
    model.collider.min.y =
        min_f32(model.collider.min.y, model.primitives[i].collider.min.y);
    model.collider.min.z =
        min_f32(model.collider.min.z, model.primitives[i].collider.min.z);

    model.collider.max.x =
        max_f32(model.collider.max.x, model.primitives[i].collider.max.x);
    model.collider.max.y =
        max_f32(model.collider.max.y, model.primitives[i].collider.max.y);
    model.collider.max.z =
        max_f32(model.collider.max.z, model.primitives[i].collider.max.z);
  }

  return_ok(Model_Create_Result, model);
}

// FIXME(nico): Same problem as before with the arena style. We cannot reclaim
// the gpu memory
Model_Error destroy_model(Model *model) {
  (void)model;
  free_(model->allocator, model->primitives);
  free_(model->allocator, model->default_materials);
  return Model_Error_None;
}

#define V(px, py, pz, nx, ny, nz, u, v)                                        \
  {                                                                            \
    .position = {.raw = {px, py, pz, 1.f}},                                    \
    .normal = {.raw = {nx, ny, nz, 0.f}},                                      \
    .tex_coord = {.raw = {u, v}},                                              \
  }

Model_Create_Result make_cube_model(
    GPU_Buffer *gpu_allocator, Gen_Handle default_material, Allocator allocator
) {
  // 4 vertices per face for hard edges, CCW front faces
  Vertex vertices[] = {
    // +Y (top)
    V(-.5f, .5f, -.5f, 0.f, 1.f, 0.f, 0.f, 0.f),
    V(-.5f, .5f, .5f, 0.f, 1.f, 0.f, 0.f, 1.f),
    V(.5f, .5f, .5f, 0.f, 1.f, 0.f, 1.f, 1.f),
    V(.5f, .5f, -.5f, 0.f, 1.f, 0.f, 1.f, 0.f),
    // -Y (bottom)
    V(-.5f, -.5f, -.5f, 0.f, -1.f, 0.f, 0.f, 0.f),
    V(.5f, -.5f, -.5f, 0.f, -1.f, 0.f, 1.f, 0.f),
    V(.5f, -.5f, .5f, 0.f, -1.f, 0.f, 1.f, 1.f),
    V(-.5f, -.5f, .5f, 0.f, -1.f, 0.f, 0.f, 1.f),
    // +X
    V(.5f, -.5f, -.5f, 1.f, 0.f, 0.f, 0.f, 1.f),
    V(.5f, .5f, -.5f, 1.f, 0.f, 0.f, 0.f, 0.f),
    V(.5f, .5f, .5f, 1.f, 0.f, 0.f, 1.f, 0.f),
    V(.5f, -.5f, .5f, 1.f, 0.f, 0.f, 1.f, 1.f),
    // -X
    V(-.5f, -.5f, -.5f, -1.f, 0.f, 0.f, 1.f, 1.f),
    V(-.5f, -.5f, .5f, -1.f, 0.f, 0.f, 0.f, 1.f),
    V(-.5f, .5f, .5f, -1.f, 0.f, 0.f, 0.f, 0.f),
    V(-.5f, .5f, -.5f, -1.f, 0.f, 0.f, 1.f, 0.f),
    // +Z
    V(-.5f, -.5f, .5f, 0.f, 0.f, 1.f, 0.f, 1.f),
    V(.5f, -.5f, .5f, 0.f, 0.f, 1.f, 1.f, 1.f),
    V(.5f, .5f, .5f, 0.f, 0.f, 1.f, 1.f, 0.f),
    V(-.5f, .5f, .5f, 0.f, 0.f, 1.f, 0.f, 0.f),
    // -Z
    V(-.5f, -.5f, -.5f, 0.f, 0.f, -1.f, 1.f, 1.f),
    V(-.5f, .5f, -.5f, 0.f, 0.f, -1.f, 1.f, 0.f),
    V(.5f, .5f, -.5f, 0.f, 0.f, -1.f, 0.f, 0.f),
    V(.5f, -.5f, -.5f, 0.f, 0.f, -1.f, 0.f, 1.f),
  };

  u32 indices[36];
  for (u32 face = 0; face < 6; face += 1) {
    u32 base = face * 4;
    indices[face * 6 + 0] = base;
    indices[face * 6 + 1] = base + 1;
    indices[face * 6 + 2] = base + 2;
    indices[face * 6 + 3] = base;
    indices[face * 6 + 4] = base + 2;
    indices[face * 6 + 5] = base + 3;
  }

  return make_model(
      &(Model_Create_Info){
        .gpu_allocator = gpu_allocator,
        .cpu_vertices =
            (Vertex_Array[]){
              [0] = {.items = vertices, .len = 24},
            },
        .cpu_indices =
            (Index_Array[]){
              [0] = {.items = indices, .len = 36},
            },
        .default_material = (Gen_Handle[]){[0] = default_material},
        .primitive_count = 1,
      },
      allocator
  );
}

Model_Create_Result make_plane_model(
    GPU_Buffer *gpu_allocator, Gen_Handle default_material, Allocator allocator
) {
  Vertex vertices[] = {
    V(-.5f, 0.f, -.5f, 0.f, 1.f, 0.f, 0.f, 0.f),
    V(-.5f, 0.f, .5f, 0.f, 1.f, 0.f, 0.f, 1.f),
    V(.5f, 0.f, .5f, 0.f, 1.f, 0.f, 1.f, 1.f),
    V(.5f, 0.f, -.5f, 0.f, 1.f, 0.f, 1.f, 0.f),
  };
  u32 indices[] = {0, 1, 2, 0, 2, 3};

  return make_model(
      &(Model_Create_Info){
        .gpu_allocator = gpu_allocator,
        .cpu_vertices =
            (Vertex_Array[]){
              [0] = {.items = vertices, .len = 4},
            },
        .cpu_indices =
            (Index_Array[]){
              [0] = {.items = indices, .len = 6},
            },
        .default_material = (Gen_Handle[]){[0] = default_material},
        .primitive_count = 1,
      },
      allocator
  );
}

#undef V