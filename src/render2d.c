#include "render2d.h"

#include "core/allocator.h"
#include "core/array.h"
#include "core/map.h"
#include "core/math.h"
#include "core/platform.h"
#include "core/strings.h"
#include "core/types.h"
#include "font.h"
#include "render.h"

#include <assert.h>
#include <stddef.h>

////////////////////////////////////
// Renderer 2d
////////////////////////////////////
typedef struct Global_Storage_Data_2D {
  Mat4 mat_proj_view;
} Global_Storage_Data_2D;

static const char default_shader_2d[] = {
#embed "../assets/shaders/default_2d.wgsl"
  , '\0'
};

#define RENDERER_2D_TRIANGLE_CAP 32768
#define RENDERER_2D_VERTEX_CAP (RENDERER_2D_TRIANGLE_CAP * 3)
#define RENDERER_2D_INDEX_CAP (RENDERER_2D_TRIANGLE_CAP * 3)

#define RENDERER_2D_VERTEX_BUFFER_SIZE                                         \
  (RENDERER_2D_VERTEX_CAP * sizeof(Vertex2D))
#define RENDERER_2D_INDEX_BUFFER_SIZE (RENDERER_2D_INDEX_CAP * sizeof(u32))
#define RENDERER_2D_GEOMETRY_BUFFER_SIZE                                       \
  (RENDERER_2D_VERTEX_BUFFER_SIZE + RENDERER_2D_INDEX_BUFFER_SIZE)

static u64 hash_renderer_2d_state(Renderer_2D *renderer) {
  u64 hash = FNV1A_INITIAL_SEED;
  for (usize i = 0; i < Renderer_2D_Atlas_MAX; i += 1) {
    hash = hash_fnv1a_stream(&renderer->current_handles[i], sizeof(u64), hash);
  }

  return hash;
}

static void renderer_2d_push_batch(Renderer_2D *renderer) {
  if (renderer->batch_count > 0) {
    renderer->batches[renderer->batch_count - 1].len =
        renderer->cpu_vertex_count -
        renderer->batches[renderer->batch_count - 1].offset;
  }

  renderer->batches[renderer->batch_count++] = (Renderer_2D_Batch){
    .batch_group_data_handle = renderer->current_batch_group_data_handle,
    .offset = renderer->cpu_vertex_count,
    .len = 0,
  };
}

static bool32
renderer_2d_refresh_current_batch_group_data(Renderer_2D *renderer) {
  Texture_Option blank_texture = query_texture(
      renderer->it, renderer->current_handles[Renderer_2D_Atlas_Blank]
  );
  Font_Atlas_Option font = query_font_atlas(
      renderer->it, renderer->current_handles[Renderer_2D_Atlas_Font]
  );
  Texture_Option sprite_texture = query_texture(
      renderer->it, renderer->current_handles[Renderer_2D_Atlas_Sprite]
  );

  if (!blank_texture.some || !font.some || !sprite_texture.some) {
    return false;
  }

  renderer->cached_info[Renderer_2D_Atlas_Blank].width =
      (f32)blank_texture.value->width;
  renderer->cached_info[Renderer_2D_Atlas_Blank].height =
      (f32)blank_texture.value->height;
  renderer->cached_info[Renderer_2D_Atlas_Sprite].width =
      (f32)sprite_texture.value->width;
  renderer->cached_info[Renderer_2D_Atlas_Sprite].height =
      (f32)sprite_texture.value->height;
  renderer->cached_info[Renderer_2D_Atlas_Font].font = font.value;
  renderer->cached_info[Renderer_2D_Atlas_Font].width =
      (f32)font.value->gpu_texture.width;
  renderer->cached_info[Renderer_2D_Atlas_Font].height =
      (f32)font.value->gpu_texture.width;

  if (open_map_get(
          renderer->group_cache, renderer->current_batch_group_data_handle
      ) == nullptr) {
    GPU_Shader_Group_Data batch_group_layout =
        unwrap(make_gpu_shader_group_data(
            &(GPU_Shader_Group_Data_Create_Info){
              .layout = renderer->batch_group_layout,
              .binds = ARRAY_LIT(
                  GPU_Shader_Bind_Data_Create_Info,
                  BIND_SAMPLER(renderer->sampler),
                  BIND_TEXTURE(*blank_texture.value),
                  BIND_TEXTURE(*sprite_texture.value),
                  BIND_TEXTURE(font.value->gpu_texture),
              )
            },
            renderer->allocator
        ));
    open_map_set(
        renderer->group_cache,
        renderer->current_batch_group_data_handle,
        batch_group_layout
    );
  }

  return true;
}

void init_renderer_2d(
    Renderer_2D *renderer, Renderer_2D_Create_Info *info, Allocator allocator
) {
  renderer->allocator = allocator;
  renderer->it = info->it;
  renderer->gpu_buffer = make_gpu_buffer(&(GPU_Buffer_Create_Info){
    .usage = GPU_Buffer_Usage_Copy_Dst | GPU_Buffer_Usage_Vertex |
             GPU_Buffer_Usage_Index | GPU_Buffer_Usage_Uniform,
    .size = RENDERER_2D_GEOMETRY_BUFFER_SIZE + sizeof(Global_Storage_Data_2D),
  });
  assert(gpu_buffer_is_valid(renderer->gpu_buffer));

  renderer->gpu_vertices =
      gpu_buffer_alloc(&renderer->gpu_buffer, RENDERER_2D_VERTEX_BUFFER_SIZE);
  renderer->gpu_indices =
      gpu_buffer_alloc(&renderer->gpu_buffer, RENDERER_2D_INDEX_BUFFER_SIZE);
  renderer->gpu_global_data =
      gpu_buffer_alloc(&renderer->gpu_buffer, sizeof(Global_Storage_Data_2D));

  assert(
      gpu_buffer_memory_is_valid(renderer->gpu_vertices) &&
      gpu_buffer_memory_is_valid(renderer->gpu_indices) &&
      gpu_buffer_memory_is_valid(renderer->gpu_global_data)
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

  renderer->global_group_layout = unwrap(make_gpu_shader_group_layout(
      &(GPU_Shader_Group_Layout_Create_Info){
        .binds = ARRAY_LIT(
            GPU_Shader_Bind_Info, SHADER_UNIFORM(sizeof(Global_Storage_Data_2D))
        )
      },
      allocator
  ));

  renderer->global_group_data = unwrap(make_gpu_shader_group_data(
      &(GPU_Shader_Group_Data_Create_Info){
        .layout = renderer->global_group_layout,
        .binds = ARRAY_LIT(
            GPU_Shader_Bind_Data_Create_Info,
            BIND_MEMORY(renderer->gpu_global_data)
        ),
      },
      allocator
  ));

  renderer->batch_group_layout = unwrap(make_gpu_shader_group_layout(
      &(GPU_Shader_Group_Layout_Create_Info){
        .binds = ARRAY_LIT(
            GPU_Shader_Bind_Info,
            SHADER_SAMPLER(),
            SHADER_TEXTURE(),
            SHADER_TEXTURE(),
            SHADER_TEXTURE()
        ),
      },
      allocator
  ));

  renderer->layout =
      unwrap(make_gpu_shader_layout(&(GPU_Shader_Layout_Create_Info){
        .groups = ARRAY_LIT(
            GPU_Shader_Group_Layout,
            renderer->global_group_layout,
            renderer->batch_group_layout
        ),
      }));

  renderer->pipeline = unwrap(make_gpu_pipeline(
      &(GPU_Pipeline_Create_Info){
        .shader_source =
            {
              .kind = GPU_Shader_Source_Raw,
              .data = from_cstring(default_shader_2d),
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
        .layout = renderer->layout,
      },
      allocator
  ));

  renderer->cpu_vertices =
      make_array(renderer->cpu_vertices, RENDERER_2D_VERTEX_CAP, allocator);
  renderer->cpu_indices =
      make_array(renderer->cpu_indices, RENDERER_2D_INDEX_CAP, allocator);

  assert(
      renderer->cpu_vertices.items != nullptr &&
      renderer->cpu_indices.items != nullptr
  );

  renderer->group_cache = make_u64_open_map(
      GPU_Shader_Group_Data, RENDERER_2D_BATCH_CAP, allocator
  );
  assert(renderer->group_cache != nullptr);

  // TODO(nico): need to have a default shader group data
  renderer->current_handles[Renderer_2D_Atlas_Blank] =
      info->blank_texture_handle;
  renderer->current_handles[Renderer_2D_Atlas_Font] = info->font_handle;
  renderer->current_handles[Renderer_2D_Atlas_Sprite] = info->sprite_handle;
  renderer->default_batch_group_data_handle = hash_renderer_2d_state(renderer);
  renderer->current_batch_group_data_handle =
      renderer->default_batch_group_data_handle;

  bool32 refresh_ok = renderer_2d_refresh_current_batch_group_data(renderer);
  assert(refresh_ok);
}

void destroy_renderer_2d(Renderer_2D *renderer) {
  destroy_gpu_pipeline(renderer->pipeline);
  destroy_gpu_shader_layout(renderer->layout);
  destroy_gpu_shader_group_layout(renderer->global_group_layout);
  destroy_gpu_shader_group_data(renderer->global_group_data);
  destroy_gpu_sampler(renderer->sampler);
  destroy_gpu_buffer(renderer->gpu_buffer);

  delete_array(renderer->cpu_vertices);
  delete_array(renderer->cpu_indices);

  Open_Map_Iterator it = open_map_iterator(renderer->group_cache);
  while (open_map_has_next(&it)) {
    GPU_Shader_Group_Data *data = open_map_next(&it);
    destroy_gpu_shader_group_data(*data);
  }

  delete_open_map(renderer->group_cache);
}

void begin_render_2d(Renderer_2D *renderer, f32 render_w, f32 render_h) {
  Global_Storage_Data_2D globals = {
    .mat_proj_view = mat4_ortho(0.f, render_w, render_h, 0.f, 0.f, 1.f),
  };

  gpu_buffer_write(
      renderer->gpu_global_data, &globals, sizeof(Global_Storage_Data_2D)
  );

  // FIXME(nico): this needs to be false since we are rendering right on top for
  // the screen framebuffer. Move to a offscreen target and then blit it on the
  // screen
  renderer->_active_pass = gpu_render_pass_begin(&(GPU_Render_Pass_Create_Info){
    .clear = false,
    // .clear_colors = {color(0, 0, 0, 0)},
  });
  renderer->batch_count = 0;
  renderer_2d_push_batch(renderer);

  gpu_render_pass_bind_pipeline(renderer->_active_pass, renderer->pipeline);
  gpu_render_pass_bind_group(
      renderer->_active_pass, renderer->global_group_data, 0
  );
}

void end_render_2d(Renderer_2D *renderer) {
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

  if (renderer->batch_count > 0) {
    renderer->batches[renderer->batch_count - 1].len =
        renderer->cpu_vertex_count -
        renderer->batches[renderer->batch_count - 1].offset;
  }
  for (usize i = 0; i < renderer->batch_count; i += 1) {
    Renderer_2D_Batch *batch = &renderer->batches[i];

    GPU_Buffer_Memory gpu_index_slice = {
      .buffer = renderer->gpu_indices.buffer,
      .offset = renderer->gpu_indices.offset + batch->offset * sizeof(u32),
      .size = batch->len * sizeof(u32)
    };

    GPU_Shader_Group_Data *group =
        open_map_get(renderer->group_cache, batch->batch_group_data_handle);
    assert(group != nullptr);
    gpu_render_pass_bind_group(renderer->_active_pass, *group, 1);

    gpu_render_pass_draw_indexed(
        renderer->_active_pass,
        renderer->gpu_vertices,
        gpu_index_slice,
        batch->len,
        0
    );
  }
  gpu_render_pass_end(renderer->_active_pass);
  renderer->cpu_vertex_count = 0;
}

void renderer_2d_set_atlas_texture(
    Renderer_2D *renderer, Renderer_2D_Atlas target, u64 resource_handle
) {
  if (renderer->current_handles[target] == resource_handle) {
    return;
  }

  renderer->current_handles[target] = resource_handle;
  renderer->current_batch_group_data_handle = hash_renderer_2d_state(renderer);
  bool32 refresh_ok = renderer_2d_refresh_current_batch_group_data(renderer);
  assert(refresh_ok);

  renderer_2d_push_batch(renderer);
}

void renderer_2d_draw_char(
    Renderer_2D *renderer, char c, Vec2 origin, f32 size, Color color
) {
  Font_Atlas_Entry_Ptr_Option entry_opt = font_atlas_get_entry(
      renderer->cached_info[Renderer_2D_Atlas_Font].font, size
  );
  if (!entry_opt.some) {
    assert(false);
    return;
  }

  Font_Atlas_Entry *entry = entry_opt.value;
  f32 x = origin.x;
  f32 y = origin.y + entry->ascent;

  Font_Glyph_Option glyph_opt = font_atlas_entry_get_glyph(entry, (utf8_char)c);
  if (!glyph_opt.some) {
    return;
  }

  Font_Glyph glyph = glyph_opt.value;
  if (glyph.dimensions.x > 0 && glyph.dimensions.y > 0) {
    renderer_2d_draw_quad(
        renderer,
        (Rectangle){
          .x = x + glyph.offset.x,
          .y = y + glyph.offset.y,
          .width = glyph.dimensions.x,
          .height = glyph.dimensions.y
        },
        Renderer_2D_Atlas_Font,
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
void renderer_2d_draw_text(
    Renderer_2D *renderer, String text, Vec2 origin, f32 size, Color color
) {
  Font_Atlas_Entry_Ptr_Option entry_opt = font_atlas_get_entry(
      renderer->cached_info[Renderer_2D_Atlas_Font].font, size
  );
  if (!entry_opt.some) {
    assert(false);
    return;
  }

  Font_Atlas_Entry *entry = entry_opt.value;
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
      renderer_2d_draw_quad(
          renderer,
          (Rectangle){
            .x = x + glyph.offset.x,
            .y = y + glyph.offset.y,
            .width = glyph.dimensions.x,
            .height = glyph.dimensions.y
          },
          Renderer_2D_Atlas_Font,
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

void renderer_2d_draw_rect(Renderer_2D *renderer, Rectangle rect, Color color) {
  renderer_2d_draw_quad(
      renderer,
      rect,
      Renderer_2D_Atlas_Blank,
      (Rectangle){0, 0, 1, 1},
      0.f,
      color
  );
}

void renderer_2d_draw_rect_outline(
    Renderer_2D *renderer, Rectangle rect, f32 thickness, Color color
) {
  // NOTE(nico): the outline is drawn inside the rect's bounds. The vertical
  // edges are inset so that the corners aren't covered twice, which would
  // blend on top of each other with a translucent color
  f32 t = min_f32(thickness, min_f32(rect.width, rect.height) * 0.5f);
  if (t <= 0.f) {
    return;
  }

  renderer_2d_draw_rect(
      renderer,
      (Rectangle){.x = rect.x, .y = rect.y, .width = rect.width, .height = t},
      color
  );
  renderer_2d_draw_rect(
      renderer,
      (Rectangle){
        .x = rect.x,
        .y = rect.y + rect.height - t,
        .width = rect.width,
        .height = t,
      },
      color
  );
  renderer_2d_draw_rect(
      renderer,
      (Rectangle){
        .x = rect.x,
        .y = rect.y + t,
        .width = t,
        .height = rect.height - t * 2.f,
      },
      color
  );
  renderer_2d_draw_rect(
      renderer,
      (Rectangle){
        .x = rect.x + rect.width - t,
        .y = rect.y + t,
        .width = t,
        .height = rect.height - t * 2.f,
      },
      color
  );
}

void renderer_2d_draw_quad(
    Renderer_2D *renderer,
    Rectangle rect,
    Renderer_2D_Atlas target,
    Rectangle src_rect,
    f32 rotation,
    Color color
) {
  f32 w = renderer->cached_info[target].width;
  f32 h = renderer->cached_info[target].height;

  f32 texture_index = (f32)target;
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

  renderer_2d_draw_triangle(renderer, tl, tr, bl);
  renderer_2d_draw_triangle(renderer, tr, br, bl);
}

void renderer_2d_draw_triangle(
    Renderer_2D *renderer, Vertex2D v0, Vertex2D v1, Vertex2D v2
) {
  if (renderer->cpu_vertex_count + 3 > RENDERER_2D_VERTEX_CAP) {
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