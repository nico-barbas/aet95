#ifndef FONT_H
#define FONT_H

#include "core/array.h"
#include "core/math.h"
#include "core/platform.h"
#include "core/types.h"
#include "stb_truetype.h"

/*
  NOTE(nico):
  Rasterized font implementation.
  I think I also want to support utf-8
*/

typedef enum Font_Error {
  Font_Error_None,
  Font_Error_Failed_To_Read_File,
  Font_Error_Failed_To_Alloc_Data,
  Font_Error_Failed_To_Load_Font,
  Font_Error_Failed_To_Load_Font_Size,
} Font_Error;

typedef struct Font_Glyph {
  Vec2 bound_min;
  Vec2 bound_max;
  Vec2 dimensions;
  Vec2 offset;
  f32 advance;
} Font_Glyph;

typedef struct Font_Atlas_Entry {
  Array(Font_Glyph) glyphs;
  f32 size;
  f32 ascent;
  f32 descent;
  f32 line_gap;
  f32 line_height;
  f32 max_advance;
  u32 first_codepoint;
  u32 last_codepoint;
} Font_Atlas_Entry;

#define LIST_TYPE Font_Atlas_Entry
#define LIST_TYPE_NAME Font_Atlas_Cache
#define LIST_FUNCTION_PREFIX font_atlas_cache
#include "core/list.h"

// NOTE(nico): Fuck x11 for forcing this naming
typedef struct Font_Atlas {
  Allocator allocator;

  GPU_Texture gpu_texture;

  stbtt_pack_context pack_ctx;
  byte *cpu_raw_data;
  byte *cpu_texture;
  byte *cpu_rgba_texture;
  Font_Atlas_Cache cache;
  u32 first_codepoint;
  u32 last_codepoint;
} Font_Atlas;

typedef Result(Font_Atlas_Entry *, Font_Error) Font_Atlas_Entry_Load_Result;
typedef Option(Font_Atlas_Entry *) Font_Atlas_Entry_Ptr_Option;
typedef Option(Font_Glyph) Font_Glyph_Option;

// NOTE(nico): Too lazy to write a _from_memory variant. So no #embed for now
// for the fonts
Font_Error
init_font_atlas_from_file(Font_Atlas *font, String path, Allocator allocator);
void destroy_font_atlas(Font_Atlas *font);

Font_Atlas_Entry_Load_Result
font_atlas_load_font_size(Font_Atlas *font, f32 size, Allocator allocator);
Font_Atlas_Entry_Ptr_Option font_atlas_get_entry(Font_Atlas *font, f32 size);

Font_Glyph_Option
font_atlas_entry_get_glyph(Font_Atlas_Entry *entry, utf8_char c);
Vec2 font_atlas_entry_measure_text(Font_Atlas_Entry *entry, String text);

#endif