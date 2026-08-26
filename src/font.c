#include "font.h"

#include "core/allocator.h"
#include "core/array.h"
#include "core/io.h"
#include "core/math.h"
#include "core/platform.h"
#include "core/runtime.h"
#include "core/types.h"
#include "stb_truetype.h"

#include <string.h>

// NOTE(nico): very contrived but should be enough for most font size
#define ATLAS_DIMENSION 2048
#define CPU_TEXTURE_SIZE (sizeof(byte) * ATLAS_DIMENSION * ATLAS_DIMENSION)

// NOTE(nico): It cited in the header that I want to support utf-8 but until
// that is the case, this will hold
#define FIRST_ASCII_CODEPOINT 32
#define LAST_ASCII_CODEPOINT 128
#define ASCII_CODEPOINT_COUNT (LAST_ASCII_CODEPOINT - FIRST_ASCII_CODEPOINT)

Font_Error
init_font_atlas_from_file(Font_Atlas *font, String path, Allocator allocator) {
  Font_Error error = Font_Error_None;
  i32 stbtt_ok = 0;

  font->allocator = allocator;
  font->first_codepoint = FIRST_ASCII_CODEPOINT;
  font->last_codepoint = LAST_ASCII_CODEPOINT;
  font->cpu_raw_data = or_return(
      read_entire_file(path, false, allocator), Font_Error_Failed_To_Read_File
  );

  Allocation_Result bitmap_alloc = allocator.alloc(allocator, CPU_TEXTURE_SIZE);
  if (bitmap_alloc.err != Allocation_Error_None) {
    return Font_Error_Failed_To_Alloc_Data;
  }
  font->cpu_texture = (byte *)bitmap_alloc.allocation;
  memset(font->cpu_texture, 0, CPU_TEXTURE_SIZE);

  Allocation_Result rgba_alloc =
      allocator.alloc(allocator, CPU_TEXTURE_SIZE * 4);
  if (rgba_alloc.err != Allocation_Error_None) {
    allocator.free(allocator, font->cpu_texture);
    return Font_Error_Failed_To_Alloc_Data;
  }
  font->cpu_rgba_texture = rgba_alloc.allocation;

  Font_Atlas_Cache_Make_Result cache_result =
      make_font_atlas_cache(32, allocator);
  if (!cache_result.ok) {
    allocator.free(allocator, font->cpu_texture);
    allocator.free(allocator, font->cpu_rgba_texture);
    return Font_Error_Failed_To_Alloc_Data;
  }
  font->cache = cache_result.value;

  stbtt_ok = stbtt_PackBegin(
      &font->pack_ctx,
      font->cpu_texture,
      ATLAS_DIMENSION,
      ATLAS_DIMENSION,
      0,
      1,
      nullptr
  );
  if (!stbtt_ok) {
    allocator.free(allocator, font->cpu_texture);
    allocator.free(allocator, font->cpu_rgba_texture);
    delete_font_atlas_cache(&font->cache);
    return Font_Error_Failed_To_Load_Font;
  }

  font->gpu_texture = make_gpu_texture(&(GPU_Texture_Create_Info){
    .space = GPU_Texture_Space_sRGB,
    .kind = GPU_Texture_Create_Info_Empty,
    .empty = {
      .width = ATLAS_DIMENSION,
      .height = ATLAS_DIMENSION,
      .channels = 4,
    },
  });
  if (!gpu_texture_is_valid(font->gpu_texture)) {
    allocator.free(allocator, font->cpu_texture);
    allocator.free(allocator, font->cpu_rgba_texture);
    delete_font_atlas_cache(&font->cache);
    stbtt_PackEnd(&font->pack_ctx);
    return Font_Error_Failed_To_Load_Font;
  }

  return error;
}

// NOTE(nico): The allocator passed here is a temp allocator for all the scratch
// data. It will try to free so there is no leak
Font_Atlas_Entry_Load_Result
font_atlas_load_font_size(Font_Atlas *font, f32 size, Allocator allocator) {
  for (usize i = 0; i < font->cache.len; i += 1) {
    Font_Atlas_Entry *entry = &font->cache.items[i];
    if (entry->size == size) {
      return ok(Font_Atlas_Entry_Load_Result, entry);
    }
  }

  Font_Atlas_Entry entry = {0};
  entry.size = size;
  entry.first_codepoint = font->first_codepoint;
  entry.last_codepoint = font->last_codepoint;

  // NOTE(nico): can probably cache this entire allocation in the font atlas
  Allocation_Result packed_char_alloc = allocator.alloc(
      allocator, sizeof(stbtt_packedchar) * ASCII_CODEPOINT_COUNT
  );
  if (packed_char_alloc.err != Allocation_Error_None) {
    return err(
        Font_Atlas_Entry_Load_Result, Font_Error_Failed_To_Load_Font_Size
    );
  }
  defer {
    allocator.free(allocator, packed_char_alloc.allocation);
  };

  stbtt_packedchar *packed_chars =
      (stbtt_packedchar *)packed_char_alloc.allocation;

  i32 stbtt_ok = stbtt_PackFontRange(
      &font->pack_ctx,
      font->cpu_raw_data,
      0,
      size,
      FIRST_ASCII_CODEPOINT,
      ASCII_CODEPOINT_COUNT,
      packed_chars
  );
  if (!stbtt_ok) {
    return err(
        Font_Atlas_Entry_Load_Result, Font_Error_Failed_To_Load_Font_Size
    );
  }

  for (usize i = 0; i < ATLAS_DIMENSION * ATLAS_DIMENSION; i += 1) {
    font->cpu_rgba_texture[i * 4 + 0] = 255;
    font->cpu_rgba_texture[i * 4 + 1] = 255;
    font->cpu_rgba_texture[i * 4 + 2] = 255;
    font->cpu_rgba_texture[i * 4 + 3] = font->cpu_texture[i];
  }

  stbtt_fontinfo font_info;
  if (!stbtt_InitFont(&font_info, font->cpu_raw_data, 0)) {
    return err(
        Font_Atlas_Entry_Load_Result, Font_Error_Failed_To_Load_Font_Size
    );
  }

  f32 scale = stbtt_ScaleForPixelHeight(&font_info, size);
  i32 raw_ascent = 0;
  i32 raw_descent = 0;
  i32 raw_line_gap = 0;
  stbtt_GetFontVMetrics(&font_info, &raw_ascent, &raw_descent, &raw_line_gap);

  entry.ascent = (f32)raw_ascent * scale;
  entry.descent = (f32)raw_descent * scale;
  entry.line_gap = (f32)raw_line_gap * scale;
  entry.line_height = entry.ascent - entry.descent + entry.line_gap;

  entry.glyphs =
      make_array(entry.glyphs, ASCII_CODEPOINT_COUNT, font->allocator);
  if (entry.glyphs.items == nullptr) {
    return err(
        Font_Atlas_Entry_Load_Result, Font_Error_Failed_To_Load_Font_Size
    );
  }

  entry.max_advance = -1000.f;
  for (usize i = 0; i < ASCII_CODEPOINT_COUNT; i += 1) {
    stbtt_packedchar *pc = &packed_chars[i];

    array_set(
        entry.glyphs,
        i,
        ((Font_Glyph){
          .bound_min = vec2(pc->x0, pc->y0),
          .bound_max = vec2(pc->x1, pc->y1),
          .dimensions = vec2(pc->xoff2 - pc->xoff, pc->yoff2 - pc->yoff),
          .offset = vec2(pc->xoff, pc->yoff),
          .advance = pc->xadvance,
        })
    );
    entry.max_advance = max_f32(entry.max_advance, pc->xadvance);
  }

  Font_Atlas_Cache_Push_Result push_result =
      font_atlas_cache_push(&font->cache, &entry);
  if (!push_result.ok) {
    return err(
        Font_Atlas_Entry_Load_Result, Font_Error_Failed_To_Load_Font_Size
    );
  };

  bool32 write_ok = gpu_texture_write(
      font->gpu_texture,
      &(GPU_Texture_Write_Info){
        .data = font->cpu_rgba_texture,
        .width = ATLAS_DIMENSION,
        .height = ATLAS_DIMENSION,
      }
  );
  if (!write_ok) {
    return err(
        Font_Atlas_Entry_Load_Result, Font_Error_Failed_To_Load_Font_Size
    );
  }

  return ok(
      Font_Atlas_Entry_Load_Result, &font->cache.items[push_result.value]
  );
}

void destroy_font_atlas(Font_Atlas *font) {
  for (usize i = 0; i < font->cache.len; i += 1) {
    delete_array(font->cache.items[i].glyphs);
  }

  font->allocator.free(font->allocator, font->cpu_texture);
  font->allocator.free(font->allocator, font->cpu_rgba_texture);
  delete_font_atlas_cache(&font->cache);
  stbtt_PackEnd(&font->pack_ctx);
  destroy_gpu_texture(font->gpu_texture);
}

Font_Atlas_Entry_Ptr_Option font_atlas_get_entry(Font_Atlas *font, f32 size) {
  for (usize i = 0; i < font->cache.len; i += 1) {
    Font_Atlas_Entry *entry = &font->cache.items[i];
    if (entry->size == size) {
      return some(Font_Atlas_Entry_Ptr_Option, entry);
    }
  }

  return none(Font_Atlas_Entry_Ptr_Option);
}

Font_Glyph_Option
font_atlas_entry_get_glyph(Font_Atlas_Entry *entry, utf8_char c) {
  if (c < entry->first_codepoint || c >= entry->last_codepoint) {
    return none(Font_Glyph_Option);
  }

  u32 index = (u32)c - entry->first_codepoint;
  return some(Font_Glyph_Option, array_get(entry->glyphs, index));
}

Vec2 font_atlas_entry_measure_text(Font_Atlas_Entry *entry, String text) {
  f32 w = 0.f;
  f32 h = entry->ascent;

  f32 current_w = 0.f;
  for (usize i = 0; i < text.len; i += 1) {
    char c = text.data[i];
    if (c == '\n') {
      w = max_f32(w, current_w);
      h += entry->line_height;
      current_w = 0.f;
    }

    Font_Glyph_Option glyph_opt =
        font_atlas_entry_get_glyph(entry, (utf8_char)c);
    if (!glyph_opt.some) {
      continue;
    }

    Font_Glyph glyph = glyph_opt.value;
    current_w += glyph.advance;
  }

  return vec2(max_f32(w, current_w), h);
}