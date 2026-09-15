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
  Database_Error_Invalid_Resource_ID,
  Database_Error_Resource_Capacity_Reached,
  Database_Error_Failed_To_Initialize,
  Database_Error_Failed_Alloc_Resource,
  Database_Error_Failed_Stream_Resource,
} Database_Error;

// typedef enum Model_ID {
//   Model_ID_Default_Cube,
//   Model_ID_MAX,
// } Model_ID;

typedef enum Font_ID {
  Font_ID_IBMPlex_Mono,
  Font_ID_MAX,
} Font_ID;

// typedef enum Texture_ID {
//   Texture_ID_MAX,
// } Texture_ID;

typedef enum Database_Resource_Kind {
  Database_Resource_Kind_Model,
  Database_Resource_Kind_Font,
  Database_Resource_Kind_Texture,
} Database_Resource_Kind;

typedef struct Database_Resource_Handle {
  u32 id;
  u32 generation;
} Database_Resource_Handle;

typedef struct Database_Resource {
  u32 backing_index;
  u32 slot_index;

  Database_Resource_Kind kind;
  enum Database_Resource_Status {
    Database_Resource_Status_Empty,
    Database_Resource_Status_Loading,
    Database_Resource_Status_Ready,
    Database_Resource_Status_Failed,
  } status;
  rawptr ptr;
} Database_Resource;

typedef struct Database_Resource_Slot {
  u32 generation;
  u32 packed;
} Database_Resource_Slot;

// #define LIST_TYPE Resource
// #define LIST_TYPE_NAME Resource_List
// #define LIST_FUNCTION_PREFIX resource_list
// #include "core/list.h"

#define RESOURCE_CAP 512

typedef struct Database {
  Allocator allocator;

  // NOTE(nico): This is an arena for now. When we move to streaming and on
  // demand model uploading, I will write a free-list style gpu allocator
  GPU_Buffer gpu_allocator;

  Database_Resource resources[RESOURCE_CAP];
  Database_Resource_Slot table[RESOURCE_CAP];
  usize count;
  usize cap;

  Open_Map stable_id_lookup;
  Material_Cache material_cache;

  // Model model_table[Model_ID_MAX];
  // Font_Atlas font_table[Font_ID_MAX];

} Database;

typedef Result(Font_Atlas_Entry *, Database_Error) Database_Font_Query;

extern Database _db;

Database_Error init_database(Allocator allocator);
Database_Font_Query database_get_font_atlas_entry(Font_ID id, f32 size);

#endif