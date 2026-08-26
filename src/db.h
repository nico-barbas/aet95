#ifndef DB_H
#define DB_H

#include "core/types.h"
#include "font.h"
#include "render.h"

typedef enum Database_Error {
  Database_Error_None,
  Database_Error_Invalid_Resource_ID,
  Database_Error_Failed_Stream_Resource,
} Database_Error;

typedef enum Model_ID {
  Model_ID_Default_Cube,
  Model_ID_MAX,
} Model_ID;

// NOTE(nico): The convention is FONTNAME_FONTSIZE
typedef enum Font_ID {
  Font_ID_IBMPlex_Mono,
  Font_ID_MAX,
} Font_ID;

typedef enum Texture_ID {
  Texture_ID_MAX,
} Texture_ID;

typedef struct Database {
  Model model_table[Model_ID_MAX];
  Font_Atlas font_table[Font_ID_MAX];
} Database;

typedef Result(Font_Atlas_Entry *, Database_Error) Database_Font_Query;

extern Database _db;

bool32 init_database(Renderer *renderer, Allocator allocator);
Database_Font_Query database_get_font_atlas_entry(Font_ID id, f32 size);

#endif