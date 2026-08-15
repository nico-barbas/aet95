#include "db.h"

#include "render.h"

Database _db = {0};

// TODO(nico): Provide a config struct that defines model repositories (gltf
// files) and a lookup of repositories and node name per model id

bool32 init_database(Renderer *renderer) {
  Model_Create_Result cube_model_result =
      model_make_cube(renderer, &renderer->default_material);
  if (!cube_model_result.ok) {
    return false;
  }

  _db.model_table[Model_ID_Default_Cube] = cube_model_result.value;
  return true;
}