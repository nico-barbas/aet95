#ifndef CORE_APP_H
#define CORE_APP_H

#include "core/array.h"
#include "core/log.h"
#include "core/math.h"
#include "core/strings.h"
#include "core/types.h"

#define APP_KEYBOARD_KEY_CAP 512
#define APP_MOUSE_BUTTON_CAP 12
#define APP_CHAR_BUFFER_CAP 512

/////////////////////////////
// Forward declarations
/////////////////////////////

// GLFW
typedef struct GLFWwindow GLFWwindow;

// WGPU opaque handles
typedef struct WGPUAdapterImpl *WGPUAdapter;
typedef struct WGPUBufferImpl *WGPUBuffer;
typedef struct WGPUCommandEncoderImpl *WGPUCommandEncoder;
typedef struct WGPUDeviceImpl *WGPUDevice;
typedef struct WGPUInstanceImpl *WGPUInstance;
typedef struct WGPUQueueImpl *WGPUQueue;
typedef struct WGPUSurfaceImpl *WGPUSurface;
typedef struct WGPUTextureImpl *WGPUTexture;
typedef struct WGPUTextureViewImpl *WGPUTextureView;
typedef struct WGPUSamplerImpl *WGPUSampler;

/////////////////////////////
// App
/////////////////////////////
typedef enum App_Window_Backend {
  App_Window_Backend_Auto,
  App_Window_Backend_X11,
  App_Window_Backend_Wayland,
} App_Window_Backend;

typedef enum App_GPU_Backend {
  App_GPU_Backend_Auto,
  App_GPU_Backend_Vulkan,
  APP_GPU_Backend_DX12,
} App_GPU_Backend;

typedef struct App {
  Allocator allocator;
  Logger logger;

  GLFWwindow *window_handle;
  App_Window_Backend window_backend;
  App_GPU_Backend gpu_backend;
  i32 window_width;
  i32 window_height;
  String window_title;

  // GPU resources
  WGPUInstance gpu_instance;
  WGPUSurface gpu_surface;
  u32 gpu_surface_format; // WGPUTextureFormat
  WGPUAdapter gpu_adapter;
  WGPUDevice gpu_device;
  WGPUQueue gpu_queue;
  WGPUTexture gpu_default_surface_texture;
  WGPUTextureView gpu_default_frame_texture_view;
  WGPUCommandEncoder gpu_current_command_encoder;

  // Runtime states
  bool32 running;
  u64 time_frequency;
  u64 last_time_ns;
  u64 current_time_ns;
  f64 last_time;
  f64 elapsed_time;
  Allocator backing_allocator;
  Allocator frame_allocator;
  Arena_Data frame_arena;

  // Input states
  Vec2 mouse_position;
  Vec2 previous_mouse_position;
  f32 mouse_scoll;
  utf8_char char_buffer[APP_CHAR_BUFFER_CAP];
  usize char_buffer_len;
  struct {
    bool8 previous;
    bool8 current;
  } mouse[APP_MOUSE_BUTTON_CAP];
  struct {
    bool8 previous;
    bool8 current;
  } keys[APP_KEYBOARD_KEY_CAP];
} App;

typedef struct App_Create_Info {
  App *app;
  i32 window_width;
  i32 window_height;
  String window_title;

  App_Window_Backend window_backend;
  App_GPU_Backend gpu_backend;

  Logger logger;
} App_Create_Info;

typedef enum Mouse_Button {
  Mouse_Button_Left = 0,
  Mouse_Button_Right = 1,
  Mouse_Button_Middle = 2,
} Mouse_Button;

typedef enum Keyboard_Key {
  Keyboard_Key_Null = 0, // Key: NULL, used for no key pressed
  // Alphanumeric keys
  Keyboard_Key_Apostrophe = 39,    // Key: '
  Keyboard_Key_Comma = 44,         // Key: ,
  Keyboard_Key_Minus = 45,         // Key: -
  Keyboard_Key_Period = 46,        // Key: .
  Keyboard_Key_Slash = 47,         // Key: /
  Keyboard_Key_Zero = 48,          // Key: 0
  Keyboard_Key_One = 49,           // Key: 1
  Keyboard_Key_Two = 50,           // Key: 2
  Keyboard_Key_Three = 51,         // Key: 3
  Keyboard_Key_Four = 52,          // Key: 4
  Keyboard_Key_Five = 53,          // Key: 5
  Keyboard_Key_Six = 54,           // Key: 6
  Keyboard_Key_Seven = 55,         // Key: 7
  Keyboard_Key_Eight = 56,         // Key: 8
  Keyboard_Key_Nine = 57,          // Key: 9
  Keyboard_Key_Semicolon = 59,     // Key: ;
  Keyboard_Key_Equal = 61,         // Key: =
  Keyboard_Key_A = 65,             // Key: A | a
  Keyboard_Key_B = 66,             // Key: B | b
  Keyboard_Key_C = 67,             // Key: C | c
  Keyboard_Key_D = 68,             // Key: D | d
  Keyboard_Key_E = 69,             // Key: E | e
  Keyboard_Key_F = 70,             // Key: F | f
  Keyboard_Key_G = 71,             // Key: G | g
  Keyboard_Key_H = 72,             // Key: H | h
  Keyboard_Key_I = 73,             // Key: I | i
  Keyboard_Key_J = 74,             // Key: J | j
  Keyboard_Key_K = 75,             // Key: K | k
  Keyboard_Key_L = 76,             // Key: L | l
  Keyboard_Key_M = 77,             // Key: M | m
  Keyboard_Key_N = 78,             // Key: N | n
  Keyboard_Key_O = 79,             // Key: O | o
  Keyboard_Key_P = 80,             // Key: P | p
  Keyboard_Key_Q = 81,             // Key: Q | q
  Keyboard_Key_R = 82,             // Key: R | r
  Keyboard_Key_S = 83,             // Key: S | s
  Keyboard_Key_T = 84,             // Key: T | t
  Keyboard_Key_U = 85,             // Key: U | u
  Keyboard_Key_V = 86,             // Key: V | v
  Keyboard_Key_W = 87,             // Key: W | w
  Keyboard_Key_X = 88,             // Key: X | x
  Keyboard_Key_Y = 89,             // Key: Y | y
  Keyboard_Key_Z = 90,             // Key: Z | z
  Keyboard_Key_Left_Bracket = 91,  // Key: [
  Keyboard_Key_Backslash = 92,     // Key: '\'
  Keyboard_Key_Right_Bracket = 93, // Key: ]
  Keyboard_Key_Grave = 96,         // Key: `
  // Function keys
  Keyboard_Key_Space = 32,          // Key: Space
  Keyboard_Key_Escape = 256,        // Key: Esc
  Keyboard_Key_Enter = 257,         // Key: Enter
  Keyboard_Key_Tab = 258,           // Key: Tab
  Keyboard_Key_Backspace = 259,     // Key: Backspace
  Keyboard_Key_Insert = 260,        // Key: Ins
  Keyboard_Key_Delete = 261,        // Key: Del
  Keyboard_Key_Right = 262,         // Key: Cursor right
  Keyboard_Key_Left = 263,          // Key: Cursor left
  Keyboard_Key_Down = 264,          // Key: Cursor down
  Keyboard_Key_Up = 265,            // Key: Cursor up
  Keyboard_Key_Page_Up = 266,       // Key: Page up
  Keyboard_Key_Page_Down = 267,     // Key: Page down
  Keyboard_Key_Home = 268,          // Key: Home
  Keyboard_Key_End = 269,           // Key: End
  Keyboard_Key_Caps_Lock = 280,     // Key: Caps lock
  Keyboard_Key_Scroll_Lock = 281,   // Key: Scroll down
  Keyboard_Key_Num_Lock = 282,      // Key: Num lock
  Keyboard_Key_Print_Screen = 283,  // Key: Print screen
  Keyboard_Key_Pause = 284,         // Key: Pause
  Keyboard_Key_F1 = 290,            // Key: F1
  Keyboard_Key_F2 = 291,            // Key: F2
  Keyboard_Key_F3 = 292,            // Key: F3
  Keyboard_Key_F4 = 293,            // Key: F4
  Keyboard_Key_F5 = 294,            // Key: F5
  Keyboard_Key_F6 = 295,            // Key: F6
  Keyboard_Key_F7 = 296,            // Key: F7
  Keyboard_Key_F8 = 297,            // Key: F8
  Keyboard_Key_F9 = 298,            // Key: F9
  Keyboard_Key_F10 = 299,           // Key: F10
  Keyboard_Key_F11 = 300,           // Key: F11
  Keyboard_Key_F12 = 301,           // Key: F12
  Keyboard_Key_Left_Shift = 340,    // Key: Shift left
  Keyboard_Key_Left_Control = 341,  // Key: Control left
  Keyboard_Key_Left_Alt = 342,      // Key: Alt left
  Keyboard_Key_Left_Super = 343,    // Key: Super left
  Keyboard_Key_Right_Shift = 344,   // Key: Shift right
  Keyboard_Key_Right_Control = 345, // Key: Control right
  Keyboard_Key_Right_Alt = 346,     // Key: Alt right
  Keyboard_Key_Right_Super = 347,   // Key: Super right
  Keyboard_Key_Kb_Menu = 348,       // Key: KB menu
  // Keypad keys
  Keyboard_Key_Kp_0 = 320,        // Key: Keypad 0
  Keyboard_Key_Kp_1 = 321,        // Key: Keypad 1
  Keyboard_Key_Kp_2 = 322,        // Key: Keypad 2
  Keyboard_Key_Kp_3 = 323,        // Key: Keypad 3
  Keyboard_Key_Kp_4 = 324,        // Key: Keypad 4
  Keyboard_Key_Kp_5 = 325,        // Key: Keypad 5
  Keyboard_Key_Kp_6 = 326,        // Key: Keypad 6
  Keyboard_Key_Kp_7 = 327,        // Key: Keypad 7
  Keyboard_Key_Kp_8 = 328,        // Key: Keypad 8
  Keyboard_Key_Kp_9 = 329,        // Key: Keypad 9
  Keyboard_Key_Kp_Decimal = 330,  // Key: Keypad .
  Keyboard_Key_Kp_Divide = 331,   // Key: Keypad /
  Keyboard_Key_Kp_Multiply = 332, // Key: Keypad *
  Keyboard_Key_Kp_Subtract = 333, // Key: Keypad -
  Keyboard_Key_Kp_Add = 334,      // Key: Keypad +
  Keyboard_Key_Kp_Enter = 335,    // Key: Keypad Enter
  Keyboard_Key_Kp_Equal = 336,    // Key: Keypad =
  // Android key buttons
  Keyboard_Key_Back = 4,        // Key: Android back button
  Keyboard_Key_Menu = 5,        // Key: Android menu button
  Keyboard_Key_Volume_Up = 24,  // Key: Android volume up button
  Keyboard_Key_Volume_Down = 25 // Key: Android volume down button
} Keyboard_Key;

typedef Array(utf8_char) Text_Array;

bool32 init_app(App_Create_Info *info, Allocator allocator);
void close_app(App *app);

bool32 app_update(App *app);
void app_begin_frame(App *app);
void app_end_frame(App *app);

u64 app_get_current_time_ns();
u64 app_get_last_time_ns();
f32 app_get_elapsed_time();
Vec2 app_get_window_size();

Vec2 app_mouse_position();
Vec2 app_mouse_delta();
f32 app_mouse_scroll();
void app_capture_mouse(bool32 on);

bool8 app_mouse_pressed(Mouse_Button button);
bool8 app_mouse_just_pressed(Mouse_Button button);
bool8 app_key_pressed(Keyboard_Key key);
bool8 app_key_just_pressed(Keyboard_Key key);
Text_Array app_chars_pressed();

/////////////////////////////
// GPU Buffer management
/////////////////////////////
typedef u32 GPU_Buffer_Usage;
typedef enum GPU_Buffer_Usage_Kind {
  GPU_Buffer_Usage_None = 0x0000000000000000,
  GPU_Buffer_Usage_Copy_Src = 0x0000000000000004,
  GPU_Buffer_Usage_Copy_Dst = 0x0000000000000008,
  GPU_Buffer_Usage_Index = 0x0000000000000010,
  GPU_Buffer_Usage_Vertex = 0x0000000000000020,
  GPU_Buffer_Usage_Uniform = 0x0000000000000040,
  GPU_Buffer_Usage_Storage = 0x0000000000000080,
} GPU_Buffer_Usage_Kind;

typedef struct GPU_Buffer {
  WGPUBuffer handle;
  GPU_Buffer_Usage usage;
  usize cap;
  usize used;
  usize align;
} GPU_Buffer;

typedef struct GPU_Buffer_Create_Info {
  GPU_Buffer_Usage usage;
  usize size;
} GPU_Buffer_Create_Info;

typedef struct GPU_Buffer_Memory {
  GPU_Buffer *buffer;
  usize offset;
  usize size;
} GPU_Buffer_Memory;

GPU_Buffer make_gpu_buffer(GPU_Buffer_Create_Info *info);
void destroy_gpu_buffer(GPU_Buffer buffer);

bool32 gpu_buffer_is_valid(GPU_Buffer buffer);
bool32 gpu_buffer_memory_is_valid(GPU_Buffer_Memory memory);

void gpu_buffer_reset(GPU_Buffer *buffer);
GPU_Buffer_Memory gpu_buffer_alloc(GPU_Buffer *buffer, usize size);
GPU_Buffer_Memory gpu_buffer_append(GPU_Buffer *buffer, void *data, usize size);

void gpu_buffer_write(GPU_Buffer_Memory memory, void *data, usize size);

/////////////////////////////
// GPU Texture management
/////////////////////////////
typedef enum GPU_Texture_Space {
  GPU_Texture_Space_Linear,
  GPU_Texture_Space_sRGB,
} GPU_Texture_Space;

typedef enum GPU_Texture_Format {
  GPU_Texture_Format_R8Unorm = 0x00000001,
  GPU_Texture_Format_R16Unorm = 0x00000005,
  GPU_Texture_Format_RG8Unorm = 0x0000000A,
  GPU_Texture_Format_RG16Unorm = 0x00000011,
  GPU_Texture_Format_RGBA8Unorm = 0x00000016,
  GPU_Texture_Format_RGBA8UnormSrgb = 0x00000017,
  GPU_Texture_Format_RGBA16Unorm = 0x00000024,
  GPU_Texture_Format_Depth24Plus = 0x00000012,
} GPU_Texture_Format;

typedef struct GPU_Texture {
  WGPUTexture handle;
  GPU_Texture_Format format;
  GPU_Texture_Space space;
  u32 width;
  u32 height;
} GPU_Texture;

typedef struct GPU_Texture_Create_Info {
  GPU_Texture_Space space;
  enum {
    GPU_Texture_Create_Info_File,
    GPU_Texture_Create_Info_Memory,
    GPU_Texture_Create_Info_Raw_Memory,
  } kind;
  union {
    String file_path;
    struct {
      byte *data;
      u32 width;
      u32 height;
      u64 channels;
    } raw;
  };
} GPU_Texture_Create_Info;

typedef enum GPU_Sampler_Filter {
  GPU_Sampler_Filter_Nearest = 0x00000001,
  GPU_Sampler_Filter_Linear = 0x00000002,
} GPU_Sampler_Filter;

typedef enum GPU_Sampler_Wrap {
  GPU_Sampler_Wrap_Clamp = 0x00000001,
  GPU_Sampler_Wrap_Repeat = 0x00000002,
} GPU_Sampler_Wrap;

typedef struct GPU_Sampler {
  WGPUSampler handle;
  GPU_Sampler_Filter filter;
  GPU_Sampler_Wrap wrap;
} GPU_Sampler;

typedef struct GPU_Sampler_Create_Info {
  GPU_Sampler_Filter filter;
  GPU_Sampler_Wrap wrap;
} GPU_Sampler_Create_Info;

GPU_Texture make_gpu_texture(GPU_Texture_Create_Info *info);
GPU_Texture make_gpu_depth_texture(u32 width, u32 height);
void destroy_gpu_texture(GPU_Texture texture);

bool32 gpu_texture_is_valid(GPU_Texture texture);
WGPUTextureView gpu_texture_derive_view(GPU_Texture texture);

GPU_Sampler make_gpu_sampler(GPU_Sampler_Create_Info *info);
void destroy_gpu_sampler(GPU_Sampler sampler);

bool32 gpu_sampler_is_value(GPU_Sampler sampler);

////////////////////////////////////
// GPU Pipeline management
////////////////////////////////////
typedef struct WGPURenderPipelineImpl *WGPURenderPipeline;
typedef struct WGPUPipelineLayoutImpl *WGPUPipelineLayout;
typedef struct WGPUBindGroupImpl *WGPUBindGroup;
typedef struct WGPUBindGroupLayoutImpl *WGPUBindGroupLayout;

typedef enum GPU_Blend_Factor {
  GPU_Blend_Factor_Zero = 0x00000001,
  GPU_Blend_Factor_One = 0x00000002,
  GPU_Blend_Factor_Src = 0x00000003,
  GPU_Blend_Factor_One_Minus_Src = 0x00000004,
  GPU_Blend_Factor_Src_Alpha = 0x00000005,
  GPU_Blend_Factor_One_Minus_Src_Alpha = 0x00000006,
  GPU_Blend_Factor_Dst = 0x00000007,
  GPU_Blend_Factor_One_Minus_Dst = 0x00000008,
  GPU_Blend_Factor_Dst_Alpha = 0x00000009,
  GPU_Blend_Factor_One_Minus_Dst_Alpha = 0x0000000A,
  GPU_Blend_Factor_Src_Alpha_Saturated = 0x0000000B,
  GPU_Blend_Factor_Constant = 0x0000000C,
  GPU_Blend_Factor_One_Minus_Constant = 0x0000000D,
} GPU_Blend_Factor;

typedef enum GPU_Primitive {
  GPU_Primitive_Line = 0x00000002,
  GPU_Primitive_Triangle = 0x00000004,
} GPU_Primitive;

typedef enum GPU_Blend_Op {
  GPU_Blend_Op_Add = 0x00000001,
  GPU_Blend_Op_Subtract = 0x00000002,
  GPU_Blend_Op_Reverse_Subtract = 0x00000003,
  GPU_Blend_Op_Min = 0x00000004,
  GPU_Blend_Op_Max = 0x00000005,
} GPU_Blend_Op;

typedef struct GPU_Blend_Component {
  GPU_Blend_Op operation;
  GPU_Blend_Factor src_factor;
  GPU_Blend_Factor dst_factor;
} GPU_Blend_Component;

typedef struct GPU_Blend_State {
  GPU_Blend_Component color;
  GPU_Blend_Component alpha;
} GPU_Blend_State;

typedef enum GPU_Shader_Data_Kind {
  GPU_Shader_Data_Kind_Uniform,
  GPU_Shader_Data_Kind_Storage,
  GPU_Shader_Data_Kind_Texture,
  GPU_Shader_Data_Kind_Sampler,
} GPU_Shader_Data_Kind;

typedef struct GPU_Shader_Data_Info {
  GPU_Shader_Data_Kind kind;
  usize associated_size;
  u32 binding_index;
} GPU_Shader_Data_Info;

typedef struct GPU_Bind_Group_Info {
  WGPUBindGroupLayout handle;
  Array(GPU_Shader_Data_Info) shader_data_infos;
  u32 group_index;
} GPU_Bind_Group_Info;

typedef struct GPU_Pipeline {
  WGPURenderPipeline handle;
  WGPUPipelineLayout layout_handle;
  Array(GPU_Bind_Group_Info) bind_group_infos;
  Allocator allocator;
} GPU_Pipeline;

typedef struct GPU_Shader_Source {
  enum {
    GPU_Shader_Source_File,
    GPU_Shader_Source_Raw,
  } kind;
  union {
    String file_path;
    String data;
  };
} GPU_Shader_Source;

typedef enum GPU_Vertex_Component {
  GPU_Vertex_Component_Float = 0x0000001C,
  GPU_Vertex_Component_Vector2 = 0x0000001D,
  GPU_Vertex_Component_Vector3 = 0x0000001E,
  GPU_Vertex_Component_Vector4 = 0x0000001F,
  GPU_Vertex_Component_Uint32 = 0x00000020,
} GPU_Vertex_Component;

typedef struct GPU_Vertex_Attribute {
  void *_padding;
  GPU_Vertex_Component component_format;
  u64 offset;
  u32 shader_location;
} GPU_Vertex_Attribute;

typedef struct GPU_Bind_Group_Create_Info {
  Array(GPU_Shader_Data_Info) shader_data_infos;
} GPU_Bind_Group_Create_Info;

typedef struct GPU_Pipeline_Create_Info {
  GPU_Shader_Source shader_source;
  GPU_Vertex_Attribute *vertex_attributes;
  usize vertex_attribute_count;
  usize vertex_stride;

  Array(GPU_Bind_Group_Create_Info) bind_groups;
  Array(GPU_Texture_Format) color_targets;

  Option(GPU_Primitive) primitive;

  GPU_Blend_State *blend_state;
  bool32 depth_test;
} GPU_Pipeline_Create_Info;

typedef struct GPU_Shader_Data_Source {
  enum {
    GPU_Shader_Data_Source_Buffer,
    GPU_Shader_Data_Source_Memory,
    GPU_Shader_Data_Source_Texture,
    GPU_Shader_Data_Source_Sampler,
  } variant;
  union {
    GPU_Buffer *buffer;
    GPU_Buffer_Memory memory;
    GPU_Texture texture;
    GPU_Sampler sampler;
  };
} GPU_Shader_Data_Source;

typedef struct GPU_Shader_Data {
  u32 binding_index;
  GPU_Shader_Data_Kind kind;
  union {
    GPU_Buffer_Memory memory;
    WGPUTextureView texture_view;
    GPU_Sampler sampler;
  };
} GPU_Shader_Data;

typedef Array(GPU_Shader_Data_Source) GPU_Shader_Data_Source_Array;
typedef Array(GPU_Shader_Data_Source_Array) GPU_Shader_Data_Source_Array_2D;
typedef Array(GPU_Shader_Data) GPU_Shader_Data_Array;

typedef struct GPU_Bind_Group {
  WGPUBindGroup handle;
  Array(GPU_Shader_Data) shader_datas;
  u32 group_index;
} GPU_Bind_Group;

typedef Array(GPU_Bind_Group) GPU_Bind_Group_Array;

// Vertex attribute helpers — Usage: VERTEX_ATTR_F32x2(My_Vertex, position, 0)
#define VERTEX_ATTR_F32(type, field, loc)                                      \
  (GPU_Vertex_Attribute) {                                                     \
    .component_format = GPU_Vertex_Component_Float,                            \
    .offset = offsetof(type, field), .shader_location = (loc)                  \
  }
#define VERTEX_ATTR_F32x2(type, field, loc)                                    \
  (GPU_Vertex_Attribute) {                                                     \
    .component_format = GPU_Vertex_Component_Vector2,                          \
    .offset = offsetof(type, field), .shader_location = (loc)                  \
  }
#define VERTEX_ATTR_F32x3(type, field, loc)                                    \
  (GPU_Vertex_Attribute) {                                                     \
    .component_format = GPU_Vertex_Component_Vector3,                          \
    .offset = offsetof(type, field), .shader_location = (loc)                  \
  }
#define VERTEX_ATTR_F32x4(type, field, loc)                                    \
  (GPU_Vertex_Attribute) {                                                     \
    .component_format = GPU_Vertex_Component_Vector4,                          \
    .offset = offsetof(type, field), .shader_location = (loc)                  \
  }
#define VERTEX_ATTR_U32(type, field, loc)                                      \
  (GPU_Vertex_Attribute) {                                                     \
    .component_format = GPU_Vertex_Component_Uint32,                           \
    .offset = offsetof(type, field), .shader_location = (loc)                  \
  }

// Shader binding descriptors — used inside GPU_Bind_Group_Create_Info
#define SHADER_UNIFORM(binding, size)                                          \
  (GPU_Shader_Data_Info) {                                                     \
    .kind = GPU_Shader_Data_Kind_Uniform, .binding_index = (binding),          \
    .associated_size = (size)                                                  \
  }
#define SHADER_STORAGE(binding, size)                                          \
  (GPU_Shader_Data_Info) {                                                     \
    .kind = GPU_Shader_Data_Kind_Storage, .binding_index = (binding),          \
    .associated_size = (size)                                                  \
  }
#define SHADER_TEXTURE(binding)                                                \
  (GPU_Shader_Data_Info) {                                                     \
    .kind = GPU_Shader_Data_Kind_Texture, .binding_index = (binding)           \
  }
#define SHADER_SAMPLER(binding)                                                \
  (GPU_Shader_Data_Info) {                                                     \
    .kind = GPU_Shader_Data_Kind_Sampler, .binding_index = (binding)           \
  }

// Bind group data sources — used inside GPU_Shader_Data_Source_Array
#define BIND_BUFFER(ptr)                                                       \
  (GPU_Shader_Data_Source) {                                                   \
    .variant = GPU_Shader_Data_Source_Buffer, .buffer = (ptr)                  \
  }
#define BIND_MEMORY(mem)                                                       \
  (GPU_Shader_Data_Source) {                                                   \
    .variant = GPU_Shader_Data_Source_Memory, .memory = (mem)                  \
  }
#define BIND_TEXTURE(tex)                                                      \
  (GPU_Shader_Data_Source) {                                                   \
    .variant = GPU_Shader_Data_Source_Texture, .texture = (tex)                \
  }
#define BIND_SAMPLER(s)                                                        \
  (GPU_Shader_Data_Source) {                                                   \
    .variant = GPU_Shader_Data_Source_Sampler, .sampler = (s)                  \
  }

GPU_Pipeline
make_gpu_pipeline(GPU_Pipeline_Create_Info *info, Allocator allocator);
void destroy_gpu_pipeline(GPU_Pipeline pipeline);

bool32 gpu_pipeline_is_valid(GPU_Pipeline pipeline);

GPU_Bind_Group gpu_pipeline_derive_bind_group(
    GPU_Pipeline pipeline,
    GPU_Shader_Data_Source_Array sources,
    u32 group_index,
    Allocator allocator
);
GPU_Bind_Group_Array gpu_pipeline_derive_bind_group_array(
    GPU_Pipeline pipeline,
    GPU_Shader_Data_Source_Array_2D sources,
    Allocator allocator
);
void destroy_gpu_bind_group(GPU_Bind_Group bind_group);
void destroy_gpu_bind_groups(GPU_Bind_Group_Array bind_groups);

////////////////////////////////////
// GPU Render target management
////////////////////////////////////
// Conservative estimate about the capabilities of most gpus
#define RENDER_TARGET_COLOR_ATTACHMENT_CAP 4

typedef struct GPU_Render_Target {
  WGPUTexture color_textures[RENDER_TARGET_COLOR_ATTACHMENT_CAP];
  WGPUTextureView color_views[RENDER_TARGET_COLOR_ATTACHMENT_CAP];
  GPU_Texture_Format color_formats[RENDER_TARGET_COLOR_ATTACHMENT_CAP];
  usize color_attachment_len;
  WGPUTexture depth_texture;
  WGPUTextureView depth_view;
  u32 width;
  u32 height;
} GPU_Render_Target;

typedef struct GPU_Render_Target_Create_Info {
  u32 width;
  u32 height;
  bool32 depth;
  Array(GPU_Texture_Format) color_formats;
} GPU_Render_Target_Create_Info;

GPU_Render_Target make_gpu_render_target(GPU_Render_Target_Create_Info *info);
void destroy_gpu_render_target(GPU_Render_Target target);

bool32 gpu_render_target_is_valid(GPU_Render_Target target);
GPU_Texture
gpu_render_target_color_texture(GPU_Render_Target *target, u32 index);

////////////////////////////////////
// GPU Render pass management
////////////////////////////////////
typedef struct WGPURenderPassEncoderImpl *WGPURenderPassEncoder;

typedef struct GPU_Render_Pass {
  WGPURenderPassEncoder handle;
  WGPUTextureView _depth_view; // non-null when depth was derived from a
                               // swapchain-path GPU_Texture
} GPU_Render_Pass;

typedef struct GPU_Render_Pass_Create_Info {
  Option(GPU_Render_Target) target;
  Option(GPU_Texture) depth; // optional depth for the swapchain path
  Color clear_colors[RENDER_TARGET_COLOR_ATTACHMENT_CAP];
  bool32 clear;
} GPU_Render_Pass_Create_Info;

GPU_Render_Pass gpu_render_pass_begin(GPU_Render_Pass_Create_Info *info);
void gpu_render_pass_end(GPU_Render_Pass pass);

#define gpu_render_pass(info)                                                  \
  for (GPU_Render_Pass pass = gpu_render_pass_begin((info)), *_once = &pass;   \
       _once;                                                                  \
       gpu_render_pass_end(pass), _once = NULL)

void gpu_render_pass_bind_pipeline(GPU_Render_Pass pass, GPU_Pipeline pipeline);
void gpu_render_pass_bind_group(GPU_Render_Pass pass, GPU_Bind_Group group);
void gpu_render_pass_bind_groups(
    GPU_Render_Pass pass, GPU_Bind_Group_Array groups
);

void gpu_render_pass_draw_indexed(
    GPU_Render_Pass pass,
    GPU_Buffer_Memory vertices,
    GPU_Buffer_Memory indices,
    usize draw_count,
    usize instance_index
);
void gpu_render_pass_draw(
    GPU_Render_Pass pass, GPU_Buffer_Memory vertices, usize vertex_count
);

#endif
