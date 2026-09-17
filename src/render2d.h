#ifndef RENDER2D_H
#define RENDER2D_H

#include "core/allocator.h"
#include "core/array.h"
#include "core/map.h"
#include "core/math.h"
#include "core/platform.h"
#include "db.h"
#include "render.h"

#define RENDERER_2D_BATCH_CAP 64

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

// [26-08-2026]: Start of the 2d refactor
// The idea is to separate the renderer states and the bind groups needed for a
// pass. Every begin and end require a Renderer2D_Batch_Info holding the bind
// group, the font used, and the textures. This still isn't the best but a lot
// more flexible than whatever this mess is. It would need a batch info cache,
// and a way to hash the combination of the 3 textures (their handles?). One of
// the goals is also to remove the dependency to the Renderer_2D when creating
// resources. Maybe move the resource ownership into the db like it's meant to
// be. The Renderer_2D should only own the geometry gpu memory and the pipeline
//
// The only problem I can see before implementing it is the restriction on the
// font size. This is a massive pain to write the game's ui and puts a lot of
// restrictions and fuckery on the UI command drain.
// One solution would be to go with SDF, but I've never implemented it and idk
// how much work that imposes on every part of the application
////////////////////////////////////

// I fucking hate having to write descriptor pools. Fuck modern API. Might have
// even be better to go with modern Vulkan versions to have bindless

typedef struct Vertex2D {
  Vec2 position;
  Vec3 tex_coord;
  Color color;
} Vertex2D;

typedef enum Renderer_2D_Atlas {
  Renderer_2D_Atlas_Blank,
  Renderer_2D_Atlas_Sprite,
  Renderer_2D_Atlas_Font,
  Renderer_2D_Atlas_MAX,
} Renderer_2D_Atlas;

typedef struct Renderer_2D_Batch {
  u64 batch_group_data_handle;
  usize offset;
  usize len;
} Renderer_2D_Batch;

typedef struct Renderer_2D {
  Allocator allocator;
  Render_Resource_Interface it;

  GPU_Buffer gpu_buffer;
  GPU_Buffer_Memory gpu_vertices;
  GPU_Buffer_Memory gpu_indices;
  GPU_Buffer_Memory gpu_global_data;

  Font_Stable_ID font_id;
  Array(Vertex2D) cpu_vertices;
  Array(u32) cpu_indices;
  usize cpu_vertex_count;

  GPU_Shader_Layout layout;
  GPU_Pipeline pipeline;

  GPU_Shader_Group_Layout global_group_layout;
  GPU_Shader_Group_Data global_group_data;

  GPU_Shader_Group_Layout batch_group_layout;

  GPU_Sampler sampler;
  // NOTE(nico): We need 2 things. One is the current state of the texture used
  // and a cached list of possible texture combination
  u64 default_batch_group_data_handle;
  u64 current_batch_group_data_handle;
  u64 current_handles[Renderer_2D_Atlas_MAX];
  Renderer_2D_Batch batches[RENDERER_2D_BATCH_CAP];
  usize batch_count;
  Open_Map group_cache;
  struct {
    Font_Atlas *font;
    f32 width;
    f32 height;
  } cached_info[Renderer_2D_Atlas_MAX];

  GPU_Render_Pass _active_pass;
} Renderer_2D;

typedef struct Renderer_2D_Create_Info {
  Render_Resource_Interface it;
  u64 blank_texture_handle;
  u64 font_handle;
  u64 sprite_handle;
} Renderer_2D_Create_Info;

void init_renderer_2d(
    Renderer_2D *renderer, Renderer_2D_Create_Info *info, Allocator allocator
);
void destroy_renderer_2d(Renderer_2D *renderer);

void begin_render_2d(Renderer_2D *renderer, f32 render_w, f32 render_h);
void end_render_2d(Renderer_2D *renderer);

void renderer_2d_set_atlas_texture(
    Renderer_2D *renderer, Renderer_2D_Atlas target, u64 resource_handle
);

void renderer_2d_draw_char(
    Renderer_2D *renderer, char c, Vec2 origin, f32 size, Color color
);
void renderer_2d_draw_text(
    Renderer_2D *renderer, String text, Vec2 origin, f32 size, Color color
);
void renderer_2d_draw_rect(Renderer_2D *renderer, Rectangle rect, Color color);
void renderer_2d_draw_rect_outline(
    Renderer_2D *renderer, Rectangle rect, f32 thickness, Color color
);
void renderer_2d_draw_quad(
    Renderer_2D *renderer,
    Rectangle rect,
    Renderer_2D_Atlas target,
    Rectangle src_rect,
    f32 rotation,
    Color color
);
void renderer_2d_draw_triangle(
    Renderer_2D *renderer, Vertex2D v0, Vertex2D v1, Vertex2D v2
);

#endif