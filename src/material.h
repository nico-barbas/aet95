#ifndef MATERIAL_H
#define MATERIAL_H

#include "core/map.h"
#include "core/platform.h"

// NOTE(nico): Right now the material model is fixed. It should still be generic
// enough for everything to work. It will use a very widely spread pbr material
// model

typedef enum Material_Error {
  Material_Error_None,
  Material_Error_Failed_To_Create_Cache,
  Material_Error_Failed_To_Create_Sampler,
  Material_Error_Failed_To_Create_Shader_Group_Data,
  Material_Error_Failed_To_Remove_From_Cache,
  Material_Error_Invalid_Handle,
  Material_Error_Invalid_Albedo_Texture,
} Material_Error;

typedef struct Material_Cache {
  Allocator allocator;
  GPU_Shader_Group_Layout layout;
  Open_Map materials;
} Material_Cache;

typedef u32 Material_Handle;

typedef struct Material {
  Material_Handle handle;
  GPU_Buffer_Memory gpu_uniforms; // Stubbed but not implemented yet

  GPU_Texture gpu_albedo;
  bool32 gpu_albedo_is_owned;

  GPU_Sampler gpu_sampler;               // Owned
  GPU_Shader_Group_Data gpu_shader_data; // Owned
} Material;

typedef struct Material_Create_Info {
  Material_Handle handle;
  GPU_Texture gpu_albedo;
  bool32 gpu_albedo_is_owned;
  GPU_Sampler_Filter gpu_sampler_filter;
  GPU_Sampler_Wrap gpu_sampler_wrap;
  // NOTE(nico): for now the sampler is shared accross all the textures of a
  // material
} Material_Create_Info;

typedef Result(Material_Handle, Material_Error) Material_Create_Result;

Material_Error init_material_cache(Material_Cache *cache, Allocator allocator);
void destroy_material_cache(Material_Cache *cache);

Material_Create_Result
material_cache_make_material(Material_Cache *cache, Material_Create_Info *info);
Material_Error
material_cache_destroy_material(Material_Cache *cache, Material_Handle handle);

#endif