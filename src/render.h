#ifndef RENDER_H
#define RENDER_H

#include "font.h"
#include "material.h"

typedef Option(Font_Atlas *) Font_Atlas_Option;
typedef Option(Material *) Material_Option;
typedef Option(GPU_Texture *) Texture_Option;

typedef struct Render_Resource_Interface Render_Resource_Interface;
struct Render_Resource_Interface {
  rawptr data;

  Font_Atlas_Option (*query_font_atlas_proc)(
      Render_Resource_Interface it, u64 handle
  );
  Material_Option (*query_material_proc)(
      Render_Resource_Interface it, u64 handle
  );
  Texture_Option (*query_texture_proc)(
      Render_Resource_Interface it, u64 handle
  );
};

#define query_material(it, handle) ((it).query_material_proc(it, handle))
#define query_font_atlas(it, handle) ((it).query_font_atlas_proc(it, handle))
#define query_texture(it, handle) ((it).query_texture_proc(it, handle))

#endif