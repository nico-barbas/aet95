#include "material.h"

#include "core/array.h"
#include "core/map.h"
#include "core/platform.h"
#include "core/runtime.h"

Material_Error init_material_cache(Material_Cache *cache, Allocator allocator) {
  if (cache->layout.handle != nullptr || cache->materials != nullptr) {
    return Material_Error_Failed_To_Create_Cache;
  }
  cache->allocator = allocator;
  cache->layout = or_return(
      make_gpu_shader_group_layout(
          &(GPU_Shader_Group_Layout_Create_Info){
            .binds = ARRAY_LIT(
                GPU_Shader_Bind_Info, SHADER_TEXTURE(), SHADER_SAMPLER()
            ),
          },
          allocator
      ),
      Material_Error_Failed_To_Create_Cache
  );

  cache->materials = make_u64_open_map(Material, 32, allocator);

  return Material_Error_None;
}

void destroy_material_cache(Material_Cache *cache) {
  destroy_gpu_shader_group_layout(cache->layout);
  delete_open_map(cache->materials);
}

// NOTE(nico): Need to decide on collision policies
// The goal of the cache is to reduce duplicate, but what is a duplicate?
// Should we compare the new request and the existing material for byte
// equality?
Material_Create_Result material_cache_make_material(
    Material_Cache *cache, Material_Create_Info *info
) {
  errdefer_scope;

  Material *existing_material = open_map_get(cache->materials, info->handle);
  if (existing_material != nullptr) {
    return_ok(Material_Create_Result, info->handle);
  }

  if (!gpu_texture_is_valid(info->gpu_albedo)) {
    return err(Material_Create_Result, Material_Error_Invalid_Albedo_Texture);
  }

  Material material = {0};
  material.handle = info->handle;
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
            .layout = cache->layout,
            .binds = ARRAY_LIT(
                GPU_Shader_Bind_Data_Create_Info,
                BIND_TEXTURE(material.gpu_albedo),
                BIND_SAMPLER(material.gpu_sampler)
            ),
          },
          cache->allocator
      ),
      err(Material_Create_Result,
          Material_Error_Failed_To_Create_Shader_Group_Data)
  );

  open_map_set(cache->materials, material.handle, material);
  return_ok(Material_Create_Result, material.handle);
}

Material_Error
material_cache_destroy_material(Material_Cache *cache, Material_Handle handle) {
  Material *material = open_map_get(cache->materials, handle);
  if (material == nullptr) {
    return Material_Error_Invalid_Handle;
  }

  if (!open_map_remove(cache->materials, handle)) {
    return Material_Error_Failed_To_Remove_From_Cache;
  }

  if (material->gpu_albedo_is_owned) {
    destroy_gpu_texture(material->gpu_albedo);
  }

  destroy_gpu_sampler(material->gpu_sampler);
  destroy_gpu_shader_group_data(material->gpu_shader_data);
  open_map_remove(cache->materials, handle);

  return Material_Error_None;
}

Material_Query_Result
material_cache_query_material(Material_Cache *cache, Material_Handle handle) {
  Material *material = open_map_get(cache->materials, handle);
  return material != nullptr
             ? ok(Material_Query_Result, material)
             : err(Material_Query_Result, Material_Error_Invalid_Handle);
}