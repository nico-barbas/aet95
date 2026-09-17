#include "db.h"

#include "core/allocator.h"
#include "core/map.h"
#include "core/platform.h"
#include "core/runtime.h"
#include "core/strings.h"
#include "core/types.h"
#include "font.h"
#include "material.h"
#include "model.h"

#include <assert.h>

#define DB_GPU_ALLOCATOR_CAP (MEGABYTE * 128)
#define DB_RESOURCE_CAP 512

#define RESOURCE_SLOT_BACKING_INDEX_MASK 0x7FFFFFFFu
#define RESOURCE_SLOT_FREE_BIT_MASK 0x80000000u

/*
  NOTE(nico):
  The goal of this system is to make asset and resource loading pain-free,
  smooth and opaque. Opaque is generally the opposite of what this project
  strives for, but this niche is one where is it really valuable. As a gameplay
  programmer, you do not care if your resource is loaded or not. You want to use
  it no matter what.

  For this reason, one of the prime feature of the db will be to make streaming
  and lazy loading invisible to the consumer.

  This means threading to not block the main thread at runtime and raises 2
  problems:
  - All the resource creation functions steps are conflated today. Parsing and
  loading to the gpu are in a single body. This only matters if the second
  problem cannot be addressed
  - Wgpu allows for multi-threaded writes and read enqueuing but it needs
  synchronization with the main queue. The platform does not expose that
  currently and for simplicity reason (nogfx API style design), I'd rather not
  expose it. This means it needs planning on how to make that invisble too for
  the consumer

  Secondly, I need to design a format that the db can use to find the resource
  root source. There needs to be 2 separate data structure. One for the schema
  and an internal one for automatic eviction of unused resource.

  Thirdly, but not needed at first, hot-reloading.

  Finally, there is currently a dependency between the database and the
  renderer. Who owns the gpu buffer is still unknown. The goal of a centralized
  renderer gpu memory was to prevent many small gpu memory allocation and speed
  it up. Might be not necessary. Up in the air. Especially since the renderer
  centralized memory is an arena-style allocator and cannot frees
*/

typedef enum Database_Resource_Kind {
  Database_Resource_Kind_Model,
  Database_Resource_Kind_Material,
  Database_Resource_Kind_Texture,
  Database_Resource_Kind_Font,
  Database_Resource_Kind_MAX,
} Database_Resource_Kind;

typedef struct Database_Resource_Handle {
  u32 id;
  u32 generation;
} Database_Resource_Handle;

typedef Option(u32) Database_Stable_ID_Option;

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

typedef struct Database {
  Allocator allocator;

  // NOTE(nico): This is an arena for now. When we move to streaming and on
  // demand model uploading, I will write a free-list style gpu allocator
  GPU_Buffer gpu_allocator;

  Database_Resource resources[DB_RESOURCE_CAP];
  Database_Resource_Slot table[DB_RESOURCE_CAP];
  usize count;
  usize cap;

  Open_Map stable_id_lookup;
  Material_Cache material_cache;

  // Model model_table[Model_ID_MAX];
  // Font_Atlas font_table[Font_ID_MAX];

} Database;

typedef Result(
    Database_Resource_Handle, Database_Error
) Database_Resource_Alloc_Result;
typedef Option(Database_Resource *) Database_Resource_Ptr_Option;

static Database g_db = {0};

static const usize resource_sizes[Database_Resource_Kind_MAX] = {
  [Database_Resource_Kind_Model] = sizeof(Model),
  [Database_Resource_Kind_Material] = sizeof(Material_Handle),
  [Database_Resource_Kind_Texture] = sizeof(GPU_Texture),
  [Database_Resource_Kind_Font] = sizeof(Font_Atlas),
};

static void database_clear(void);
// static Database_Resource_Alloc_Result
// database_reserve_resource_handle(Database_Resource_Kind kind);
static Database_Resource_Alloc_Result
database_alloc_resource(Database_Resource_Create_Info *info);
static Database_Error database_free_resource(Database_Resource_Handle handle);
static Database_Resource_Ptr_Option
database_get_resource_ptr(Database_Resource_Handle handle);

static u64 bake_stable_id(Database_Resource_Kind kind, u32 id) {
  return (u64)kind << 32 | (u64)id;
}

static u64 database_resource_handle_pack(Database_Resource_Handle handle) {
  return (u64)handle.generation << 32 | (u64)handle.id;
}

Database_Error init_database(Allocator allocator) {
  errdefer_scope;

  g_db.allocator = allocator;

  g_db.gpu_allocator = make_gpu_buffer(&(GPU_Buffer_Create_Info){
    .size = DB_GPU_ALLOCATOR_CAP,
    .usage = GPU_Buffer_Usage_Vertex | GPU_Buffer_Usage_Index |
             GPU_Buffer_Usage_Copy_Dst,
  });

  if (!gpu_buffer_is_valid(g_db.gpu_allocator)) {
    return Database_Error_Failed_To_Initialize;
  }
  errdefer {
    destroy_gpu_buffer(g_db.gpu_allocator);
  };

  if (init_material_cache(&g_db.material_cache, allocator) !=
      Material_Error_None) {
    return Database_Error_Failed_To_Initialize;
  }

  g_db.stable_id_lookup =
      make_u64_open_map(Database_Resource_Handle, DB_RESOURCE_CAP, allocator);
  if (g_db.stable_id_lookup == nullptr) {
    return Database_Error_Failed_To_Initialize;
  }
  errdefer {
    delete_open_map(g_db.stable_id_lookup);
  };

  database_clear();

  Database_Resource_Handle font_handle = or_return(
      database_alloc_resource(&(Database_Resource_Create_Info){
        .kind = Database_Resource_Kind_Font,
        .stable_id =
            some(Database_Stable_ID_Option, Font_Stable_ID_IBMPlex_Mono),
        .font.filepath = from_cstring("assets/fonts/IBMPlexMono-Regular.ttf"),
      }),
      Database_Error_Failed_To_Initialize
  );
  errdefer {
    database_free_resource(font_handle);
  };

  Database_Resource_Handle cube_handle = or_return(
      database_alloc_resource(&(Database_Resource_Create_Info){
        .kind = Database_Resource_Kind_Model,
        .stable_id =
            some(Database_Stable_ID_Option, Model_Stable_ID_Default_Cube),
        .skip_init = true,
      }),
      Database_Error_Failed_To_Initialize
  );
  errdefer {
    database_free_resource(cube_handle);
  };

  Database_Resource_Ptr_Option cube_resource_opt =
      database_get_resource_ptr(cube_handle);
  if (!cube_resource_opt.some) {
    return Database_Error_Failed_To_Initialize;
  }
  Model *cube_model = (Model *)cube_resource_opt.value->ptr;
  *cube_model = or_return(
      make_cube_model(&g_db.gpu_allocator, (Material_Handle){0}),
      Database_Error_Failed_To_Initialize
  );

  byte white_pixel[4] = {255, 255, 255, 255};
  Database_Resource_Handle white_texture_handle = or_return(
      database_alloc_resource(&(Database_Resource_Create_Info){
        .kind = Database_Resource_Kind_Texture,
        .stable_id = some(Database_Stable_ID_Option, Texture_Stable_ID_White),
        .texture =
            {
              .kind = GPU_Texture_Kind_2D,
              .space = GPU_Texture_Space_sRGB,
              .source = GPU_Texture_Source_Raw_Memory,
              .raw =
                  {.data = white_pixel, .width = 1, .height = 1, .channels = 4},
            },
      }),
      Database_Error_Failed_To_Initialize
  );
  errdefer {
    database_free_resource(white_texture_handle);
  };

  // NOTE(nico): Fuck this shit. I need to think of a way to automate startup
  // resource creation. There 10 billions indirections in this shit
  Database_Resource_Ptr_Option white_texture_resource_opt =
      database_get_resource_ptr(white_texture_handle);
  if (!white_texture_resource_opt.some) {
    return Database_Error_Failed_To_Initialize;
  }
  Database_Resource_Handle default_material_handle = or_return(
      database_alloc_resource(&(Database_Resource_Create_Info){
        .kind = Database_Resource_Kind_Material,
        .stable_id =
            some(Database_Stable_ID_Option, Material_Stable_Id_Default),
        .material =
            {
              .gpu_albedo =
                  *((GPU_Texture *)white_texture_resource_opt.value->ptr),
              .gpu_sampler_filter = GPU_Sampler_Filter_Nearest,
              .gpu_sampler_wrap = GPU_Sampler_Wrap_Clamp,
            },
      }),
      Database_Error_Failed_To_Initialize
  );
  errdefer {
    database_free_resource(default_material_handle);
  };

  commit();
  return Database_Error_None;
}

Database_Error destroy_database() {
  database_clear();
  destroy_material_cache(&g_db.material_cache);
  destroy_gpu_buffer(g_db.gpu_allocator);
  return Database_Error_None;
}

GPU_Shader_Group_Layout
database_get_default_material_shader_group_layout(void) {
  return g_db.material_cache.layout;
}

// FIXME(nico): need to clean the underlying resources here
static void database_clear(void) {
  g_db.cap = DB_RESOURCE_CAP;
  for (usize i = 0; i < g_db.count; i += 1) {
    free_(g_db.allocator, g_db.resources[i].ptr);
  }

  for (usize i = 0; i < g_db.cap; i += 1) {
    g_db.table[i] = (Database_Resource_Slot){
      .generation = 1,
      .packed = RESOURCE_SLOT_FREE_BIT_MASK,
    };
  }
}

// static Database_Resource_Alloc_Result
// database_reserve_resource_handle(Database_Resource_Kind kind) {
//   if (g_db.count >= g_db.cap) {
//     return err(
//         Database_Resource_Alloc_Result,
//         Database_Error_Resource_Capacity_Reached
//     );
//   }

//   if (kind >= Database_Resource_Kind_MAX) {
//     return err(
//         Database_Resource_Alloc_Result, Database_Error_Failed_Alloc_Resource
//     );
//   }

//   u32 slot_index = 0;
//   bool32 slot_found = false;
//   for (usize i = 0; i < g_db.cap; i += 1) {
//     if (g_db.table[i].packed & RESOURCE_SLOT_FREE_BIT_MASK) {
//       slot_index = (u32)i;
//       slot_found = true;
//       break;
//     }
//   }

//   if (!slot_found) {
//     return err(
//         Database_Resource_Alloc_Result,
//         Database_Error_Resource_Capacity_Reached
//     );
//   }

//   u32 backing_index = (u32)g_db.count;
//   u32 generation = g_db.table[slot_index].generation;

//   g_db.table[slot_index] = (Database_Resource_Slot){
//     .generation = generation,
//     .packed = backing_index & RESOURCE_SLOT_BACKING_INDEX_MASK,
//   };

//   g_db.resources[backing_index] = (Database_Resource){
//     .backing_index = backing_index,
//     .slot_index = slot_index,
//     .kind = kind,
//     .status = Database_Resource_Status_Loading,
//   };

//   Database_Resource_Handle handle =
//       (Database_Resource_Handle){.generation = generation, .id = slot_index};

//   return ok(Database_Resource_Alloc_Result, handle);
// }

// FIXME(nico): It is REALLY primordial for this operation to be atomic. It
// CANNOT leave the db in a broken state
static Database_Resource_Alloc_Result
database_alloc_resource(Database_Resource_Create_Info *info) {
  if (g_db.count >= g_db.cap) {
    return err(
        Database_Resource_Alloc_Result, Database_Error_Resource_Capacity_Reached
    );
  }

  if (info->kind >= Database_Resource_Kind_MAX) {
    return err(
        Database_Resource_Alloc_Result, Database_Error_Failed_Alloc_Resource
    );
  }

  usize size = resource_sizes[info->kind];

  u32 slot_index = 0;
  bool32 slot_found = false;
  for (usize i = 0; i < g_db.cap; i += 1) {
    if (g_db.table[i].packed & RESOURCE_SLOT_FREE_BIT_MASK) {
      slot_index = (u32)i;
      slot_found = true;
      break;
    }
  }

  if (!slot_found) {
    return err(
        Database_Resource_Alloc_Result, Database_Error_Resource_Capacity_Reached
    );
  }
  rawptr ptr = or_return(
      alloc(g_db.allocator, size),
      err(Database_Resource_Alloc_Result, Database_Error_Failed_Alloc_Resource)
  );

  u32 backing_index = (u32)g_db.count;
  u32 generation = g_db.table[slot_index].generation;

  Database_Resource_Handle handle =
      (Database_Resource_Handle){.generation = generation, .id = slot_index};

  if (!info->skip_init) {
    switch (info->kind) {
    case Database_Resource_Kind_Model: {
      Model *model = (Model *)ptr;
      *model = or_return(
          make_model(&info->model),
          err(Database_Resource_Alloc_Result,
              Database_Error_Failed_To_Initialize_Resource)
      );
    } break;
    case Database_Resource_Kind_Texture: {
      GPU_Texture *texture = (GPU_Texture *)ptr;
      *texture = or_return(
          make_gpu_texture(&info->texture),
          err(Database_Resource_Alloc_Result,
              Database_Error_Failed_To_Initialize_Resource)
      );
    } break;
    case Database_Resource_Kind_Material: {
      info->material.handle = database_resource_handle_pack(handle);
      Material_Handle *material_handle = (Material_Handle *)ptr;
      *material_handle = or_return(
          material_cache_make_material(&g_db.material_cache, &info->material),
          err(Database_Resource_Alloc_Result,
              Database_Error_Failed_To_Initialize_Resource)
      );
    } break;
    case Database_Resource_Kind_Font: {
      Font_Atlas *font = (Font_Atlas *)ptr;
      Font_Error err =
          init_font_atlas_from_file(font, info->font.filepath, g_db.allocator);
      if (err != Font_Error_None) {
        return err(
            Database_Resource_Alloc_Result,
            Database_Error_Failed_To_Initialize_Resource
        );
      }
    } break;
    // TODO(nico): add the material subtype
    case Database_Resource_Kind_MAX:
      assert(false);
    }
  }

  g_db.table[slot_index] = (Database_Resource_Slot){
    .generation = generation,
    .packed = backing_index & RESOURCE_SLOT_BACKING_INDEX_MASK,
  };

  g_db.resources[backing_index] = (Database_Resource){
    .backing_index = backing_index,
    .slot_index = slot_index,
    .kind = info->kind,
    .status = Database_Resource_Status_Loading,
    .ptr = ptr,
  };

  g_db.count += 1;

  if (info->stable_id.some) {
    open_map_set(
        g_db.stable_id_lookup,
        bake_stable_id(info->kind, info->stable_id.value),
        handle
    );
  }

  return ok(Database_Resource_Alloc_Result, handle);
}

// NOTE(nico): Same as alloc. It needs to be atomic
static Database_Error database_free_resource(Database_Resource_Handle handle) {
  if (handle.id >= g_db.cap ||
      g_db.table[handle.id].generation != handle.generation) {
    return Database_Error_Invalid_Resource_Handle;
  }

  usize last_resource_index = g_db.count - 1;
  usize last_slot_index = (usize)g_db.resources[last_resource_index].slot_index;
  usize removed_resource_index =
      (usize)(g_db.table[handle.id].packed & RESOURCE_SLOT_BACKING_INDEX_MASK);

  if (g_db.resources[removed_resource_index].slot_index != handle.id) {
    return Database_Error_Invalid_Resource_Handle;
  }

  Database_Resource resource = g_db.resources[removed_resource_index];
  switch (resource.kind) {
  case Database_Resource_Kind_Model:
    destroy_model((Model *)resource.ptr);
    break;
  case Database_Resource_Kind_Material:
    material_cache_destroy_material(
        &g_db.material_cache, (*(Material_Handle *)resource.ptr)
    );
    break;
  case Database_Resource_Kind_Texture:
    destroy_gpu_texture((*(GPU_Texture *)resource.ptr));
    break;
  case Database_Resource_Kind_Font:
    destroy_font_atlas((Font_Atlas *)resource.ptr);
    break;
  case Database_Resource_Kind_MAX:
    return Database_Error_Invalid_Resource_Handle;
  }

  g_db.table[handle.id] = (Database_Resource_Slot){
    .generation = g_db.table[handle.id].generation + 1,
    .packed = RESOURCE_SLOT_FREE_BIT_MASK,
  };
  free_(g_db.allocator, g_db.resources[removed_resource_index].ptr);

  if (removed_resource_index != last_resource_index) {
    g_db.resources[removed_resource_index] =
        g_db.resources[last_resource_index];
    g_db.resources[removed_resource_index].backing_index =
        (u32)removed_resource_index;
    g_db.table[last_slot_index].packed =
        (u32)removed_resource_index & RESOURCE_SLOT_BACKING_INDEX_MASK;
  }

  g_db.count -= 1;

  return Database_Error_None;
}

static Database_Resource_Ptr_Option
database_get_resource_ptr(Database_Resource_Handle handle) {
  if (handle.id >= g_db.cap ||
      g_db.table[handle.id].generation != handle.generation) {
    return none(Database_Resource_Ptr_Option);
  }
  u32 backing_index =
      g_db.table[handle.id].packed & RESOURCE_SLOT_BACKING_INDEX_MASK;
  return some(Database_Resource_Ptr_Option, &g_db.resources[backing_index]);
}

Database_Model_Query database_get_stable_model(Model_Stable_ID id) {
  u64 stable_id = bake_stable_id(Database_Resource_Kind_Model, id);
  Database_Resource_Handle *handle =
      open_map_get(g_db.stable_id_lookup, stable_id);
  if (handle == nullptr) {
    return err(Database_Model_Query, Database_Error_Invalid_Resource_Handle);
  }

  Database_Resource_Ptr_Option resource_opt =
      database_get_resource_ptr(*handle);
  if (!resource_opt.some) {
    return err(Database_Model_Query, Database_Error_Invalid_Resource_Handle);
  }

  if (resource_opt.value->kind != Database_Resource_Kind_Model) {
    return err(Database_Model_Query, Database_Error_Internal_Failure);
  }

  return ok(Database_Model_Query, (Model *)resource_opt.value->ptr);
}

Database_Font_Query database_get_stable_font_atlas(Font_Stable_ID id) {
  u64 stable_id = bake_stable_id(Database_Resource_Kind_Font, id);
  Database_Resource_Handle *handle =
      open_map_get(g_db.stable_id_lookup, stable_id);
  if (handle == nullptr) {
    return err(Database_Font_Query, Database_Error_Invalid_Resource_Handle);
  }

  Database_Resource_Ptr_Option resource_opt =
      database_get_resource_ptr(*handle);
  if (!resource_opt.some) {
    return err(Database_Font_Query, Database_Error_Invalid_Resource_Handle);
  }

  if (resource_opt.value->kind != Database_Resource_Kind_Font) {
    return err(Database_Font_Query, Database_Error_Internal_Failure);
  }

  Font_Atlas *font = (Font_Atlas *)resource_opt.value->ptr;
  return ok(Database_Font_Query, font);
}

Database_Material_Query database_get_stable_material(Material_Stable_ID id) {
  u64 stable_id = bake_stable_id(Database_Resource_Kind_Material, id);
  Database_Resource_Handle *handle =
      open_map_get(g_db.stable_id_lookup, stable_id);
  if (handle == nullptr) {
    return err(Database_Material_Query, Database_Error_Invalid_Resource_Handle);
  }

  Database_Resource_Ptr_Option resource_opt =
      database_get_resource_ptr(*handle);
  if (!resource_opt.some) {
    return err(Database_Material_Query, Database_Error_Invalid_Resource_Handle);
  }

  if (resource_opt.value->kind != Database_Resource_Kind_Material) {
    return err(Database_Material_Query, Database_Error_Internal_Failure);
  }

  Material *material = or_return(
      material_cache_query_material(
          &g_db.material_cache, (*(Material_Handle *)resource_opt.value->ptr)
      ),
      err(Database_Material_Query, Database_Error_Invalid_Resource_Handle)
  );
  return ok(Database_Material_Query, material);
}

Database_Texture_Query database_get_stable_texture(Texture_Stable_ID id) {
  u64 stable_id = bake_stable_id(Database_Resource_Kind_Texture, id);
  Database_Resource_Handle *handle =
      open_map_get(g_db.stable_id_lookup, stable_id);
  if (handle == nullptr) {
    return err(Database_Texture_Query, Database_Error_Invalid_Resource_Handle);
  }

  Database_Resource_Ptr_Option resource_opt =
      database_get_resource_ptr(*handle);
  if (!resource_opt.some) {
    return err(Database_Texture_Query, Database_Error_Invalid_Resource_Handle);
  }

  if (resource_opt.value->kind != Database_Resource_Kind_Texture) {
    return err(Database_Texture_Query, Database_Error_Internal_Failure);
  }

  return ok(Database_Texture_Query, (GPU_Texture *)resource_opt.value->ptr);
}