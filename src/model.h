#ifndef MODEL_H
#define MODEL_H

#include "cgltf_parser.h"
#include "core/physics.h"
#include "core/platform.h"
#include "material.h"

#define MESH_PRIMITIVE_CAP 16

typedef enum Model_Error {
  Model_Error_None,
  Model_Error_Invalid_Data,
} Model_Error;

// NOTE(nico): until we have a more defined gpu allocator type, pass the gpu
// buffer in the mesh/model create info BY POINTER (this sucks).. But only
// temporary

// NOTE(nico): Defined once, this is a bit rigid. If the problem ever arises
// from one consumer, I will make it more generic
typedef struct Vertex {
  Vec4 position;
  Vec4 normal;
  Vec2 tex_coord;
} Vertex;

typedef Array(Vertex) Vertex_Array;
typedef Array(u32) Index_Array;

typedef struct Mesh_Primitive {
  GPU_Buffer_Memory gpu_vertices;
  GPU_Buffer_Memory gpu_indices;
  usize index_count;
  AABB_Collider collider;
} Mesh_Primitive;

typedef struct Model {
  Mesh_Primitive primitives[MESH_PRIMITIVE_CAP];
  Material_Handle default_materials[MESH_PRIMITIVE_CAP];
  AABB_Collider collider;
  usize primitive_count;
} Model;

typedef struct Mesh_Primitive_Create_Info {
  GPU_Buffer *gpu_allocator;
  Vertex_Array vertices;
  Index_Array indices;
} Mesh_Primitive_Create_Info;

typedef struct Mesh_Primitive_Update_Info {
  Vertex_Array vertices;
  Index_Array indices;
} Mesh_Primitive_Update_Info;

typedef struct Mesh_Primitive_Draw_Info {
  Mesh_Primitive primitive;
  Material_Handle material_handle;
  Mat4 transform;
  Color color;
} Mesh_Primitive_Draw_Info;

typedef struct Model_Create_Info {
  GPU_Buffer *gpu_allocator;
  usize primitive_count;
  Vertex_Array cpu_vertices[MESH_PRIMITIVE_CAP];
  Index_Array cpu_indices[MESH_PRIMITIVE_CAP];
  Material_Handle default_material[MESH_PRIMITIVE_CAP];
} Model_Create_Info;

typedef struct Model_Draw_Info {
  Model *model;
  Mat4 transform;
  Color color;
  Option(Material_Handle) material_handles[MESH_PRIMITIVE_CAP];
} Model_Draw_Info;

typedef Result(Model, Model_Error) Model_Create_Result;
typedef Result(Mesh_Primitive, Model_Error) Mesh_Primitive_Create_Result;

Mesh_Primitive_Create_Result
make_mesh_primitive(Mesh_Primitive_Create_Info *info);
Model_Error destroy_mesh_primitive(Mesh_Primitive *primitive);

Model_Create_Result make_model(Model_Create_Info *info);
Model_Error destroy_model(Model *model);
Model_Create_Result
make_cube_model(GPU_Buffer *gpu_allocator, Material_Handle default_material);
Model_Create_Result
make_plane_model(GPU_Buffer *gpu_allocator, Material_Handle default_material);

#endif