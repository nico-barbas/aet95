#ifndef FONT_H
#define FONT_H

#include "core/array.h"
#include "core/math.h"
#include "core/platform.h"
#include "core/types.h"

/*
  NOTE(nico):
  Rasterized font implementation.
  SDF Planned with a BIG maybe. It's probably not worth the time spent on it.
  Will depends on how comfortable the text is. Since it is a programming game,
  this will matter a lot for quality

  I think I also want to support utf-8

  What is still in the air is the support for a font cache. Seeing as the
  renderer is very restrictive by design, there isn't any point for now.
  Any solution to support multi-size fonts require a texture packer, or some gpu
  shenanigans that I'm too lazy to implement: texture arrays of the same size,
  bindless texture array that isn't supported by WebGPU anyway (do I care?)
*/

typedef enum Font_Error {
  Font_Error_None,
  Font_Error_Failed_To_Read_File,
  Font_Error_Failed_To_Alloc_Temp_Data,
  Font_Error_Failed_To_Read_Font,
  Font_Error_Failed_To_Load_Font,
} Font_Error;

typedef struct Font_Glyph {
  Vec2 bound_min;
  Vec2 bound_max;
  Vec2 dimensions;
  Vec2 offset;
  f32 advance;
} Font_Glyph;

typedef Option(Font_Glyph) Font_Glyph_Option;

// NOTE(nico): Fuck x11 for forcing this naming
typedef struct Font_Atlas {
  GPU_Texture gpu_texture;
  Array(Font_Glyph) glyphs;
  u32 first_codepoint;
  u32 last_codepoint;
  f32 pixel_height;
  f32 ascent;
  f32 descent;
  f32 line_gap;
  f32 line_height;
  f32 max_advance;
} Font_Atlas;

typedef Result(Font_Atlas, Font_Error) Font_Atlas_Create_Result;

// NOTE(nico): Too lazy to write a _from_memory variant. So no #embed for now
// for the fonts
Font_Atlas_Create_Result
make_font_atlas_from_file(String path, f32 size, Allocator allocator);

Font_Glyph_Option font_atlas_get_glyph(Font_Atlas *font, utf8_char c);
Vec2 font_atlas_measure_texture(Font_Atlas *font, String text);

#endif