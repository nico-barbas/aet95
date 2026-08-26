#include "db.h"

#include "core/allocator.h"
#include "core/strings.h"
#include "core/types.h"
#include "font.h"
#include "render.h"

#include <assert.h>

Database _db = {0};

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

bool32 init_database(Renderer *renderer, Allocator allocator) {
  Model_Create_Result cube_model_result =
      model_make_cube(renderer, &renderer->default_material);
  if (!cube_model_result.ok) {
    return false;
  }

  _db.model_table[Model_ID_Default_Cube] = cube_model_result.value;

  Font_Error font_err = init_font_atlas_from_file(
      &_db.font_table[Font_ID_IBMPlex_Mono],
      from_c_str("assets/fonts/IBMPlexMono-Regular.ttf"),
      allocator
  );
  assert(font_err == Font_Error_None);
  return true;
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