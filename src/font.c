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
#define ATLAS_DIMENSION 512
#define SCRATCH_BITMAP_SIZE (sizeof(byte) * ATLAS_DIMENSION * ATLAS_DIMENSION)

// NOTE(nico): It cited in the header that I want to support utf-8 but until
// that is the case, this will hold
#define FIRST_ASCII_CODEPOINT 32
#define LAST_ASCII_CODEPOINT 128
#define ASCII_CODEPOINT_COUNT (LAST_ASCII_CODEPOINT - FIRST_ASCII_CODEPOINT)

Font_Atlas_Create_Result
make_font_atlas_from_file(String path, f32 size, Allocator allocator) {
  Font_Atlas font = {0};

  byte *file_data = or_return(
      read_entire_file(path, false, allocator),
      err(Font_Atlas_Create_Result, Font_Error_None)
  );
  defer {
    allocator.free(allocator, file_data);
  };

  Allocation_Result bitmap_alloc =
      allocator.alloc(allocator, SCRATCH_BITMAP_SIZE);
  if (bitmap_alloc.err != Allocation_Error_None) {
    return err(Font_Atlas_Create_Result, Font_Error_Failed_To_Alloc_Temp_Data);
  }
  defer {
    allocator.free(allocator, bitmap_alloc.allocation);
  };

  byte *bitmap = (byte *)bitmap_alloc.allocation;
  memset(bitmap, 0, SCRATCH_BITMAP_SIZE);

  Allocation_Result packed_char_alloc = allocator.alloc(
      allocator, sizeof(stbtt_packedchar) * ASCII_CODEPOINT_COUNT
  );
  if (packed_char_alloc.err != Allocation_Error_None) {
    return err(Font_Atlas_Create_Result, Font_Error_Failed_To_Alloc_Temp_Data);
  }
  defer {
    allocator.free(allocator, packed_char_alloc.allocation);
  };

  stbtt_packedchar *packed_chars =
      (stbtt_packedchar *)packed_char_alloc.allocation;
  stbtt_pack_context pack_ctx;
  // NOTE(nico): I hate this fuckery, but it is necessary for the defer to work
  // properly
  stbtt_pack_context *pack_ctx_ptr = &pack_ctx;

  i32 stbtt_ok = stbtt_PackBegin(
      &pack_ctx, bitmap, ATLAS_DIMENSION, ATLAS_DIMENSION, 0, 1, nullptr
  );
  if (!stbtt_ok) {
    return err(Font_Atlas_Create_Result, Font_Error_Failed_To_Read_Font);
  }
  defer {
    stbtt_PackEnd(pack_ctx_ptr);
  };

  stbtt_ok = stbtt_PackFontRange(
      &pack_ctx,
      file_data,
      0,
      size,
      FIRST_ASCII_CODEPOINT,
      ASCII_CODEPOINT_COUNT,
      packed_chars
  );
  if (!stbtt_ok) {
    return err(Font_Atlas_Create_Result, Font_Error_Failed_To_Read_Font);
  }

  Allocation_Result rgba_alloc =
      allocator.alloc(allocator, SCRATCH_BITMAP_SIZE * 4);
  if (rgba_alloc.err != Allocation_Error_None) {
    return err(Font_Atlas_Create_Result, Font_Error_Failed_To_Load_Font);
  }
  defer {
    allocator.free(allocator, rgba_alloc.allocation);
  };

  byte *rgba = rgba_alloc.allocation;
  for (usize i = 0; i < ATLAS_DIMENSION * ATLAS_DIMENSION; i += 1) {
    rgba[i * 4 + 0] = 255;
    rgba[i * 4 + 1] = 255;
    rgba[i * 4 + 2] = 255;
    rgba[i * 4 + 3] = bitmap[i];
  }

  // Now the only problem is cleanup on error path of non-scoped resources
  font.gpu_texture = make_gpu_texture(&(GPU_Texture_Create_Info){
    .space = GPU_Texture_Space_sRGB,
    .kind = GPU_Texture_Create_Info_Raw_Memory,
    .raw = {.data = rgba, .width = 1, .height = 1, .channels = 4},
  });
  if (!gpu_texture_is_valid(font.gpu_texture)) {
    return err(Font_Atlas_Create_Result, Font_Error_Failed_To_Load_Font);
  }

  stbtt_fontinfo font_info;
  if (!stbtt_InitFont(&font_info, file_data, 0)) {
    return err(Font_Atlas_Create_Result, Font_Error_Failed_To_Load_Font);
  }

  f32 scale = stbtt_ScaleForPixelHeight(&font_info, size);
  i32 raw_ascent = 0;
  i32 raw_descent = 0;
  i32 raw_line_gap = 0;
  stbtt_GetFontVMetrics(&font_info, &raw_ascent, &raw_descent, &raw_line_gap);

  font.ascent = (f32)raw_ascent * scale;
  font.descent = (f32)raw_descent * scale;
  font.line_gap = (f32)raw_line_gap * scale;
  font.line_height = font.ascent - font.descent + font.line_gap;

  font.glyphs = make_array(font.glyphs, ASCII_CODEPOINT_COUNT, allocator);
  if (font.glyphs.items == nullptr) {
    return err(Font_Atlas_Create_Result, Font_Error_Failed_To_Load_Font);
  }

  for (usize i = 0; i < ASCII_CODEPOINT_COUNT; i += 1) {
    stbtt_packedchar *pc = &packed_chars[i];

    array_set(
        font.glyphs,
        i,
        ((Font_Glyph){
          .bound_min = vec2(pc->x0, pc->y0),
          .bound_max = vec2(pc->x1, pc->y1),
          .dimensions = vec2(pc->xoff2 - pc->xoff, pc->yoff2 - pc->yoff),
          .offset = vec2(pc->xoff, pc->yoff),
          .advance = pc->xadvance,
        })
    );
  }

  return ok(Font_Atlas_Create_Result, font);
}

Font_Glyph_Option font_atlas_get_glyph(Font_Atlas *font, utf8_char c) {
  if (c < font->first_codepoint || c >= font->last_codepoint) {
    return none(Font_Glyph_Option);
  }

  u32 index = (u32)c - font->first_codepoint;
  return some(Font_Glyph_Option, array_get(font->glyphs, index));
}