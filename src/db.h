#ifndef DB_H
#define DB_H

#include "core/types.h"
#include "font.h"
#include "render.h"

typedef enum Model_ID {
  Model_ID_Default_Cube,
  Model_ID_MAX,
} Model_ID;

// NOTE(nico): The convention is FONTNAME_FONTSIZE
typedef enum Font_ID {
  Font_ID_IBM_Default,
  Font_ID_MAX,
} Font_ID;

typedef enum Texture_ID {
  Texture_ID_MAX,
} Texture_ID;

typedef struct Database {
  Model model_table[Model_ID_MAX];
  Font_Atlas font_table[Font_ID_MAX];
} Database;

extern Database _db;

bool32 init_database(Renderer *renderer, Allocator allocator);

#endif