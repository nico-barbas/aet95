#ifndef GAME_H
#define GAME_H

#include "core/allocator.h"
#include "core/camera.h"
#include "core/platform.h"
#include "core/strings.h"
#include "db.h"
#include "hal.h"
#include "render.h"
#include "render2d.h"

/*
  NOTE(nico):
  1 unit = 1 meter
*/

#define FRAME_ALLOCATOR_SIZE MEGABYTE * 128
#define STARTUP_WINDOW_WIDTH 1600
#define STARTUP_WINDOW_HEIGHT 900

#define TICK_RATE 120
#define DT (1.f / (f32)TICK_RATE)
#define FRAME_NS (1000000000ull / TICK_RATE)
#define MAX_FRAME_NS 250000000
#define MAX_FIXED_STEPS_PER_FRAME 8

#define SCENE_CAP 512
// #define ENTITY_SCENE_GRAPH_IMPL

#define ENTITY_MASK(k) ((Entity_Kind_Mask)(1u << (k)))
#define ENTITY_MASK_ALL ((Entity_Kind_Mask)(~0u))
#define ENTITY_MASK_NONE ((Entity_Kind_Mask)(0u))

typedef struct Scene Scene;

/////////////////////////////
/////////////////////////////
// Type declarations
/////////////////////////////
/////////////////////////////

///////////////////////
// Entities
///////////////////////
typedef struct Entity_Handle {
  i32 id;
  i32 generation;
} Entity_Handle;

typedef Option(Entity_Handle) Entity_Handle_Option;

typedef struct Entity_Slot {
  i32 generation;
  i32 backing_index;
  bool8 available;
} Entity_Slot;

typedef u32 Entity_Kind_Mask;
typedef enum Entity_Kind {
  Entity_Kind_Invalid,
  Entity_Kind_Machine,
  Entity_Kind_MAX,
} Entity_Kind;

typedef enum Entity_Flag {
  Entity_Flag_Removed = 1 << 0,
} Entity_Flag;
typedef u32 Entity_Flags;

typedef struct Machine_Entity {
  Aet_Machine hardware;
} Machine_Entity;

typedef struct Entity {
  Entity_Kind kind;
  Entity_Flags flags;
  i32 slot_id;

  Model_ID model;

#if defined(ENTITY_SCENE_GRAPH_IMPL)
  Entity_Handle_Option parent;
  Entity_Handle_Option first_child;
  Entity_Handle_Option last_child;
  Entity_Handle_Option next;
#endif

  Vec3 up;
  Vec3 position;
  Quat rotation;

  Vec3 velocity;

  union {
    Machine_Entity machine;
  };
} Entity;

//////////////////////////////////////////////
// Devices
// Public API to allow for tests
//////////////////////////////////////////////

/*
  NOTE(nico):
  All the device will need a stable pointer to the scene they
  belong to because of the vtable the machine relies on. Makes sense and that
  pointer should ALWAYS be valid

  ALWAYS assert in all the begining of all the deviced procedures
*/

typedef enum Navigation_Register : u32 {
  Navigation_Register_Status = 0,
  Navigation_Register_Position_X = 1,
  Navigation_Register_Position_Z = 2,
  Navigation_Register_Target_X = 3,
  Navigation_Register_Target_Z = 4,
  Navigation_Register_Distance = 5,
} Navigation_Register;

typedef u32 Navigation_Flags;
typedef enum Navigation_Flag {
  Navigation_Flag_Valid = 1 << 0,
  Navigation_Flag_Target_Set = 1 << 1,
  Navigation_Flag_Invalid_Target = 1 << 2,
} Navigation_Flag;

typedef enum Navigation_Error {
  Navigation_Error_None,
  Navigation_Error_Internal_Failure,
  Navgiation_Error_Invalid_Target,
} Navigation_Error;

typedef struct Navigation_Device {
  Scene *scene;
  Entity_Handle owner;
  Navigation_Flags flags;
  Option(Vec3) target;
} Navigation_Device;

typedef Result(Vec3, Navigation_Error) Navigation_Position_Result;
typedef Result(f32, Navigation_Error) Navigation_Distance_Result;

Aet_Fault navigation_device_read_from_register(rawptr data, u32 reg, u32 *out);
Aet_Fault navigation_device_write_to_register(rawptr data, u32 reg, u32 value);
Navigation_Flags navigation_device_get_flags(Navigation_Device *device);
Navigation_Position_Result
navigation_device_get_position(Navigation_Device *device);
Navigation_Position_Result
navigation_device_get_target_position(Navigation_Device *device);
Navigation_Error
navigation_device_set_target_position(Navigation_Device *device, Vec3 target);
Navigation_Distance_Result
navigation_device_get_target_distance(Navigation_Device *device);

typedef u32 Motor_Flags;
typedef enum Motor_Flag {
  Motor_Flag_Moving = 1 << 0,
  Motor_Flag_Blocked = 1 << 1,
} Motor_Flag;

typedef enum Motor_Error {
  Motor_Error_None,
  Motor_Error_Internal_Failure,
} Motor_Error;

typedef struct Motor_Device {
  Scene *scene;
  Entity_Handle owner;
  Motor_Flags flags;
  f32 max_speed;
} Motor_Device;

typedef Result(Vec3, Motor_Error) Motor_Direction_Result;

Motor_Flags motor_device_get_flags(Motor_Device *device);
Motor_Direction_Result motor_device_get_direction(Motor_Device *device);
Motor_Error motor_device_set_direction(Motor_Device *device, Vec3 dir);
f32 motor_device_get_max_speed(Motor_Device *device);

///////////////////////
// Scene
///////////////////////
struct Scene {
  usize entity_count;
  Entity entities[SCENE_CAP];
  Entity_Slot entity_slots[SCENE_CAP];
  Entity_Handle entity_lookup[Entity_Kind_MAX][SCENE_CAP];
  usize entity_lookup_count[Entity_Kind_MAX];

  // Cameras
  Orbit_Camera orbit_camera;
  Raw_Camera_State active_camera_state;

  // Cached state
};

typedef struct Game_State {
  App app;
  Renderer renderer;
  Renderer2D renderer_2d;
#if defined(DEBUG)
  Debug_Renderer debug_renderer;
#endif

  u64 time_accumulator;
  Scene scene;

  String version;
  Allocator global_allocator;
  Arena_Data frame_arena;
  Allocator frame_allocator;
} Game_State;

extern Game_State _game;

/////////////////////////////
/////////////////////////////
// Main lifecycle hookss
/////////////////////////////
/////////////////////////////
void init_game(void);
void close_game(void);
void update_game(void);
void render_game(void);

///////////////////////////////////
///////////////////////////////////
// Scene management procedures
///////////////////////////////////
///////////////////////////////////

/*
  Note(nico): [29-08-26] This code is older and should move to the Result
  paradigm that's used throughout the codebase now
*/
void init_scene(Scene *scene, Allocator allocator);
void destroy_scene(Scene *scene);
Entity_Handle scene_add_entity(Scene *scene, Entity *entity);
Entity *scene_get_entity_ptr(Scene *scene, Entity_Handle _handle);
Entity_Handle scene_get_entity_handle(Scene *scene, Entity *entity);
bool32 scene_remove_entity(Scene *scene, Entity_Handle _handle);
void scene_free_all_entites(Scene *scene);

#endif