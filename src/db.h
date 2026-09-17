#ifndef DB_H
#define DB_H

#include "core/platform.h"
#include "core/types.h"
#include "font.h"
#include "material.h"
#include "model.h"

typedef enum Database_Error : u32 {
  Database_Error_None,
  Database_Error_Internal_Failure,
  Database_Error_Invalid_Handle,
  Database_Error_Resource_Capacity_Reached,
  Database_Error_Failed_To_Initialize,
  Database_Error_Failed_Alloc_Resource,
  Database_Error_Failed_Stream_Resource,
  Database_Error_Failed_To_Initialize_Resource,
} Database_Error;

typedef enum Database_Resource_Kind {
  Database_Resource_Kind_Model,
  Database_Resource_Kind_Material,
  Database_Resource_Kind_Texture,
  Database_Resource_Kind_Font,
  Database_Resource_Kind_MAX,
} Database_Resource_Kind;

typedef enum Model_Stable_ID : u32 {
  Model_Stable_ID_Default_Cube,
} Model_Stable_ID;

typedef enum Font_Stable_ID {
  Font_Stable_ID_IBMPlex_Mono,
} Font_Stable_ID;

typedef enum Material_Stable_ID {
  Material_Stable_Id_Default,
} Material_Stable_ID;

typedef enum Texture_Stable_ID {
  Texture_Stable_ID_White,
} Texture_Stable_ID;

typedef Option(u32) Database_Stable_ID_Option;

typedef struct Database_Resource_Create_Info {
  Database_Resource_Kind kind;
  Database_Stable_ID_Option stable_id;
  bool32 skip_init;
  union {
    Model_Create_Info model;
    Material_Create_Info material;
    GPU_Texture_Create_Info texture;
    struct {
      String filepath;
    } font;
  };
} Database_Resource_Create_Info;

typedef Result(Model *, Database_Error) Database_Model_Query;
typedef Result(Material *, Database_Error) Database_Material_Query;
typedef Result(GPU_Texture *, Database_Error) Database_Texture_Query;
typedef Result(Font_Atlas *, Database_Error) Database_Font_Atlas_Query;

typedef Result(Gen_Handle, Database_Error) Database_Create_Result;

Database_Error init_database(Allocator allocator);
Database_Error destroy_database(void);

Gen_Handle_Option
database_lookup_stable_id(Database_Resource_Kind kind, u32 stable_id);
Database_Create_Result
database_create_resource(Database_Resource_Create_Info *info);
Database_Error detabase_destroy_resource(Gen_Handle handle);

// NOTE(nico): These can fail today, but move toward a never fail approach. If
// the requested resource doesn't exist, provide a fallback
Database_Model_Query database_query_model(Gen_Handle handle);
Database_Material_Query database_query_material(Gen_Handle handle);
Database_Texture_Query database_query_texture(Gen_Handle handle);
Database_Font_Atlas_Query database_query_font_atlas(Gen_Handle handle);

#endif