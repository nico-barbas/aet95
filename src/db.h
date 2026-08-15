#ifndef DB_H
#define DB_H

#include "core/types.h"
#include "render.h"

typedef enum Model_ID {
  Model_ID_Default_Cube,
  Model_ID_MAX,
} Model_ID;

typedef struct Database {
  Model model_table[Model_ID_MAX];
} Database;

extern Database _db;

bool32 init_database(Renderer *renderer);

#endif