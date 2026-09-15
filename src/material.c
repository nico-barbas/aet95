#include "material.h"

#include "core/array.h"
#include "core/map.h"
#include "core/platform.h"

Material_Error init_material_cache(Material_Cache *cache, Allocator allocator) {
  if (cache->layout.handle != nullptr || cache->materials != nullptr) {
    return Material_Error_Failed_To_Create_Cache;
  }

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

  cache->materials = make_u32_open_map(Material, 32, allocator);

  return Material_Error_None;
}

void destroy_material_cache(Material_Cache *cache);