#ifndef DB_H
#define DB_H

#include "core/platform.h"
#include "core/types.h"
#include "font.h"
#include "material.h"
#include "model.h"
// #include "render.h"

typedef enum Database_Error {
  Database_Error_None,
  Database_Error_Failed_To_Initialize,
  Database_Error_Invalid_Resource_ID,
  Database_Error_Failed_Stream_Resource,
} Database_Error;

typedef enum Model_ID {
  Model_ID_Default_Cube,
  Model_ID_MAX,
} Model_ID;

typedef enum Font_ID {
  Font_ID_IBMPlex_Mono,
  Font_ID_MAX,
} Font_ID;

typedef enum Texture_ID {
  Texture_ID_MAX,
} Texture_ID;

typedef struct Database {
  Allocator allocator;

  // NOTE(nico): This is an arena for now. When we move to streaming and on
  // demand model uploading, I will write a free-list style gpu allocator
  GPU_Buffer gpu_allocator;

  Model model_table[Model_ID_MAX];
  Font_Atlas font_table[Font_ID_MAX];

  Material_Cache material_table;
} Database;

typedef Result(Font_Atlas_Entry *, Database_Error) Database_Font_Query;

extern Database _db;

Database_Error init_database(Allocator allocator);
Database_Font_Query database_get_font_atlas_entry(Font_ID id, f32 size);

#endif