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
  Database_Error_Invalid_Resource_Handle,
  Database_Error_Resource_Capacity_Reached,
  Database_Error_Failed_To_Initialize,
  Database_Error_Failed_Alloc_Resource,
  Database_Error_Failed_Stream_Resource,
  Database_Error_Failed_To_Initialize_Resource,
} Database_Error;

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

typedef Result(Model *, Database_Error) Database_Model_Query;
typedef Result(Font_Atlas *, Database_Error) Database_Font_Query;
typedef Result(Material *, Database_Error) Database_Material_Query;
typedef Result(GPU_Texture *, Database_Error) Database_Texture_Query;

Database_Error init_database(Allocator allocator);
Database_Error destroy_database();

GPU_Shader_Group_Layout database_get_default_material_shader_group_layout(void);

Database_Model_Query database_get_stable_model(Model_Stable_ID id);
Database_Font_Query database_get_stable_font_atlas(Font_Stable_ID id);
Database_Material_Query database_get_stable_material(Material_Stable_ID id);
Database_Texture_Query database_get_stable_texture(Texture_Stable_ID id);

#endif