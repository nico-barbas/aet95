#include "game.h"

#include "asm.h"
#include "core/allocator.h"
#include "core/camera.h"
#include "core/imgui.h"
#include "core/log.h"
#include "core/math.h"
#include "core/platform.h"
#include "core/strings.h"
#include "db.h"
#include "font.h"
#include "render.h"
#include "render2d.h"
#include "ui.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

Game_State _game = {0};

/////////////////////////////
// Game specific helpers
/////////////////////////////
static bool32 parse_window_backend_env(App_Window_Backend *out) {
#if defined(PLATFORM_WEB)
  *out = App_Window_Backend_Auto;
  return true;
#else
  const char *value = getenv("AET95_WINDOW_BACKEND");

  if (value == nullptr || value[0] == '\0' || strcmp(value, "auto") == 0) {
    *out = App_Window_Backend_Auto;
    return true;
  }

  if (strcmp(value, "x11") == 0) {
    *out = App_Window_Backend_X11;
    return true;
  }

  if (strcmp(value, "wayland") == 0) {
    *out = App_Window_Backend_Wayland;
    return true;
  }

  return false;
#endif
}

static Element_Dimensions
measure_texture_wrapper(Element_Font el_font, String text) {
  Database_Font_Query font_query =
      database_get_font_atlas_entry((Font_ID)el_font.user_index, el_font.size);
  if (!font_query.ok) {
    return (Element_Dimensions){0};
  }

  Vec2 dim = font_atlas_entry_measure_text(font_query.value, text);
  return (Element_Dimensions){.width = dim.x, .height = dim.y};
}

////////////////////////////////////
// Actual game code
////////////////////////////////////
void init_game(void) {
  _game.version = from_c_str("0.0.1-a");
  _game.global_allocator = heap_allocator();

  Allocation_Result frame_mem = _game.global_allocator.alloc(
      _game.global_allocator, FRAME_ALLOCATOR_SIZE
  );
  assert(frame_mem.err == Allocation_Error_None);

  init_arena(&_game.frame_arena, frame_mem.allocation, FRAME_ALLOCATOR_SIZE);
  _game.frame_allocator = arena_allocator(&_game.frame_arena);

  App_Create_Info info = {
    .app = &_game.app,
    .window_width = STARTUP_WINDOW_WIDTH,
    .window_height = STARTUP_WINDOW_HEIGHT,
    .window_title = from_c_str("p59"),
    .logger = console_logger(Log_Level_Debug),
  };
  assert(parse_window_backend_env(&info.window_backend));

  bool32 app_ok = init_app(&info, _game.global_allocator);
  assert(app_ok);

#if defined(DEBUG)
  init_debug_renderer(
      &_game.debug_renderer,
      STARTUP_WINDOW_WIDTH,
      STARTUP_WINDOW_HEIGHT,
      _game.global_allocator
  );
#endif

  init_renderer(
      &_game.renderer,
      STARTUP_WINDOW_WIDTH,
      STARTUP_WINDOW_HEIGHT,
      _game.global_allocator
  );

  bool32 db_ok = init_database(&_game.renderer, _game.global_allocator);
  assert(db_ok);

  init_renderer_2d(
      &_game.renderer_2d,
      Font_ID_IBMPlex_Mono,
      from_c_str(""),
      _game.global_allocator
  );

  init_scene(&_game.scene, _game.global_allocator);
  init_element_context(
      &_game.el_ctx,
      &(Element_Context_Create_Info){
        .measure_text_proc = measure_texture_wrapper
      },
      _game.global_allocator
  );

  Aet_Machine machine = {0};
  assert(
      aet_machine_init(
          &machine,
          &(Aet_Machine_Create_Info){.ram_byte_cap = 512},
          _game.frame_allocator
      ) == Aet_Machine_Error_None
  );

  Aet_Program program = unwrap(
      aet_assemble(from_c_str("loadw r0, rx0, -32768"), _game.frame_allocator)
  );

  aet_cpu_load_program(&machine.cpu, program);
  aet_machine_run(&machine, 5000);

  tmp_init_game_view(&_game.renderer_2d, _game.global_allocator);
}

void close_game(void) {
  destroy_element_context(&_game.el_ctx);
  destroy_renderer(&_game.renderer);
  destroy_renderer_2d(&_game.renderer_2d);
  destroy_debug_renderer(&_game.debug_renderer);

  _game.global_allocator.free(_game.global_allocator, _game.frame_arena.buf);
  close_app(&_game.app);
}

///////////////////////////////////
// Scene management procedures
///////////////////////////////////
void init_scene(Scene *scene, Allocator allocator) {
  (void)allocator;
  free_all_entites(scene);

  scene->orbit_camera = orbit_camera();
}

void destroy_scene(Scene *scene) {
  (void)scene;
}

#define handle_eq(h1, h2)                                                      \
  ((h1).id == (h2).id && (h1).generation == (h2).generation)

Entity_Handle add_entity(Scene *scene, Entity *entity) {
  if (scene->entity_count >= SCENE_CAP) {
    assert(false);
    return (Entity_Handle){0};
  }
  i32 slot_idx = -1;
  i32 backing_idx = (i32)scene->entity_count;

  // Get the slot
  for (i32 i = 0; i < SCENE_CAP; i += 1) {
    if (scene->entity_slots[i].available) {
      slot_idx = i;
      break;
    }
  }

  assert(slot_idx >= 0);

  scene->entity_slots[slot_idx].backing_index = backing_idx;
  scene->entity_slots[slot_idx].available = false;

  scene->entities[scene->entity_count] = *entity;
  scene->entities[scene->entity_count].slot_id = slot_idx;
  scene->entity_count += 1;

  Entity_Handle result = (Entity_Handle){
    .id = slot_idx,
    .generation = scene->entity_slots[slot_idx].generation,
  };

  usize lookup_count = scene->entity_lookup_count[entity->kind]++;
  scene->entity_lookup[entity->kind][lookup_count] = result;

  return result;
}

Entity *get_entity_ptr(Scene *scene, Entity_Handle handle) {
  if (scene->entity_slots[handle.id].generation != handle.generation) {
    return nullptr;
  }

  i32 backing_idx = scene->entity_slots[handle.id].backing_index;
  return &scene->entities[backing_idx];
}

Entity_Handle get_entity_handle(Scene *scene, Entity *entity) {
  return (Entity_Handle){
    .id = entity->slot_id,
    .generation = scene->entity_slots[entity->slot_id].generation,
  };
}

bool32 remove_entity(Scene *scene, Entity_Handle handle) {
  if (scene->entity_slots[handle.id].generation != handle.generation) {
    return false;
  }

#if defined(ENTITY_SCENE_GRAPH_IMPL)
  Entity *entity = get_entity_ptr(scene, handle);

  if (entity->parent.some) {
    unparent_entity(scene, handle);
  }

  // NOTE(nico): Idk what's the most logical way to remove the links. Either
  // reparent the children to the node entity's parent or put them back at the
  // root. I'll go with the later for now
  Entity_Handle_Option current = entity->first_child;
  while (current.some) {
    Entity *child_entity = get_entity_ptr(scene, current.value);
    current = child_entity->next;

    child_entity->parent = none(Entity_Handle_Option);
    child_entity->next = none(Entity_Handle_Option);
  }

#endif

  i32 last_slot_idx = scene->entities[scene->entity_count - 1].slot_id;
  i32 removed_entity_idx = scene->entity_slots[handle.id].backing_index;

  scene->entity_slots[last_slot_idx].backing_index = removed_entity_idx;
  scene->entity_slots[handle.id].generation += 1;
  scene->entity_slots[handle.id].available = true;

  Entity_Kind k = scene->entities[removed_entity_idx].kind;
  scene->entities[removed_entity_idx] =
      scene->entities[scene->entity_count - 1];
  scene->entity_count -= 1;

  usize len = scene->entity_lookup_count[k];
  for (usize i = 0; i < len; i += 1) {
    if (handle_eq(handle, scene->entity_lookup[k][i])) {
      scene->entity_lookup[k][i] = scene->entity_lookup[k][len - 1];
      scene->entity_lookup_count[k] -= 1;
      break;
    }
  }

  return true;
}

void free_all_entites(Scene *scene) {
  for (i32 i = 0; i < SCENE_CAP; i += 1) {
    scene->entity_slots[i] = (Entity_Slot){.available = true};
  }
  scene->entity_count = 0;
}

bool32 entity_handle_eq(void *h1, void *h2) {
  Entity_Handle *_h1 = (Entity_Handle *)h1;
  Entity_Handle *_h2 = (Entity_Handle *)h2;

  return _h1->id == _h2->id && _h1->generation == _h2->generation;
}

u64 entity_handle_hash(void *key, usize size) {
  (void)size;
  Entity_Handle *h = (Entity_Handle *)key;

  u64 hash = hash_fnv1a(&h->generation, sizeof(h->generation));
  hash ^= hash_fnv1a(&h->id, sizeof(h->id));
  return hash;
}

#if defined(ENTITY_SCENE_GRAPH_IMPL)
void parent_entity(
    Scene *scene, Entity_Handle entity_handle, Entity_Handle parent_handle
) {
  Entity *entity = get_entity_ptr(scene, entity_handle);
  Entity *parent_entity = get_entity_ptr(scene, parent_handle);

  if (entity->parent.some) {
    unparent_entity(scene, entity_handle);
  }

  if (parent_entity->last_child.some) {
    Entity *last_child_entity =
        get_entity_ptr(scene, parent_entity->last_child.value);
    last_child_entity->next = some(Entity_Handle_Option, entity_handle);
  } else {
    parent_entity->first_child = some(Entity_Handle_Option, entity_handle);
  }

  parent_entity->last_child = some(Entity_Handle_Option, entity_handle);
  entity->parent = some(Entity_Handle_Option, parent_handle);
}

void unparent_entity(Scene *scene, Entity_Handle _handle) {
  Entity *entity = get_entity_ptr(scene, _handle);
  assert(entity->parent.some);

  Entity *parent_entity = get_entity_ptr(scene, entity->parent.value);

  entity->parent = none(Entity_Handle_Option);

  Entity_Handle_Option previous = none(Entity_Handle_Option);
  Entity_Handle_Option current = parent_entity->first_child;
  while (current.some) {
    Entity *child_entity = get_entity_ptr(scene, current.value);

    if (handle_eq(current.value, _handle)) {
      if (!previous.some) {
        parent_entity->first_child = child_entity->next;
      } else {
        Entity *previous_child_entity = get_entity_ptr(scene, previous.value);
        previous_child_entity->next = child_entity->next;
      }

      if (parent_entity->last_child.some &&
          handle_eq(current.value, parent_entity->last_child.value)) {
        parent_entity->last_child = previous;
      }

      child_entity->next = none(Entity_Handle_Option);
      break;
    }

    previous = current;
    current = child_entity->next;
  }
}
#endif

static Raw_Camera *scene_active_camera(Scene *scene) {
  return &scene->orbit_camera.base;
}

/////////////////////////////
// Main lifecycle hookss
/////////////////////////////

static void update_active_camera_fixed(Scene *scene, f32 dt) {
  f32 frame_w = (f32)_game.app.window_width;
  f32 frame_h = (f32)_game.app.window_height;

  update_orbit_camera(&scene->orbit_camera, frame_w, frame_h, dt);
}

void update_game(void) {
  f32 fixed_dt = DT;

  u64 elapsed_time_ns =
      min_u64(app_get_current_time_ns() - app_get_last_time_ns(), MAX_FRAME_NS);
  _game.time_accumulator += elapsed_time_ns;

  if (app_key_just_pressed(Keyboard_Key_Escape)) {
    _game.app.running = false;
  }

  u32 fixed_step_count = 0;
  while (_game.time_accumulator >= FRAME_NS &&
         fixed_step_count < MAX_FIXED_STEPS_PER_FRAME) {
    Scene *scene = &_game.scene;

    scene->active_camera_state.previous = scene->active_camera_state.current;
    update_active_camera_fixed(scene, fixed_dt);
    scene->active_camera_state.current = *scene_active_camera(scene);

    // update_entities(&_state.db, scene, fixed_dt);

    _game.time_accumulator -= FRAME_NS;
    fixed_step_count += 1;
  }

  if (_game.time_accumulator >= FRAME_NS) {
    _game.time_accumulator %= FRAME_NS;
  }

  _game.frame_allocator.free_all(_game.frame_allocator);
}

void render_game(void) {
  f32 frame_alpha = (f32)_game.time_accumulator / (f32)FRAME_NS;
  Scene *scene = &_game.scene;

  Raw_Camera render_camera = lerp_raw_cameras(
      &scene->active_camera_state.previous,
      &scene->active_camera_state.current,
      frame_alpha
  );
  update_raw_camera(
      &render_camera, (f32)_game.app.window_width, (f32)_game.app.window_height
  );

  begin_render(&_game.renderer, &render_camera);

  draw_model(
      &_game.renderer,
      &(Model_Draw_Info){
        .model = _db.model_table[Model_ID_Default_Cube],
        .transform = mat4_identity(),
        .color = color(1, 1, 1, 1),
      }
  );

  end_render(&_game.renderer);

  begin_render_2d(
      &_game.renderer_2d,
      (f32)_game.app.window_width,
      (f32)_game.app.window_height
  );

  begin_ui(&_game.el_ctx);
  game_view(&_game);
  Element_Render_Command_Buffer cmds = end_ui(&_game.el_ctx);

  render_game_view(&_game.renderer_2d, cmds);

  end_render_2d(&_game.renderer_2d);
}