#include "game.h"

#include "asm.h"
#include "core/allocator.h"
#include "core/camera.h"
#include "core/log.h"
#include "core/math.h"
#include "core/platform.h"
#include "core/strings.h"
#include "db.h"
#include "hal.h"
#include "render.h"
#include "render2d.h"
#include "view.h"

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
  init_view(&_game.renderer_2d);

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

  push_view_inbound_event((View_Inbound_Event){
    .kind = View_Inbound_Event_Open_Window,
    .open_window = (Window_Open_Info){
      .title = from_c_str("code::builder"),
      .kind = Window_Kind_Code_Editor,
      .position = vec2(100, 100),
      .width = 400,
      .height = 400,
    },
  });

  push_view_inbound_event((View_Inbound_Event){
    .kind = View_Inbound_Event_Open_Window,
    .open_window = (Window_Open_Info){
      .title = from_c_str("code::builder"),
      .kind = Window_Kind_Code_Editor,
      .position = vec2(700, 100),
      .width = 400,
      .height = 400,
    },
  });
}

void close_game(void) {
#if defined(DEBUG)
  destroy_debug_renderer(&_game.debug_renderer);
#endif

  destroy_renderer(&_game.renderer);
  destroy_renderer_2d(&_game.renderer_2d);
  destroy_view();

  _game.global_allocator.free(_game.global_allocator, _game.frame_arena.buf);
  close_app(&_game.app);
}

///////////////////////////////////
// Scene management procedures
///////////////////////////////////
void init_scene(Scene *scene, Allocator allocator) {
  (void)allocator;
  scene_free_all_entites(scene);

  // NOTE(nico): really awkward to initialize
  scene_add_entity(
      scene,
      &(Entity){
        .kind = Entity_Kind_Machine,
        .up = VEC3_UP,
        .model = Model_ID_Default_Cube,
        .machine = {0},
      }
  );
  scene->orbit_camera = orbit_camera();
}

void destroy_scene(Scene *scene) {
  (void)scene;
}

#define handle_eq(h1, h2)                                                      \
  ((h1).id == (h2).id && (h1).generation == (h2).generation)

// FIXME(nico): move all this crap to return a Result like the windows in view.c
Entity_Handle scene_add_entity(Scene *scene, Entity *entity) {
  if (scene->entity_count >= SCENE_CAP) {
    assert(false);
    return (Entity_Handle){0};
  }
  u32 slot_index = 0;
  u32 slot_found = false;

  // Get the slot
  for (usize i = 0; i < SCENE_CAP; i += 1) {
    if (scene->entity_slots[i].available) {
      slot_found = true;
      slot_index = (u32)i;
      break;
    }
  }

  if (!slot_found) {
    assert(false);
    return (Entity_Handle){0};
  }
  assert(slot_index >= 0);

  u32 backing_idx = (u32)scene->entity_count;
  scene->entity_slots[slot_index].backing_index = backing_idx;
  scene->entity_slots[slot_index].available = false;

  scene->entities[scene->entity_count] = *entity;
  scene->entities[scene->entity_count].slot_id = slot_index;
  // FIXME(nico): Well.. for now we just hope that all entities
  // We should rollback in case of failure to not leave the scene in a broken
  // state
  assert(
      init_entity(
          scene, &scene->entities[scene->entity_count], _game.global_allocator
      ) == Scene_Error_None
  );

  scene->entity_count += 1;

  Entity_Handle result = (Entity_Handle){
    .id = slot_index,
    .generation = scene->entity_slots[slot_index].generation,
  };

  usize lookup_count = scene->entity_lookup_count[entity->kind]++;
  scene->entity_lookup[entity->kind][lookup_count] = result;

  return result;
}

Entity *scene_get_entity_ptr(Scene *scene, Entity_Handle handle) {
  if (scene->entity_slots[handle.id].generation != handle.generation) {
    return nullptr;
  }

  u32 backing_idx = scene->entity_slots[handle.id].backing_index;
  return &scene->entities[backing_idx];
}

Entity_Handle scene_get_entity_handle(Scene *scene, Entity *entity) {
  return (Entity_Handle){
    .id = entity->slot_id,
    .generation = scene->entity_slots[entity->slot_id].generation,
  };
}

bool32 scene_remove_entity(Scene *scene, Entity_Handle handle) {
  if (scene->entity_slots[handle.id].generation != handle.generation) {
    return false;
  }
  Entity *entity = scene_get_entity_ptr(scene, handle);
  assert(entity != nullptr);
  destroy_entity(scene, entity);

#if defined(ENTITY_SCENE_GRAPH_IMPL)

  if (entity->parent.some) {
    unparent_entity(scene, handle);
  }

  // NOTE(nico): Idk what's the most logical way to remove the links. Either
  // reparent the children to the node entity's parent or put them back at the
  // root. I'll go with the later for now
  Entity_Handle_Option current = entity->first_child;
  while (current.some) {
    Entity *child_entity = scene_get_entity_ptr(scene, current.value);
    current = child_entity->next;

    child_entity->parent = none(Entity_Handle_Option);
    child_entity->next = none(Entity_Handle_Option);
  }

#endif

  u32 last_slot_idx = scene->entities[scene->entity_count - 1].slot_id;
  u32 removed_entity_idx = scene->entity_slots[handle.id].backing_index;

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

void scene_free_all_entites(Scene *scene) {
  for (i32 i = 0; i < SCENE_CAP; i += 1) {
    scene->entity_slots[i] = (Entity_Slot){.available = true};
  }
  scene->entity_count = 0;
}

// static bool32 entity_handle_eq(void *h1, void *h2) {
//   Entity_Handle *_h1 = (Entity_Handle *)h1;
//   Entity_Handle *_h2 = (Entity_Handle *)h2;

//   return _h1->id == _h2->id && _h1->generation == _h2->generation;
// }

// static u64 entity_handle_hash(void *key, usize size) {
//   (void)size;
//   Entity_Handle *h = (Entity_Handle *)key;

//   u64 hash = hash_fnv1a(&h->generation, sizeof(h->generation));
//   hash ^= hash_fnv1a(&h->id, sizeof(h->id));
//   return hash;
// }

u64 entity_handle_pack(Entity_Handle handle) {
  return (u64)handle.generation | ((u64)handle.id << 32);
}

Entity_Handle entity_handle_unpack(u64 bits) {
  return (Entity_Handle){
    .id = (u32)(bits >> 32),
    .generation = (u32)(bits & 0xffffffff),
  };
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

static const char tmp_program[] = {
#embed "../docs/examples/drive-to-target.asm"
  , '\0'
};

Scene_Error init_entity(Scene *scene, Entity *entity, Allocator allocator) {
  Entity_Handle handle = scene_get_entity_handle(scene, entity);

  switch (entity->kind) {
  case Entity_Kind_Machine: {
    entity->machine.navigation.owner = handle;
    entity->machine.motor.owner = handle;
    entity->machine.motor.max_speed = 1.f;

    Aet_Machine_Error error = aet_machine_init(
        &entity->machine.hardware,
        &(Aet_Machine_Create_Info){
          .clock_hz = 240,
          .ram_byte_cap = 4 * KILOBYTE,
          .available_devices = (1 << Aet_Device_Class_Navigation) |
                               (1 << Aet_Device_Class_Motor),
          .devices =
              {
                [Aet_Device_Class_Navigation] =
                    {
                      .data = scene,
                      .extra = entity_handle_pack(handle),
                      .read_u32_fn = navigation_device_read_from_register,
                      .write_u32_fn = navigation_device_write_to_register,
                    },
                [Aet_Device_Class_Motor] =
                    {
                      .data = scene,
                      .extra = entity_handle_pack(handle),
                      .read_u32_fn = motor_device_read_from_register,
                      .write_u32_fn = motor_device_write_to_register,
                    },
              },
        },
        allocator
    );

    if (error != Aet_Machine_Error_None) {
      return Scene_Error_Failed_To_Init_Entity;
    }

    Aet_Assembler_Result assembler_result =
        aet_assemble(from_c_str(tmp_program), allocator);
    if (!assembler_result.ok) {
      return Scene_Error_Failed_To_Init_Entity;
    }

    aet_cpu_load_program(&entity->machine.hardware.cpu, assembler_result.value);
  } break;
  case Entity_Kind_Invalid:
  case Entity_Kind_MAX:
    assert(false);
    return Scene_Error_Failed_To_Init_Entity;
  }

  return Scene_Error_None;
}

Scene_Error destroy_entity(Scene *scene, Entity *entity) {
  (void)scene;

  switch (entity->kind) {
  case Entity_Kind_Machine:
    aet_machine_destroy(&entity->machine.hardware);
    break;
  case Entity_Kind_Invalid:
  case Entity_Kind_MAX:
    assert(false);
    return Scene_Error_Failed_To_Destroy_Entity;
  }

  return Scene_Error_None;
}

static Raw_Camera *scene_active_camera(Scene *scene) {
  return &scene->orbit_camera.base;
}

//////////////////////////////////////////////
// Devices
// Public API to allow for tests
//////////////////////////////////////////////

// NOTE(nico): Maybe hide the getter/setter functions and only expose the
// register functions

Aet_Fault
navigation_device_read_from_register(rawptr data, u64 bits, u32 reg, u32 *out) {
  Scene *scene = (Scene *)data;
  Entity_Handle handle = entity_handle_unpack(bits);
  Entity *entity = scene_get_entity_ptr(scene, handle);
  assert(entity != nullptr && entity->kind == Entity_Kind_Machine);

  Navigation_Device *device = &entity->machine.navigation;

  Aet_Fault fault = Aet_Fault_None;
  switch (reg) {
  case Navigation_Register_Status: {
    Navigation_Flags flags = navigation_device_get_flags(scene, device);
    *out = (u32)flags;
  } break;
  case Navigation_Register_Position_X: {
    Vec3 position = or_return(
        navigation_device_get_position(scene, device),
        Aet_Fault_Internal_Device_Error
    );

    i32 x = (i32)position.x;
    *out = (u32)x;
  } break;
  case Navigation_Register_Position_Z: {
    Vec3 position = or_return(
        navigation_device_get_position(scene, device),
        Aet_Fault_Internal_Device_Error
    );

    i32 z = (i32)position.z;
    *out = (u32)z;
  } break;
  case Navigation_Register_Target_X: {
    Vec3 target = or_return(
        navigation_device_get_target_position(scene, device),
        Aet_Fault_Internal_Device_Error
    );

    i32 x = (i32)target.x;
    *out = (u32)x;
  } break;
  case Navigation_Register_Target_Z: {
    Vec3 target = or_return(
        navigation_device_get_target_position(scene, device),
        Aet_Fault_Internal_Device_Error
    );

    i32 z = (i32)target.z;
    *out = (u32)z;
  } break;
  case Navigation_Register_Distance: {
    f32 dist = or_return(
        navigation_device_get_target_distance(scene, device),
        Aet_Fault_Internal_Device_Error
    );

    *out = (u32)dist;
  } break;
  default:
    fault = Aet_Fault_Invalid_Address;
    break;
  }

  return fault;
}

Aet_Fault
navigation_device_write_to_register(rawptr data, u64 bits, u32 reg, u32 value) {
  Scene *scene = (Scene *)data;
  Entity_Handle handle = entity_handle_unpack(bits);
  Entity *entity = scene_get_entity_ptr(scene, handle);
  assert(entity != nullptr && entity->kind == Entity_Kind_Machine);

  Navigation_Device *device = &entity->machine.navigation;

  Aet_Fault fault = Aet_Fault_None;
  switch (reg) {
  case Navigation_Register_Distance:
  case Navigation_Register_Status:
  case Navigation_Register_Position_X:
  case Navigation_Register_Position_Z: {
    fault = Aet_Fault_Invalid_MMIO_Operation;
  } break;
  case Navigation_Register_Target_X: {
    Vec3 target = or_return(
        navigation_device_get_target_position(scene, device),
        Aet_Fault_Internal_Device_Error
    );

    i32 x = (i32)value;
    Navigation_Error error = navigation_device_set_target_position(
        scene, device, vec3((f32)x, target.y, target.z)
    );

    if (error != Navigation_Error_None) {
      fault = Aet_Fault_Internal_Device_Error;
    }
  } break;
  case Navigation_Register_Target_Z: {
    Vec3 target = or_return(
        navigation_device_get_target_position(scene, device),
        Aet_Fault_Internal_Device_Error
    );

    i32 z = (i32)value;
    Navigation_Error error = navigation_device_set_target_position(
        scene, device, vec3(target.x, target.y, (f32)z)
    );

    if (error != Navigation_Error_None) {
      fault = Aet_Fault_Internal_Device_Error;
    }
  } break;
  default:
    fault = Aet_Fault_Invalid_Address;
    break;
  }

  return fault;
}

Navigation_Flags
navigation_device_get_flags(Scene *scene, Navigation_Device *device) {
  assert(scene != nullptr);
  return device->flags;
}

Navigation_Position_Result
navigation_device_get_position(Scene *scene, Navigation_Device *device) {
  assert(scene != nullptr);
  Entity *owner = scene_get_entity_ptr(scene, device->owner);
  assert(owner != nullptr);

  if (owner == nullptr) {
    return err(Navigation_Position_Result, Navigation_Error_Internal_Failure);
  }

  return ok(Navigation_Position_Result, owner->position);
}

// NOTE(nico): Time will tell if this is the correct way to handle that part or
// if it creates more confusion than not. A program should check the
// Navigation_Flag_Target_Set bit flag before querying the target position
Navigation_Position_Result
navigation_device_get_target_position(Scene *scene, Navigation_Device *device) {
  assert(scene != nullptr);

  return ok(
      Navigation_Position_Result,
      device->target.some ? device->target.value : (Vec3){0}
  );
}

Navigation_Error navigation_device_set_target_position(
    Scene *scene, Navigation_Device *device, Vec3 target
) {
  assert(scene != nullptr);
  device->target.some = true;
  device->target.value = target;

  // TODO(nico): need to check terrain, but as it is not available yet, nothing
  // to check and error on

  // FIXME(nico): need a mechanism to clear this flags
  device->flags |= Navigation_Flag_Target_Set;

  return Navigation_Error_None;
}

Navigation_Distance_Result
navigation_device_get_target_distance(Scene *scene, Navigation_Device *device) {
  assert(scene != nullptr);
  if (!device->target.some) {
    return err(Navigation_Distance_Result, Navgiation_Error_Invalid_Target);
  }

  Entity *owner = scene_get_entity_ptr(scene, device->owner);
  assert(owner != nullptr);

  if (owner == nullptr) {
    return err(Navigation_Distance_Result, Navigation_Error_Internal_Failure);
  }

  f32 dist = vec3_length(vec3_sub(owner->position, device->target.value));
  return ok(Navigation_Distance_Result, dist);
}

Aet_Fault
motor_device_read_from_register(rawptr data, u64 bits, u32 reg, u32 *out) {
  Scene *scene = (Scene *)data;
  Entity_Handle handle = entity_handle_unpack(bits);
  Entity *entity = scene_get_entity_ptr(scene, handle);
  assert(entity != nullptr && entity->kind == Entity_Kind_Machine);

  Motor_Device *device = &entity->machine.motor;

  Aet_Fault fault = Aet_Fault_None;
  switch (reg) {
  case Motor_Register_Status: {
    Motor_Flags flags = motor_device_get_flags(scene, device);
    *out = flags;
  } break;
  default:
    fault = Aet_Fault_Invalid_Address;
  }

  return fault;
}

Aet_Fault
motor_device_write_to_register(rawptr data, u64 bits, u32 reg, u32 value) {
  Scene *scene = (Scene *)data;
  Entity_Handle handle = entity_handle_unpack(bits);
  Entity *entity = scene_get_entity_ptr(scene, handle);
  assert(entity != nullptr && entity->kind == Entity_Kind_Machine);

  Motor_Device *device = &entity->machine.motor;

  Aet_Fault fault = Aet_Fault_None;
  switch (reg) {
  case Motor_Register_Direction_X: {
    i32 sign = value == 0 ? 0 : (i32)value < 0 ? -1 : 1;
    f32 magnitude = (f32)sign * device->max_speed;

    entity->velocity.x = magnitude;
  } break;
  case Motor_Register_Direction_Z: {
    i32 sign = (i32)value < 0 ? -1 : 1;
    f32 magnitude = (f32)sign * device->max_speed;

    entity->velocity.z = magnitude;
  } break;
  case Motor_Register_Status:
  default:
    fault = Aet_Fault_Invalid_Address;
    break;
  }

  return fault;
}

Motor_Flags motor_device_get_flags(Scene *scene, Motor_Device *device) {
  assert(scene != nullptr);
  return device->flags;
}

Motor_Direction_Result
motor_device_get_direction(Scene *scene, Motor_Device *device) {
  assert(scene != nullptr);
  Entity *owner = scene_get_entity_ptr(scene, device->owner);
  assert(owner != nullptr);

  if (owner == nullptr) {
    return err(Motor_Direction_Result, Motor_Error_Internal_Failure);
  }

  return ok(Motor_Direction_Result, vec3_normalize(owner->velocity));
}

Motor_Error
motor_device_set_direction(Scene *scene, Motor_Device *device, Vec3 dir) {
  assert(scene != nullptr);
  Entity *owner = scene_get_entity_ptr(scene, device->owner);
  assert(owner != nullptr);

  if (owner == nullptr) {
    return Motor_Error_Internal_Failure;
  }

  owner->velocity = vec3_scale(vec3_normalize(dir), device->max_speed);
  return Motor_Error_None;
}

f32 motor_device_get_max_speed(Scene *scene, Motor_Device *device) {
  assert(scene != nullptr);
  return device->max_speed;
}

/////////////////////////////
// Main lifecycle hookss
/////////////////////////////
static void update_entities(Scene *scene, f32 dt) {
  for (usize i = 0; i < scene->entity_count; i += 1) {
    Entity *entity = &scene->entities[i];

    switch (entity->kind) {
    case Entity_Kind_Machine: {
      usize budget = entity->machine.hardware.cpu.clock_hz / TICK_RATE;
      aet_machine_run(&entity->machine.hardware, budget);
    } break;
    case Entity_Kind_Invalid:
    case Entity_Kind_MAX:
      assert(false);
    }

    // FIXME(nico): Need a flag on entities for velocity support. Need to think
    // about it
    entity->position =
        vec3_add(entity->position, vec3_scale(entity->velocity, dt));

    if (entity->flags & Entity_Flag_Removed) {
      scene_remove_entity(scene, scene_get_entity_handle(scene, entity));
      i -= 1;
    }
  }
}

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

    update_entities(scene, fixed_dt);

    _game.time_accumulator -= FRAME_NS;
    fixed_step_count += 1;
  }

  if (_game.time_accumulator >= FRAME_NS) {
    _game.time_accumulator %= FRAME_NS;
  }

  _game.frame_allocator.free_all(_game.frame_allocator);
}

////////////////////////////////////
// Rendering
////////////////////////////////////
static void scene_render(Scene *scene, Renderer *renderer) {
  for (usize i = 0; i < scene->entity_count; i += 1) {
    Entity *entity = &scene->entities[i];

    switch (entity->kind) {
    case Entity_Kind_Machine: {
      draw_model(
          renderer,
          &(Model_Draw_Info){
            .model = _db.model_table[entity->model],
            .transform = mat4_from_trs(
                entity->position, entity->rotation, vec3(1, 1, 1)
            ),
            .color = color(1, 1, 1, 1),
          }
      );
    } break;
    case Entity_Kind_Invalid:
    case Entity_Kind_MAX:
      assert(false);
    }
  }
}

#if defined(DEBUG)
static void draw_debug_ground_grid(
    Debug_Renderer *renderer, f32 extent, f32 step, Color c
) {
  for (f32 v = -extent; v <= extent; v += step) {
    draw_debug_line(renderer, vec3(-extent, 0.f, v), vec3(extent, 0.f, v), c);
    draw_debug_line(renderer, vec3(v, 0.f, -extent), vec3(v, 0.f, extent), c);
  }
}
#endif

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

  scene_render(scene, &_game.renderer);

  end_render(&_game.renderer);

#if defined(DEBUG)
  begin_debug_render(
      &_game.debug_renderer, &render_camera, &_game.renderer.depth_texture
  );

  draw_debug_ground_grid(
      &_game.debug_renderer, 20.f, 1.f, color(0.25f, 0.25f, 0.25f, 1.f)
  );

  end_debug_render(&_game.debug_renderer);
#endif

  update_view();
  render_view((f32)_game.app.window_width, (f32)_game.app.window_height);
}