#ifndef MATERIAL_H
#define MATERIAL_H

#include "core/map.h"
#include "core/platform.h"

typedef enum Material_Error {
  Material_Error_None,
  Material_Error_Failed_To_Create_Cache,
} Material_Error;

typedef struct Material_Cache {
  GPU_Shader_Group_Layout layout;
  Open_Map materials;
} Material_Cache;

typedef struct Material {
  u32 handle;
  GPU_Buffer_Memory gpu_uniforms;
  GPU_Texture gpu_albedo;
  GPU_Sampler gpu_sampler;
  GPU_Shader_Group_Data gpu_shader_data;
} Material;

Material_Error init_material_cache(Material_Cache *cache, Allocator allocator);
void destroy_material_cache(Material_Cache *cache);

#endif