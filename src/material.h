#ifndef MATERIAL_H
#define MATERIAL_H

#include "core/allocator.h"
#include "core/platform.h"

typedef enum Material_Error {
  Material_Error_None,
  Material_Error_Failed_To_Initialize_Layout,
  Material_Error_Failed_To_Create_Sampler,
  Material_Error_Failed_To_Create_Data,
  Material_Error_Invalid_Albedo_Texture,
} Material_Error;

typedef struct Material {
  GPU_Buffer_Memory gpu_uniforms; // Stubbed but not implemented yet

  GPU_Texture gpu_albedo;
  bool32 gpu_albedo_is_owned;

  GPU_Sampler gpu_sampler;               // Owned
  GPU_Shader_Group_Data gpu_shader_data; // Owned
} Material;

typedef struct Material_Create_Info {
  // Material_Handle handle;
  GPU_Texture gpu_albedo;
  bool32 gpu_albedo_is_owned;
  GPU_Sampler_Filter gpu_sampler_filter;
  GPU_Sampler_Wrap gpu_sampler_wrap;
  // NOTE(nico): for now the sampler is shared accross all the textures of a
  // material
} Material_Create_Info;

typedef Result(Material, Material_Error) Material_Create_Result;

Material_Error init_material_system(Allocator allocator);
void destroy_material_system(void);
GPU_Shader_Group_Layout get_material_default_shader_group_layout(void);

Material_Create_Result
make_material(Material_Create_Info *info, Allocator allocator);
Material_Error destroy_material(Material *material);

#endif