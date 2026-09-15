#include "db.h"

#include "core/allocator.h"
#include "core/map.h"
#include "core/platform.h"
#include "core/runtime.h"
#include "core/strings.h"
#include "core/types.h"
#include "font.h"
#include "model.h"

#include <assert.h>

Database _db = {0};

#define DB_GPU_ALLOCATOR_CAP (MEGABYTE * 128)
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

// TODO(nico): Provide a config struct that defines model repositories (gltf
// files) and a lookup of repositories and node name per model id

typedef Result(
    Database_Resource_Handle, Database_Error
) Database_Resource_Alloc_Result;

static void database_clear();
static Database_Resource_Alloc_Result
database_alloc_resource(Database_Resource_Kind kind, usize size);

Database_Error init_database(Allocator allocator) {
  errdefer_scope;

  _db.allocator = allocator;

  _db.gpu_allocator = make_gpu_buffer(&(GPU_Buffer_Create_Info){
    .size = DB_GPU_ALLOCATOR_CAP,
    .usage = GPU_Buffer_Usage_Vertex | GPU_Buffer_Usage_Index |
             GPU_Buffer_Usage_Copy_Dst,
  });

  if (!gpu_buffer_is_valid(_db.gpu_allocator)) {
    return Database_Error_Failed_To_Initialize;
  }
  errdefer {
    destroy_gpu_buffer(_db.gpu_allocator);
  };

  _db.stable_id_lookup =
      make_u32_open_map(Database_Resource_Handle, RESOURCE_CAP, allocator);
  if (_db.stable_id_lookup == nullptr) {
    return Database_Error_Failed_To_Initialize;
  }
  errdefer {
    delete_open_map(_db.stable_id_lookup);
  };

  database_clear();

  // _db.model_table[Model_ID_Default_Cube] = or_return(
  //     make_cube_model(&_db.gpu_allocator, 0),
  //     Database_Error_Failed_To_Initialize
  // );
  // errdefer {
  //   destroy_model(&_db.model_table[Model_ID_Default_Cube]);
  // };

  // Font_Error font_err = init_font_atlas_from_file(
  //     &_db.font_table[Font_ID_IBMPlex_Mono],
  //     from_cstring("assets/fonts/IBMPlexMono-Regular.ttf"),
  //     allocator
  // );

  // if (font_err != Font_Error_None) {
  //   return Database_Error_Failed_To_Initialize;
  // }
  // errdefer {
  //   destroy_font_atlas(&_db.font_table[Font_ID_IBMPlex_Mono]);
  // };

  commit();
  return Database_Error_None;
}

static void database_clear() {
  _db.cap = RESOURCE_CAP;
  for (usize i = 0; i < _db.cap; i += 1) {
    _db.table[i] = (Database_Resource_Slot){
      .generation = 1,
      .packed = RESOURCE_SLOT_FREE_BIT_MASK,
    };
  }
}

static Database_Resource_Alloc_Result
database_alloc_resource(Database_Resource_Kind kind, usize size) {
  if (_db.count >= _db.cap) {
    return err(
        Database_Resource_Alloc_Result, Database_Error_Resource_Capacity_Reached
    );
  }

  u32 slot_index = 0;
  bool32 slot_found = false;
  for (usize i = 0; i < _db.cap; i += 1) {
    if (_db.table[i].packed & RESOURCE_SLOT_FREE_BIT_MASK) {
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
      alloc(_db.allocator, size),
      err(Database_Resource_Alloc_Result, Database_Error_Failed_Alloc_Resource)
  );

  u32 backing_index = (u32)_db.count;
  u32 generation = _db.table[slot_index].generation;

  _db.table[slot_index] = (Database_Resource_Slot){
    .generation = generation,
    .packed = backing_index & RESOURCE_SLOT_BACKING_INDEX_MASK,
  };

  _db.resources[backing_index] = (Database_Resource){
    .backing_index = backing_index,
    .slot_index = slot_index,
    .kind = kind,
    .status = Database_Resource_Status_Loading,
    .ptr = ptr,
  };

  return ok(
      Database_Resource_Alloc_Result,
      ((Database_Resource_Handle){.generation = generation, .id = slot_index})
  );
}

Database_Font_Query database_get_font_atlas_entry(Font_ID id, f32 size) {
  if (id >= Font_ID_MAX) {
    return err(Database_Font_Query, Database_Error_Invalid_Resource_ID);
  }

  Font_Atlas *font = &_db.font_table[id];
  Font_Atlas_Entry_Ptr_Option entry_opt = font_atlas_get_entry(font, size);
  if (entry_opt.some) {
    return ok(Database_Font_Query, entry_opt.value);
  }

  // NOTE(nico): The database need its own scratch allocator. Need to see how
  // to do that
  Font_Atlas_Entry_Load_Result entry_load_result =
      font_atlas_load_font_size(font, size, heap_allocator());
  if (!entry_load_result.ok) {
    assert(false);
    return err(Database_Font_Query, Database_Error_Failed_Stream_Resource);
  }

  return ok(Database_Font_Query, entry_load_result.value);
}
