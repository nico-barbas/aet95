#include "model.h"

#include "core/platform.h"
#include "core/types.h"
#include "material.h"

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

// static Model_Create_Result
// make_model_from_raw_geometry(Model_Create_Info *info) {
//   if (info->kind != Model_Create_Source_Raw_Geometry) {
//     return err(Model_Create_Result, Model_Error_Invalid_Data);
//   }

//   Vertex_Array cpu_vertices = info->raw.cpu_vertices;
//   Index_Array cpu_indices = info->raw.cpu_indices;

//   Model model = {
//     .primitives =
//         {
//           [0] =
//               (Mesh_Primitive){
//                 .index_count = cpu_indices.len,
//                 .gpu_vertices = gpu_buffer_append(
//                     info->gpu_allocator,
//                     cpu_vertices.items,
//                     sizeof(cpu_vertices.items[0]) * cpu_vertices.len
//                 ),
//                 .gpu_indices = gpu_buffer_append(
//                     info->gpu_allocator,
//                     cpu_indices.items,
//                     sizeof(cpu_indices.items[0]) * cpu_indices.len
//                 ),
//               },
//         },
//     .default_materials =
//         {
//           [0] = info->raw.default_material,
//         },
//     .primitive_count = 1,
//     .collider = get_vertex_array_aabb_collider(cpu_vertices),
//   };

//   return ok(Model_Create_Result, model);
// }

// static Model_Create_Result
// make_model_from_gltf_file(Model_Create_Info *info, Allocator allocator) {
//   if (info->gltf.node_name.len == 0) {
//     return err(Model_Create_Result, Model_Error_Invalid_Data);
//   }

//   Gltf_Mesh gltf_model = or_return(
//       gltf_document_parse_mesh(
//           info->gltf.document,
//           &(Gltf_Mesh_Parse_Info){
//             .node_name = info->gltf.node_name,
//             .vertex_stride = sizeof(Vertex) / sizeof(f32),
//             .attribute_formats =
//                 {
//                   [Gltf_Supported_Attribute_Position] =
//                       {
//                         .count = 4,
//                         .offset = offsetof(Vertex, position),
//                         .splat = 1.f,
//                       },
//                   [Gltf_Supported_Attribute_Normal] =
//                       {
//                         .count = 4,
//                         .offset = offsetof(Vertex, normal),
//                         .splat = 0.f,
//                       },
//                   [Gltf_Supported_Attribute_Texcoord] =
//                       {
//                         .count = 2,
//                         .offset = offsetof(Vertex, tex_coord),
//                       },
//                 }
//           },
//           allocator
//       ),
//       err(Model_Create_Result, Model_Error_Failed_To_Create_From_GLTF_File)
//   );

//   gltf_model.

//   // cgltf_data *data = (cgltf_data *)info->gltf.data;

//   // if (data->nodes_count == 0) {
//   //   return err(Model_Create_Result, Model_Error_Invalid_GLTF_File);
//   // }

//   // Model model = {0};
//   // cgltf_mesh *mesh = nullptr;
//   // for (usize i = 0; i < data->nodes_count; i += 1) {
//   //   cgltf_node *node = &data->nodes[i];
//   //   String name = from_cstring(node->name);
//   //   if (string_equal(name, info->gltf.model_name) && node->mesh !=
//   nullptr) {
//   //     mesh = node->mesh;
//   //     break;
//   //   }
//   // }

//   // if (mesh == nullptr || mesh->primitives_count > MESH_PRIMITIVE_CAP) {
//   //   return err(
//   //       Model_Create_Result, Model_Error_Failed_To_Create_From_GLTF_File
//   //   );
//   // }

//   // model.primitive_count = mesh->primitives_count;
//   // for (usize i = 0; i < model.primitive_count; i += 1) {
//   //   cgltf_primitive *prim = &mesh->primitives[i];

//   //   cgltf_accessor *pos_accessor = nullptr;
//   //   cgltf_accessor *norm_accessor = nullptr;
//   //   cgltf_accessor *tex_coord_accessor = nullptr;

//   //   for (usize j = 0; j < prim->attributes_count; j += 1) {
//   //     cgltf_attribute *attribute = &prim->attributes[j];

//   //     if (attribute->type == cgltf_attribute_type_position) {
//   //       pos_accessor = attribute->data;
//   //     } else if (attribute->type == cgltf_attribute_type_normal) {
//   //       norm_accessor = attribute->data;
//   //     } else if (
//   //         attribute->type == cgltf_attribute_type_texcoord &&
//   //         attribute->index == 0
//   //     ) {
//   //       // FIXME(nico): add support for multiple uv channels when needed
//   //       tex_coord_accessor = attribute->data;
//   //     }
//   //   }

//   //   assert(pos_accessor != nullptr);
//   //   assert(norm_accessor != nullptr);
//   //   assert(tex_coord_accessor != nullptr);
//   //   assert(
//   //       pos_accessor->count == norm_accessor->count &&
//   //       pos_accessor->count == tex_coord_accessor->count
//   //   );

//   //   Vertex_Array vertices =
//   //       make_array(vertices, pos_accessor->count, allocator);
//   //   if (vertices.items == nullptr) {
//   //     return err(
//   //         Model_Create_Result, Model_Error_Failed_To_Create_From_GLTF_File
//   //     );
//   //   }
//   //   defer {
//   //     delete_array(vertices);
//   //   };

//   //   for (usize j = 0; j < vertices.len; j += 1) {
//   //     Vertex *v = array_get_ptr(vertices, j);
//   //     cgltf_accessor_read_float(pos_accessor, j, v->position.raw, 3);
//   //     v->position.w = 1.0f;
//   //     cgltf_accessor_read_float(norm_accessor, j, v->normal.raw, 3);
//   //     v->normal.w = 0.0f;
//   //     cgltf_accessor_read_float(tex_coord_accessor, j, v->tex_coord.raw,
//   2);
//   //   }

//   //   cgltf_accessor *index_accessor = prim->indices;
//   //   if (index_accessor == nullptr) {
//   //     return err(
//   //         Model_Create_Result, Model_Error_Failed_To_Create_From_GLTF_File
//   //     );
//   //   }

//   //   usize index_count = index_accessor->count;
//   //   Index_Array indices = make_array(indices, index_count, allocator);
//   //   if (indices.items == nullptr) {
//   //     return err(
//   //         Model_Create_Result, Model_Error_Failed_To_Create_From_GLTF_File
//   //     );
//   //   }
//   //   defer {
//   //     delete_array(indices);
//   //   };

//   //   for (usize j = 0; j < index_count; j += 1) {
//   //     u32 *index = array_get_ptr(indices, j);
//   //     cgltf_accessor_read_uint(index_accessor, j, index, 1);
//   //   }

//   //   Material_Create_Result material_result = material_load_gltf(
//   //       renderer, prim->material, info->root_path, allocator
//   //   );
//   //   assert(material_result.ok);

//   //   model.primitives[i] = (Mesh_Primitive){
//   //     .index_count = index_count,
//   //     .gpu_vertices = gpu_buffer_append(
//   //         info->gpu_allocator, vertices.items, sizeof(Vertex) *
//   vertices.len
//   //     ),
//   //     .gpu_indices = gpu_buffer_append(
//   //         info->gpu_allocator, indices.items, sizeof(u32) * indices.len
//   //     ),
//   //     .collider = get_vertex_array_aabb_collider(vertices),
//   //   };

//   //   model.default_materials[i] = material_result.value.handle;
//   // }

//   // model.collider.min = vec3(INF_F32, INF_F32, INF_F32);
//   // model.collider.max = vec3(-INF_F32, -INF_F32, -INF_F32);
//   // for (usize i = 0; i < model.primitive_count; i += 1) {
//   //   model.collider.min.x =
//   //       min_f32(model.collider.min.x, model.primitives[i].collider.min.x);
//   //   model.collider.min.y =
//   //       min_f32(model.collider.min.y, model.primitives[i].collider.min.y);
//   //   model.collider.min.z =
//   //       min_f32(model.collider.min.z, model.primitives[i].collider.min.z);

//   //   model.collider.max.x =
//   //       max_f32(model.collider.max.x, model.primitives[i].collider.max.x);
//   //   model.collider.max.y =
//   //       max_f32(model.collider.max.y, model.primitives[i].collider.max.y);
//   //   model.collider.max.z =
//   //       max_f32(model.collider.max.z, model.primitives[i].collider.max.z);
//   // }

//   // return ok(Model_Create_Result, model);
// }

// FIXME(nico): not checking if the primitive buffer is being correctly created
Model_Create_Result make_model(Model_Create_Info *info) {
  Model model = {
    .primitive_count = info->primitive_count,
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

  return ok(Model_Create_Result, model);
}

// FIXME(nico): Same problem as before with the arena style. We cannot reclaim
// the gpu memory
Model_Error destroy_model(Model *model) {
  (void)model;
  return Model_Error_None;
}

#define V(px, py, pz, nx, ny, nz, u, v)                                        \
  {                                                                            \
    .position = {.raw = {px, py, pz, 1.f}},                                    \
    .normal = {.raw = {nx, ny, nz, 0.f}},                                      \
    .tex_coord = {.raw = {u, v}},                                              \
  }

Model_Create_Result
make_cube_model(GPU_Buffer *gpu_allocator, Gen_Handle default_material) {
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

  return make_model(&(Model_Create_Info){
    .gpu_allocator = gpu_allocator,
    .cpu_vertices =
        {
          [0] = (Vertex_Array){.items = vertices, .len = 24},
        },
    .cpu_indices =
        {
          [0] = (Index_Array){.items = indices, .len = 36},
        },
    .default_material = {[0] = default_material},
    .primitive_count = 1,
  });
}

Model_Create_Result
make_plane_model(GPU_Buffer *gpu_allocator, Gen_Handle default_material) {
  Vertex vertices[] = {
    V(-.5f, 0.f, -.5f, 0.f, 1.f, 0.f, 0.f, 0.f),
    V(-.5f, 0.f, .5f, 0.f, 1.f, 0.f, 0.f, 1.f),
    V(.5f, 0.f, .5f, 0.f, 1.f, 0.f, 1.f, 1.f),
    V(.5f, 0.f, -.5f, 0.f, 1.f, 0.f, 1.f, 0.f),
  };
  u32 indices[] = {0, 1, 2, 0, 2, 3};

  return make_model(&(Model_Create_Info){
    .gpu_allocator = gpu_allocator,
    .cpu_vertices =
        {
          [0] = (Vertex_Array){.items = vertices, .len = 24},
        },
    .cpu_indices =
        {
          [0] = (Index_Array){.items = indices, .len = 36},
        },
    .default_material = {[0] = default_material},
    .primitive_count = 1,
  });
}

#undef V