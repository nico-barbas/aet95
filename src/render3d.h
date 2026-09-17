#ifndef RENDER3D_H
#define RENDER3D_H

#include "core/allocator.h"
#include "core/array.h"
#include "core/camera.h"
#include "core/math.h"
#include "core/platform.h"
#include "model.h"

/////////////////////////////////////
// Actual rendering
/////////////////////////////////////

typedef struct Instance_Data {
  Mat4 transform;
  Mat4 normal;
  Color color;
} Instance_Data;

typedef struct Renderer {
  // Render_Resource_Interface it;

  GPU_Texture depth_texture; // FIXME(nico): we'll use a offscreen target, so
                             // this need doesn't exists
  // GPU_Buffer geometry_buffer;
  GPU_Buffer storage_buffer;
  GPU_Buffer_Memory gpu_global_data;
  GPU_Buffer_Memory gpu_instances_data;

  GPU_Shader_Layout default_layout;
  GPU_Pipeline default_pipeline;

  GPU_Shader_Group_Layout global_group_layout;
  GPU_Shader_Group_Data global_group_data;
  // Material_Handle default_material;

  GPU_Render_Pass _active_pass;

  // Runtime states
  // Open_Map texture_cache;
  Array(Instance_Data) instances_data;
  usize instance_count;
} Renderer;

void init_renderer(
    Renderer *renderer,
    // Render_Resource_Interface it,
    i32 render_w,
    i32 render_h,
    Allocator allocator
);
void destroy_renderer(Renderer *renderer);

void begin_render_3d(Renderer *renderer, Raw_Camera *camera);
void end_render_3d(Renderer *renderer);

void draw_model(Renderer *renderer, Model_Draw_Info *info);
void draw_mesh_primitive(Renderer *renderer, Mesh_Primitive_Draw_Info *info);

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

  GPU_Shader_Layout layout;
  GPU_Pipeline pipeline;

  GPU_Shader_Group_Layout global_group_layout;
  GPU_Shader_Group_Data global_group_data;
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