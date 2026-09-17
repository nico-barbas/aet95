#include "material.h"

#include "core/allocator.h"
#include "core/array.h"
#include "core/platform.h"
#include "core/runtime.h"

static GPU_Shader_Group_Layout default_layout = {0};

Material_Error init_material_system(Allocator allocator) {
  GPU_Shader_Group_Layout_Create_Result result = make_gpu_shader_group_layout(
      &(GPU_Shader_Group_Layout_Create_Info){
        .binds =
            ARRAY_LIT(GPU_Shader_Bind_Info, SHADER_TEXTURE(), SHADER_SAMPLER()),
      },
      allocator
  );

  if (!result.ok) {
    return Material_Error_Failed_To_Initialize_Layout;
  }

  default_layout = result.value;
  return Material_Error_None;
}

void destroy_material_system(void) {
  destroy_gpu_shader_group_layout(default_layout);
}

GPU_Shader_Group_Layout get_material_default_shader_group_layout(void) {
  return default_layout;
}

Material_Create_Result
make_material(Material_Create_Info *info, Allocator allocator) {
  GPU_Shader_Group_Layout layout = get_material_default_shader_group_layout();

  errdefer_scope;
  if (!gpu_texture_is_valid(info->gpu_albedo)) {
    return err(Material_Create_Result, Material_Error_Invalid_Albedo_Texture);
  }

  Material material = {0};
  material.gpu_albedo = info->gpu_albedo;
  material.gpu_albedo_is_owned = info->gpu_albedo_is_owned;
  material.gpu_sampler = make_gpu_sampler(&(GPU_Sampler_Create_Info){
    .filter = info->gpu_sampler_filter,
    .wrap = info->gpu_sampler_wrap,
  });

  // NOTE(nico): need to migrate the rest of the gpu code to a result based
  // return type
  if (!gpu_sampler_is_valid(material.gpu_sampler)) {
    return err(Material_Create_Result, Material_Error_Failed_To_Create_Sampler);
  }
  errdefer {
    destroy_gpu_sampler(material.gpu_sampler);
  };

  material.gpu_shader_data = or_return(
      make_gpu_shader_group_data(
          &(GPU_Shader_Group_Data_Create_Info){
            .layout = layout,
            .binds = ARRAY_LIT(
                GPU_Shader_Bind_Data_Create_Info,
                BIND_TEXTURE(material.gpu_albedo),
                BIND_SAMPLER(material.gpu_sampler)
            ),
          },
          allocator
      ),
      err(Material_Create_Result, Material_Error_Failed_To_Create_Data)
  );

  return_ok(Material_Create_Result, material);
}

Material_Error destroy_material(Material *material) {
  if (material->gpu_albedo_is_owned) {
    destroy_gpu_texture(material->gpu_albedo);
  }

  destroy_gpu_sampler(material->gpu_sampler);
  destroy_gpu_shader_group_data(material->gpu_shader_data);

  return Material_Error_None;
}