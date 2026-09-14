#ifndef MATERIAL_H
#define MATERIAL_H

#include "core/platform.h"

typedef struct Material {
  u32 handle;
  GPU_Buffer_Memory gpu_uniforms;
  GPU_Texture gpu_albedo;
  GPU_Sampler gpu_sampler;
  GPU_Shader_Group_Data gpu_shader_data;
} Material;

#endif