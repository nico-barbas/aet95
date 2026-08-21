#ifndef GAME_H
#define GAME_H

#include "core/allocator.h"
#include "core/camera.h"
#include "core/imgui.h"
#include "core/platform.h"
#include "core/strings.h"
#include "render.h"

#define FRAME_ALLOCATOR_SIZE MEGABYTE * 128
#define STARTUP_WINDOW_WIDTH 1280
#define STARTUP_WINDOW_HEIGHT 720

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

/////////////////////////////
// Type declarations
/////////////////////////////

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
  Entity_Kind_Vehicle,
  Entity_Kind_MAX,
} Entity_Kind;

typedef enum Entity_Flag {
  Entity_Flag_Removed = 1 << 0,
} Entity_Flag;
typedef u32 Entity_Flags;

typedef struct Entity {
  Entity_Kind kind;
  Entity_Flags flags;
  i32 slot_id;

#if defined(ENTITY_SCENE_GRAPH_IMPL)
  Entity_Handle_Option parent;
  Entity_Handle_Option first_child;
  Entity_Handle_Option last_child;
  Entity_Handle_Option next;
#endif

  Vec3 position;
  Quat rotation;
} Entity;

typedef struct Scene {
  usize entity_count;
  Entity entities[SCENE_CAP];
  Entity_Slot entity_slots[SCENE_CAP];
  Entity_Handle entity_lookup[Entity_Kind_MAX][SCENE_CAP];
  usize entity_lookup_count[Entity_Kind_MAX];

  // Cameras
  Orbit_Camera orbit_camera;
  Raw_Camera_State active_camera_state;

  // Cached state
} Scene;

typedef struct Game_State {
  App app;
  Renderer renderer;
  Renderer2D renderer_2d;
#if defined(DEBUG)
  Debug_Renderer debug_renderer;
#endif

  u64 time_accumulator;
  Scene scene;
  Element_Context el_ctx;

  String version;
  Allocator global_allocator;
  Arena_Data frame_arena;
  Allocator frame_allocator;
} Game_State;

extern Game_State _game;

/////////////////////////////
// Main lifecycle hookss
/////////////////////////////
void init_game(void);
void close_game(void);
void update_game(void);
void render_game(void);

///////////////////////////////////
// Scene management procedures
///////////////////////////////////
void init_scene(Scene *scene, Allocator allocator);
void destroy_scene(Scene *scene);
Entity_Handle add_entity(Scene *scene, Entity *entity);
Entity *get_entity_ptr(Scene *scene, Entity_Handle _handle);
Entity_Handle get_entity_handle(Scene *scene, Entity *entity);
bool32 remove_entity(Scene *scene, Entity_Handle _handle);
void free_all_entites(Scene *scene);

bool32 entity_handle_eq(void *h1, void *h2);
u64 entity_handle_hash(void *key, usize size);

#endif