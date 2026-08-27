#ifndef RENDER2D_H
#define RENDER2D_H

#include "core/allocator.h"
#include "core/array.h"
#include "core/math.h"
#include "core/platform.h"
#include "db.h"

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
// the goals is also to remove the dependency to the Renderer2D when creating
// resources. Maybe move the resource ownership into the db like it's meant to
// be. The Renderer2D should only own the geometry gpu memory and the pipeline
//
// The only problem I can see before implementing it is the restriction on the
// font size. This is a massive pain to write the game's ui and puts a lot of
// restrictions and fuckery on the UI command drain.
// One solution would be to go with SDF, but I've never implemented it and idk
// how much work that imposes on every part of the application
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

  Font_ID font_id;
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
    Font_ID font_id,
    String sprite_atlas_path,
    Allocator allocator
);
void destroy_renderer_2d(Renderer2D *renderer);

void begin_render_2d(Renderer2D *renderer, f32 render_w, f32 render_h);
void end_render_2d(Renderer2D *renderer);

void draw_char(
    Renderer2D *renderer, char c, Vec2 origin, f32 size, Color color
);
void draw_text(
    Renderer2D *renderer, String text, Vec2 origin, f32 size, Color color
);
void draw_rect(Renderer2D *renderer, Rectangle rect, Color color);
void draw_rect_outline(
    Renderer2D *renderer, Rectangle rect, f32 thickness, Color color
);
void draw_quad(
    Renderer2D *renderer,
    Rectangle rect,
    Renderer2D_Atlas id,
    Rectangle src_rect,
    f32 rotation,
    Color color
);
void draw_triangle(Renderer2D *renderer, Vertex2D v0, Vertex2D v1, Vertex2D v2);

#endif