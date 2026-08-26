#include "render2d.h"

#include "core/allocator.h"
#include "core/array.h"
#include "core/math.h"
#include "core/platform.h"
#include "core/strings.h"
#include "core/types.h"

#include <assert.h>
#include <stddef.h>

////////////////////////////////////
// Renderer 2d
////////////////////////////////////
typedef struct Global_Storage_Data2D {
  Mat4 mat_proj_view;
} Global_Storage_Data2D;

static const char default_shader_2d[] = {
#embed "../assets/shaders/default_2d.wgsl"
  , '\0'
};

#define RENDERER2D_TRIANGLE_CAP 32768
#define RENDERER2D_VERTEX_CAP (RENDERER2D_TRIANGLE_CAP * 3)
#define RENDERER2D_INDEX_CAP (RENDERER2D_TRIANGLE_CAP * 3)

#define RENDERER2D_VERTEX_BUFFER_SIZE (RENDERER2D_VERTEX_CAP * sizeof(Vertex2D))
#define RENDERER2D_INDEX_BUFFER_SIZE (RENDERER2D_INDEX_CAP * sizeof(u32))
#define RENDERER2D_GEOMETRY_BUFFER_SIZE                                        \
  (RENDERER2D_VERTEX_BUFFER_SIZE + RENDERER2D_INDEX_BUFFER_SIZE)

void init_renderer_2d(
    Renderer2D *renderer,
    Font_ID font_id,
    String sprite_atlas_path,
    Allocator allocator
) {
  // NOTE(nico): This is extremly contrived, but the renderer is the one place
  // where it doesn't matter. I am not building an engine
  // One buffer for everything

  renderer->gpu_buffer = make_gpu_buffer(&(GPU_Buffer_Create_Info){
    .usage = GPU_Buffer_Usage_Copy_Dst | GPU_Buffer_Usage_Vertex |
             GPU_Buffer_Usage_Index | GPU_Buffer_Usage_Uniform,
    .size = RENDERER2D_GEOMETRY_BUFFER_SIZE + sizeof(Global_Storage_Data2D),
  });
  assert(gpu_buffer_is_valid(renderer->gpu_buffer));

  renderer->gpu_vertices =
      gpu_buffer_alloc(&renderer->gpu_buffer, RENDERER2D_VERTEX_BUFFER_SIZE);
  renderer->gpu_indices =
      gpu_buffer_alloc(&renderer->gpu_buffer, RENDERER2D_INDEX_BUFFER_SIZE);
  renderer->gpu_global_data =
      gpu_buffer_alloc(&renderer->gpu_buffer, sizeof(Global_Storage_Data2D));

  assert(
      gpu_buffer_memory_is_valid(renderer->gpu_vertices) &&
      gpu_buffer_memory_is_valid(renderer->gpu_indices) &&
      gpu_buffer_memory_is_valid(renderer->gpu_global_data)
  );

  byte white_pixel[4] = {255, 255, 255, 255};
  renderer->textures[Renderer2D_Atlas_Blank] =
      make_gpu_texture(&(GPU_Texture_Create_Info){
        .space = GPU_Texture_Space_sRGB,
        .kind = GPU_Texture_Create_Info_Raw_Memory,
        .raw = {.data = white_pixel, .width = 1, .height = 1, .channels = 4},
      });

  // NOTE(nico): temp code
  renderer->textures[Renderer2D_Atlas_Font] =
      _db.font_table[font_id].gpu_texture;

  // NOTE(nico): hack, but I want to get it to work
  if (string_equal(sprite_atlas_path, from_c_str(""))) {
    renderer->textures[Renderer2D_Atlas_Sprites] =
        make_gpu_texture(&(GPU_Texture_Create_Info){
          .space = GPU_Texture_Space_sRGB,
          .kind = GPU_Texture_Create_Info_Raw_Memory,
          .raw = {.data = white_pixel, .width = 1, .height = 1, .channels = 4},
        });
  } else {
    renderer->textures[Renderer2D_Atlas_Sprites] =
        make_gpu_texture(&(GPU_Texture_Create_Info){
          .space = GPU_Texture_Space_sRGB,
          .kind = GPU_Texture_Create_Info_File,
          .file_path = sprite_atlas_path,
        });
    ;
  }

  assert(
      gpu_texture_is_valid(renderer->textures[Renderer2D_Atlas_Blank]) &&
      gpu_texture_is_valid(renderer->textures[Renderer2D_Atlas_Font]) &&
      gpu_texture_is_valid(renderer->textures[Renderer2D_Atlas_Sprites])
  );

  renderer->sampler = make_gpu_sampler(&(GPU_Sampler_Create_Info){
    .filter = GPU_Sampler_Filter_Nearest,
    .wrap = GPU_Sampler_Wrap_Repeat,
  });

  GPU_Vertex_Attribute vertex_attrs[] = {
    VERTEX_ATTR_F32x2(Vertex2D, position, 0),
    VERTEX_ATTR_F32x3(Vertex2D, tex_coord, 1),
    VERTEX_ATTR_F32x4(Vertex2D, color, 2),
  };

  renderer->pipeline = make_gpu_pipeline(
      &(GPU_Pipeline_Create_Info){
        .shader_source =
            {
              .kind = GPU_Shader_Source_Raw,
              .data = from_c_str(default_shader_2d),
            },
        .vertex_attributes = vertex_attrs,
        .vertex_attribute_count = 3,
        .vertex_stride = sizeof(Vertex2D),
        .depth_test = false,
        .blend_state =
            &(GPU_Blend_State){
              .color =
                  {
                    .operation = GPU_Blend_Op_Add,
                    .src_factor = GPU_Blend_Factor_Src_Alpha,
                    .dst_factor = GPU_Blend_Factor_One_Minus_Src_Alpha,
                  },
              .alpha =
                  {
                    .operation = GPU_Blend_Op_Add,
                    .src_factor = GPU_Blend_Factor_One,
                    .dst_factor = GPU_Blend_Factor_One_Minus_Src_Alpha,
                  },
            },
        .bind_groups = ARRAY_LIT(
            GPU_Bind_Group_Create_Info,
            {
              .shader_data_infos = ARRAY_LIT(
                  GPU_Shader_Data_Info,
                  SHADER_UNIFORM(0, sizeof(Global_Storage_Data2D)),
                  SHADER_SAMPLER(1),
                  SHADER_TEXTURE(2),
                  SHADER_TEXTURE(3),
                  SHADER_TEXTURE(4),
              ),
            },
        )
      },
      allocator
  );
  assert(gpu_pipeline_is_valid(renderer->pipeline));

  renderer->bind_group = gpu_pipeline_derive_bind_group(
      renderer->pipeline,
      (GPU_Shader_Data_Source_Array)ARRAY_LIT(
          GPU_Shader_Data_Source,
          BIND_MEMORY(renderer->gpu_global_data),
          BIND_SAMPLER(renderer->sampler),
          BIND_TEXTURE(renderer->textures[Renderer2D_Atlas_Blank]),
          BIND_TEXTURE(renderer->textures[Renderer2D_Atlas_Font]),
          BIND_TEXTURE(renderer->textures[Renderer2D_Atlas_Sprites])
      ),
      0,
      allocator
  );

  renderer->cpu_vertices =
      make_array(renderer->cpu_vertices, RENDERER2D_VERTEX_CAP, allocator);
  renderer->cpu_indices =
      make_array(renderer->cpu_indices, RENDERER2D_INDEX_CAP, allocator);
  renderer->font_id = font_id;

  assert(
      renderer->cpu_vertices.items != nullptr &&
      renderer->cpu_indices.items != nullptr
  );
}

void destroy_renderer_2d(Renderer2D *renderer) {
  destroy_gpu_pipeline(renderer->pipeline);
  destroy_gpu_bind_group(renderer->bind_group);
  destroy_gpu_sampler(renderer->sampler);
  destroy_gpu_buffer(renderer->gpu_buffer);

  for (usize i = 0; i < Renderer2D_Atlas_MAX; i += 1) {
    destroy_gpu_texture(renderer->textures[i]);
  }

  delete_array(renderer->cpu_vertices);
  delete_array(renderer->cpu_indices);
}

void begin_render_2d(Renderer2D *renderer, f32 render_w, f32 render_h) {
  Global_Storage_Data2D globals = {
    .mat_proj_view = mat4_ortho(0.f, render_w, render_h, 0.f, 0.f, 1.f),
  };

  gpu_buffer_write(
      renderer->gpu_global_data, &globals, sizeof(Global_Storage_Data2D)
  );

  // FIXME(nico): this needs to be false since we are rendering right on top for
  // the screen framebuffer. Move to a offscreen target and then blit it on the
  // screen
  renderer->_active_pass = gpu_render_pass_begin(&(GPU_Render_Pass_Create_Info){
    .clear = false,
    // .clear_colors = {color(0, 0, 0, 0)},
  });

  gpu_render_pass_bind_pipeline(renderer->_active_pass, renderer->pipeline);
  gpu_render_pass_bind_group(renderer->_active_pass, renderer->bind_group);
}

void end_render_2d(Renderer2D *renderer) {
  gpu_buffer_write(
      renderer->gpu_vertices,
      renderer->cpu_vertices.items,
      renderer->cpu_vertex_count * sizeof(Vertex2D)
  );
  gpu_buffer_write(
      renderer->gpu_indices,
      renderer->cpu_indices.items,
      renderer->cpu_vertex_count * sizeof(u32)
  );

  gpu_render_pass_draw_indexed(
      renderer->_active_pass,
      renderer->gpu_vertices,
      renderer->gpu_indices,
      renderer->cpu_vertex_count,
      0
  );
  gpu_render_pass_end(renderer->_active_pass);
  renderer->cpu_vertex_count = 0;
}

void draw_char(
    Renderer2D *renderer, char c, Vec2 origin, f32 size, Color color
) {
  // NOTE(nico): It sucks a lot to have to do a db query for 1 char.. but oh
  // well. This is temporary and the larger 2d renderer rewrite will solve this
  // issue with the batch info
  Database_Font_Query font_query =
      database_get_font_atlas_entry(renderer->font_id, size);
  if (!font_query.ok) {
    // NOTE(nico): Should draw magenta quads to signify a failed font entry load
    return;
  }

  Font_Atlas_Entry *entry = font_query.value;
  f32 x = origin.x;
  f32 y = origin.y + entry->ascent;

  Font_Glyph_Option glyph_opt = font_atlas_entry_get_glyph(entry, (utf8_char)c);
  if (!glyph_opt.some) {
    return;
  }

  Font_Glyph glyph = glyph_opt.value;
  if (glyph.dimensions.x > 0 && glyph.dimensions.y > 0) {
    draw_quad(
        renderer,
        (Rectangle){
          .x = x + glyph.offset.x,
          .y = y + glyph.offset.y,
          .width = glyph.dimensions.x,
          .height = glyph.dimensions.y
        },
        Renderer2D_Atlas_Font,
        (Rectangle){
          .x = glyph.bound_min.x,
          .y = glyph.bound_min.y,
          .width = glyph.bound_max.x - glyph.bound_min.x,
          .height = glyph.bound_max.y - glyph.bound_min.y,
        },
        0.f,
        color
    );
  }
}

// FIXME(nico): No support for utf8
void draw_text(
    Renderer2D *renderer, String text, Vec2 origin, f32 size, Color color
) {
  Database_Font_Query font_query =
      database_get_font_atlas_entry(renderer->font_id, size);
  if (!font_query.ok) {
    // NOTE(nico): Should draw magenta quads to signify a failed font entry load
    return;
  }

  Font_Atlas_Entry *entry = font_query.value;
  f32 x = origin.x;
  f32 y = origin.y + entry->ascent;

  for (usize i = 0; i < text.len; i += 1) {
    char c = text.data[i];
    if (c == '\n') {
      x = origin.x;
      y += entry->line_height;
    }

    Font_Glyph_Option glyph_opt =
        font_atlas_entry_get_glyph(entry, (utf8_char)c);
    if (!glyph_opt.some) {
      continue;
    }

    Font_Glyph glyph = glyph_opt.value;
    if (glyph.dimensions.x > 0 && glyph.dimensions.y > 0) {
      draw_quad(
          renderer,
          (Rectangle){
            .x = x + glyph.offset.x,
            .y = y + glyph.offset.y,
            .width = glyph.dimensions.x,
            .height = glyph.dimensions.y
          },
          Renderer2D_Atlas_Font,
          (Rectangle){
            .x = glyph.bound_min.x,
            .y = glyph.bound_min.y,
            .width = glyph.bound_max.x - glyph.bound_min.x,
            .height = glyph.bound_max.y - glyph.bound_min.y,
          },
          0.f,
          color
      );
    }

    x += glyph.advance;
  }
}

void draw_rect(Renderer2D *renderer, Rectangle rect, Color color) {
  draw_quad(
      renderer,
      rect,
      Renderer2D_Atlas_Blank,
      (Rectangle){0, 0, 1, 1},
      0.f,
      color
  );
}

void draw_quad(
    Renderer2D *renderer,
    Rectangle rect,
    Renderer2D_Atlas id,
    Rectangle src_rect,
    f32 rotation,
    Color color
) {
  GPU_Texture texture = renderer->textures[id];
  f32 w = (f32)texture.width;
  f32 h = (f32)texture.height;

  f32 texture_index = (f32)id;
  Vec2 uv_min = vec2(src_rect.x / w, src_rect.y / h);
  Vec2 uv_max = vec2(
      (src_rect.x + src_rect.width) / w, (src_rect.y + src_rect.height) / h
  );

  Vertex2D tl = {
    .tex_coord = vec3(uv_min.x, uv_min.y, texture_index),
    .color = color,
  };
  Vertex2D tr = {
    .tex_coord = vec3(uv_max.x, uv_min.y, texture_index),
    .color = color,
  };
  Vertex2D bl = {
    .tex_coord = vec3(uv_min.x, uv_max.y, texture_index),
    .color = color,
  };
  Vertex2D br = {
    .tex_coord = vec3(uv_max.x, uv_max.y, texture_index),
    .color = color,
  };

  if (rotation == 0.f) {
    tl.position = vec2(rect.x, rect.y);
    tr.position = vec2(rect.x + rect.width, rect.y);
    bl.position = vec2(rect.x, rect.y + rect.height);
    br.position = vec2(rect.x + rect.width, rect.y + rect.height);
  } else {
    assert(false);
    // NOTE(nico): fuck that, I don't need it for now
  }

  draw_triangle(renderer, tl, tr, bl);
  draw_triangle(renderer, tr, br, bl);
}

void draw_triangle(
    Renderer2D *renderer, Vertex2D v0, Vertex2D v1, Vertex2D v2
) {
  if (renderer->cpu_vertex_count + 3 > RENDERER2D_VERTEX_CAP) {
    // TODO(nico): handle this gracefully. Can't be asked right now
    assert(false);
  }

  usize i = renderer->cpu_vertex_count;

  array_set(renderer->cpu_vertices, i, v0);
  array_set(renderer->cpu_vertices, i + 1, v1);
  array_set(renderer->cpu_vertices, i + 2, v2);

  array_set(renderer->cpu_indices, i, (u32)i);
  array_set(renderer->cpu_indices, i + 1, (u32)i + 1);
  array_set(renderer->cpu_indices, i + 2, (u32)i + 2);

  renderer->cpu_vertex_count += 3;
}