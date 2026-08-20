#ifndef RENDER_H
#define RENDER_H

#include "core/allocator.h"
#include "core/array.h"
#include "core/camera.h"
#include "core/map.h"
#include "core/math.h"
#include "core/platform.h"
#include "font.h"

#define MESH_PRIMITIVE_CAP 16

// Reserved material handle for the built-in flat-white material. Real handles
// are fnv1a hashes of glTF material names, so 0 is safe to reserve.
#define DEFAULT_MATERIAL_HANDLE 0u

/////////////////////////////////////
// Actual rendering
/////////////////////////////////////
/*
  TODO(nico): The renderer is pretty barebone at the moment. Planned features:
    - Batch draw calls per model to allow instancing
    - Batch per material to reduce bind group swaps
    - Render to an offscreen target
    - Do either a depth pre-pass or a full g-buffer
*/
typedef struct Vertex {
  Vec4 position;
  Vec4 normal;
  Vec2 tex_coord;
} Vertex;

typedef Array(Vertex) Vertex_Array;
typedef Array(u32) Index_Array;

typedef struct Instance_Data {
  Mat4 transform;
  Mat4 normal;
  Color color;
} Instance_Data;

typedef struct Material {
  u32 handle;
  GPU_Buffer_Memory gpu_uniforms;
  GPU_Texture map_albedo;
  GPU_Sampler sampler;
  GPU_Bind_Group bind_group;
} Material;

typedef enum Material_Create_Error {
  Material_Create_Error_None,
} Material_Create_Error;

typedef Result(Material, Material_Create_Error) Material_Create_Result;
typedef Option(Material) Material_Option;

typedef struct Mesh_Primitive {
  GPU_Buffer_Memory gpu_vertices;
  GPU_Buffer_Memory gpu_indices;
  usize index_count;
  u32 material_handle;
  AABB_Collider collider;
} Mesh_Primitive;

typedef struct Mesh_Primitive_Create_Info {
  Vertex_Array vertices;
  Index_Array indices;
  Material *material;
} Mesh_Primitive_Create_Info;

// NOTE(nico): This is very similar to the corresponding create info. This is
// mostly for type correctness reason and clearer semantics. The material update
// isn't available for now
typedef struct Mesh_Primitive_Update_Info {
  Vertex_Array vertices;
  Index_Array indices;
} Mesh_Primitive_Update_Info;

typedef struct Mesh_Primitive_Draw_Info {
  Mesh_Primitive primitive;
  Mat4 transform;
  Color color;
} Mesh_Primitive_Draw_Info;

typedef enum Mesh_Primitive_Create_Error {
  Mesh_Primitive_Create_Error_None,
} Mesh_Primitive_Create_Error;

typedef Result(
    Mesh_Primitive, Mesh_Primitive_Create_Error
) Mesh_Primitive_Create_Result;

typedef struct Model {
  Mesh_Primitive primitives[MESH_PRIMITIVE_CAP];
  AABB_Collider collider;
  usize primitive_count;
} Model;

typedef struct Model_Create_Info {
  void *gltf_data;
  String root_path;
  String model_name;
} Model_Create_Info;

typedef struct Model_Draw_Info {
  Model model;
  Mat4 transform;
  Color color;
} Model_Draw_Info;

typedef enum Model_Create_Error {
  Model_Create_Error_None,
  Model_Create_Error_Emtpy_GLTF_File,
} Model_Create_Error;

typedef Result(Model, Model_Create_Error) Model_Create_Result;

typedef struct Renderer {
  GPU_Texture depth_texture; // FIXME(nico): we'll use a offscreen target, so
                             // this need doesn't exists
  GPU_Buffer geometry_buffer;
  GPU_Buffer storage_buffer;
  GPU_Buffer_Memory gpu_global_data;
  GPU_Buffer_Memory gpu_instances_data;

  GPU_Pipeline default_pipeline;
  GPU_Bind_Group global_bind_group;
  Material default_material;

  GPU_Render_Pass _active_pass;

  // Runtime states
  Open_Map texture_cache;
  Open_Map material_cache;
  Array(Instance_Data) instances_data;
  usize instance_count;
} Renderer;

void init_renderer(
    Renderer *renderer, i32 render_w, i32 render_h, Allocator allocator
);
void destroy_renderer(Renderer *renderer);

void begin_render(Renderer *renderer, Raw_Camera *camera);
void end_render(Renderer *renderer);

void draw_model(Renderer *renderer, Model_Draw_Info *info);
void draw_mesh_primitive(Renderer *renderer, Mesh_Primitive_Draw_Info *info);

Model_Create_Result model_make_cube(Renderer *renderer, Material *material);
Model_Create_Result model_make_plane(Renderer *renderer, Material *material);

Model_Create_Result model_load_from_geometry(
    Renderer *renderer,
    Vertex_Array vertices,
    Index_Array indices,
    Material *material
);
Model_Create_Result model_load_gltf_from_file(
    Renderer *renderer,
    Model_Create_Info *info,
    Allocator allocator,
    Allocator temp_allocator
);

// NOTE(nico): this granularity allows for arbitrarily owning a chunk of gpu
// memory at runtime without having to handle all the low-level plumbing.
// It exposes the initial upload and the update at any point if the allocated
// memory is large enough.
// This may seem superfluous but it prevents leaking the gpu abstraction in
// gameplay code, which is debatable but cleaner to read
Mesh_Primitive_Create_Result mesh_primitive_load_from_geometry(
    Renderer *renderer, Mesh_Primitive_Create_Info *info
);
bool32 mesh_primitive_update_from_geometry(
    Mesh_Primitive *primitive, Mesh_Primitive_Update_Info *info
);

////////////////////////////////////
// Renderer 2d
// This is a VERY BAD implementation
// but it should be enough to render
// all the UI in the game.
//
// The code quality is very low and it could be improve a lot. The initial idea
// of having a struct owning all the gpu states for a given pass is not bad.
// Creating a flexible data structure with user supplied textures and shaders
// should be good enough for 99% of the use cases. The only problem is shader
// structure and uniform and how to do it without exposing the entire plumbings?
////////////////////////////////////
typedef struct Vertex2D {
  Vec2 position;
  Vec3 tex_coord;
  Color color;
} Vertex2D;

typedef enum Renderer2D_Atlas {
  Renderer2D_Atlas_Blank,
  Renderer2D_Atlas_Font,
  Renderer2D_Atlas_Sprites,
  Renderer2D_Atlas_MAX,
} Renderer2D_Atlas;

typedef struct Renderer2D {
  GPU_Buffer gpu_buffer;
  GPU_Buffer_Memory gpu_vertices;
  GPU_Buffer_Memory gpu_indices;
  GPU_Buffer_Memory gpu_global_data;

  Font_Atlas font;
  Array(Vertex2D) cpu_vertices;
  Array(u32) cpu_indices;
  usize cpu_vertex_count;

  GPU_Pipeline pipeline;
  GPU_Bind_Group bind_group;
  GPU_Sampler sampler;
  GPU_Texture textures[Renderer2D_Atlas_MAX];

  GPU_Render_Pass _active_pass;
} Renderer2D;

void init_renderer_2d(
    Renderer2D *renderer,
    Font_Atlas font,
    String sprite_atlas_path,
    Allocator allocator
);
void destroy_renderer_2d(Renderer2D *renderer);

void begin_render_2d(Renderer2D *renderer, f32 render_w, f32 render_h);
void end_render_2d(Renderer2D *renderer);

void draw_rect(Renderer2D *renderer, Rectangle rect, Color color);
void draw_quad(
    Renderer2D *renderer,
    Rectangle rect,
    Renderer2D_Atlas id,
    Rectangle src_rect,
    f32 rotation,
    Color color
);
void draw_triangle(Renderer2D *renderer, Vertex2D v0, Vertex2D v1, Vertex2D v2);

#if defined DEBUG
typedef struct Debug_Vertex {
  Vec3 position;
  u32 color;
} Debug_Vertex;

typedef struct Debug_Renderer {
  GPU_Buffer geometry_buffer;
  GPU_Buffer storage_buffer;
  GPU_Buffer_Memory gpu_global_data;
  GPU_Buffer_Memory gpu_vertices;

  GPU_Pipeline pipeline;
  GPU_Bind_Group global_bind_group;
  GPU_Render_Pass _active_pass;

  Array(Debug_Vertex) cpu_vertices;
  usize vertex_count;
} Debug_Renderer;

void init_debug_renderer(
    Debug_Renderer *renderer, i32 render_w, i32 render_h, Allocator allocator
);
void destroy_debug_renderer(Debug_Renderer *renderer);

void begin_debug_render(
    Debug_Renderer *renderer, Raw_Camera *camera, GPU_Texture *depth_texture
);
void end_debug_render(Debug_Renderer *renderer);

void draw_debug_line(
    Debug_Renderer *renderer, Vec3 start, Vec3 end, Color color
);
void draw_debug_cube(Debug_Renderer *renderer, Vec3 min, Vec3 max, Color color);
void draw_debug_obb(
    Debug_Renderer *renderer, Vec3 center, Quat rotation, Vec3 half, Color c
);
void draw_debug_camera_frustum(
    Debug_Renderer *renderer, Raw_Camera *camera, Color color
);
#endif

#endif