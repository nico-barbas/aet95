#include "core/platform.h"

#include "core/log.h"
#include "core/math.h"
#include "core/types.h"
#include "stb_image.h"

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

/*
  FIXME(nico):
  - Render-target capacity assertion is reversed.
  - Bind-group handles and derived texture views leak.
  - Depth24Plus has an outdated enum value.
  - Buffer slices retain pointers to movable wrappers.
  - Texture channel accounting is incorrect after forced RGBA decoding.
  - Pipeline entry points and shader visibility are hardcoded.
  - Several partial failures leak resources.
  - GPU parent resources are released before children.
*/

#if defined(PLATFORM_WEB)
#include <GLFW/glfw3.h>
#include <emscripten/emscripten.h>
#include <webgpu/webgpu.h>

WGPUCallbackMode callback_mode = WGPUCallbackMode_AllowProcessEvents;
#elif defined(_WIN32)

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#include <webgpu/webgpu.h>
#include <webgpu/wgpu.h>

WGPUCallbackMode callback_mode = WGPUCallbackMode_WaitAnyOnly;

#elif defined(__linux__)

#define GLFW_EXPOSE_NATIVE_X11
#define GLFW_EXPOSE_NATIVE_WAYLAND
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#include <webgpu/webgpu.h>
#include <webgpu/wgpu.h>

static WGPUCallbackMode callback_mode = WGPUCallbackMode_WaitAnyOnly;

#endif

#define FRAME_ARENA_SIZE (50 * MEGABYTE)

static App *_app = nullptr;

static void platform_assert_ok(Logger *logger) {
#if defined(DEBUG)
  const char *msg = nullptr;
  i32 err = glfwGetError(&msg);
  if (err != GLFW_NO_ERROR) {
    log_error(logger, msg);
    assert(false);
  }
#endif
}

static u64 platform_get_timer_now_ns(App *app) {
#if defined(PLATFORM_WEB)
  return (u64)(emscripten_get_now() * 1000000.0);
#else
  return (glfwGetTimerValue() * 1000000000ull) / app->time_frequency;
#endif
}

static void gpu_adapter_callback(
    WGPURequestAdapterStatus status,
    WGPUAdapter adapter,
    WGPUStringView message,
    WGPU_NULLABLE void *userdata1,
    WGPU_NULLABLE void *userdata2
);

static void gpu_device_callback(
    WGPURequestDeviceStatus status,
    WGPUDevice device,
    WGPUStringView message,
    WGPU_NULLABLE void *userdata1,
    WGPU_NULLABLE void *userdata2
);

static void gpu_error_callback(
    WGPUDevice const *device,
    WGPUErrorType type,
    WGPUStringView message,
    WGPU_NULLABLE void *userdata1,
    WGPU_NULLABLE void *userdata2
);

static void input_mouse_button_callback(
    GLFWwindow *window, i32 button, i32 action, i32 mods
);

static void input_key_callback(
    GLFWwindow *window, i32 key, i32 scancode, i32 action, i32 mods
);

static void
input_mouse_scroll_callback(GLFWwindow *window, f64 xoffset, f64 yoffset);

static void
input_char_pressed_callback(GLFWwindow *window, utf8_char codepoint);

bool32 init_app(App_Create_Info *info, Allocator allocator) {
  _Static_assert(
      sizeof(GPU_Vertex_Attribute) == sizeof(WGPUVertexAttribute),
      "GPU_Vertex_Attribute size mismatch with WGPUVertexAttribute"
  );

  // FIXME(nico): This is not the best. Fine for now
  assert(info->logger.log_proc != nullptr);

  App *app = info->app;
  *app = (App){0};

  app->allocator = allocator;
  app->logger = info->logger;
  app->backing_allocator = allocator;

  init_arena(
      &app->frame_arena,
      allocator.alloc(allocator, FRAME_ARENA_SIZE).allocation,
      FRAME_ARENA_SIZE
  );
  app->frame_allocator = arena_allocator(&app->frame_arena);

#if defined(_WIN32)
  if (info->window_backend != App_Window_Backend_Auto) {
    log_error(
        &app->logger,
        "Invalid Window backend selected. On Windows, only "
        "App_Window_Backend_Auto is supported"
    );
    return false;
  }

  glfwInitHint(GLFW_PLATFORM, GLFW_ANY_PLATFORM);
#elif defined(__linux__)
  // FIXME(nico): work on the logger to have string parsing
  // The string library already handle that so just use this
  switch (info->window_backend) {
  case App_Window_Backend_Auto:
    log_debug(&app->logger, "Window backend auto selected");
    glfwInitHint(GLFW_PLATFORM, GLFW_ANY_PLATFORM);
    break;
  case App_Window_Backend_X11:
    log_debug(&app->logger, "Window backend x11 selected");
    glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_X11);
    break;
  case App_Window_Backend_Wayland:
    log_debug(&app->logger, "Window backend wayland selected");
    glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_WAYLAND);
    break;
  default:
    log_error(&app->logger, "Invalid Window backend selected");
    return false;
  }
#endif

  platform_assert_ok(&app->logger);

  if (!glfwInit()) {
    log_error(&app->logger, "Failed to initialize glfw");
    return false;
  }

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

  app->window_backend = info->window_backend;
  app->gpu_backend = info->gpu_backend;

  app->window_handle = glfwCreateWindow(
      info->window_width,
      info->window_height,
      info->window_title.data,
      nullptr,
      nullptr
  );
  app->window_width = info->window_width;
  app->window_height = info->window_height;
  app->window_title = string_clone(info->window_title, allocator);

  if (app->window_handle == nullptr) {
    log_error(&app->logger, "Failed to open a window");
    return false;
  }

  // FIXME(nico): this is only valid because we do not allow to choose the
  // monitor
  GLFWmonitor *primary_monitor = glfwGetPrimaryMonitor();
  if (primary_monitor == nullptr) {
    log_error(&app->logger, "Failed to query the monitor");
    return false;
  }

  i32 primary_monitor_x = 0, primary_monitor_y = 0;
  glfwGetMonitorPos(primary_monitor, &primary_monitor_x, &primary_monitor_y);
  const GLFWvidmode *mode = glfwGetVideoMode(primary_monitor);

  i32 center_x = primary_monitor_x + (mode->width - info->window_width) / 2;
  i32 center_y = primary_monitor_y + (mode->height - info->window_height) / 2;
  glfwSetWindowPos(app->window_handle, center_x, center_y);

  glfwSetMouseButtonCallback(app->window_handle, input_mouse_button_callback);
  glfwSetKeyCallback(app->window_handle, input_key_callback);
  glfwSetScrollCallback(app->window_handle, input_mouse_scroll_callback);
  glfwSetCharCallback(app->window_handle, input_char_pressed_callback);

#if defined(PLATFORM_WEB)
  app->gpu_instance = wgpuCreateInstance(nullptr);
#else
  WGPUInstanceExtras instance_extras = {
    .chain = {.next = nullptr, .sType = (WGPUSType)WGPUSType_InstanceExtras},
    .backends = WGPUInstanceBackend_Primary,
    .flags = WGPUInstanceFlag_Validation | WGPUInstanceFlag_Debug,
  };

  app->gpu_instance = wgpuCreateInstance(&(WGPUInstanceDescriptor){
    .nextInChain = &instance_extras.chain,
  });
#endif

  if (app->gpu_instance == nullptr) {
    log_error(&app->logger, "Failed to create a WGPU instance");
    return false;
  }

#if defined(PLATFORM_WEB)
  // FIXME(nico): Is this normal? Probably not
  WGPUEmscriptenSurfaceSourceCanvasHTMLSelector canvas_desc = {
    .chain =
        {
          .sType = WGPUSType_EmscriptenSurfaceSourceCanvasHTMLSelector,
        },
    .selector = {
      "#canvas"
    }, // NOTE(nico): This is the default canvas id used by the
       // emscripten html shell. Maybe would be better to
       // use a custom selector inside the App_Create_Info
  };

  app->gpu_surface = wgpuInstanceCreateSurface(
      app->gpu_instance,
      &(WGPUSurfaceDescriptor){
        .nextInChain = &canvas_desc.chain,
      }
  );
#elif defined(_WIN32)
  HWND hwnd = glfwGetWin32Window(app->window_handle);
  HINSTANCE hinstance = GetModuleHandle(nullptr);

  WGPUSurfaceSourceWindowsHWND surface_source = {
    .chain =
        (WGPUChainedStruct){
          .sType = WGPUSType_SurfaceSourceWindowsHWND,
        },
    .hinstance = hinstance,
    .hwnd = hwnd,
  };

  app->gpu_surface = wgpuInstanceCreateSurface(
      app->gpu_instance,
      &(WGPUSurfaceDescriptor){
        .nextInChain = &surface_source.chain,
      }
  );
#elif defined(__linux__)
  int platform = glfwGetPlatform();
  if (platform == GLFW_PLATFORM_WAYLAND) {
    WGPUSurfaceSourceWaylandSurface surface_source = {
      .chain =
          (WGPUChainedStruct){.sType = WGPUSType_SurfaceSourceWaylandSurface},
      .display = glfwGetWaylandDisplay(),
      .surface = glfwGetWaylandWindow(app->window_handle),
    };

    app->gpu_surface = wgpuInstanceCreateSurface(
        app->gpu_instance,
        &(WGPUSurfaceDescriptor){
          .nextInChain = &surface_source.chain,
        }
    );
  } else {
    WGPUSurfaceSourceXlibWindow surface_source = {
      .chain = (WGPUChainedStruct){.sType = WGPUSType_SurfaceSourceXlibWindow},
      .display = glfwGetX11Display(),
      .window = (uint64_t)glfwGetX11Window(app->window_handle),
    };

    app->gpu_surface = wgpuInstanceCreateSurface(
        app->gpu_instance,
        &(WGPUSurfaceDescriptor){
          .nextInChain = &surface_source.chain,
        }
    );
  }
#endif

  if (app->gpu_surface == nullptr) {
    log_error(&app->logger, "Failed to create a WGPU surface");
    return false;
  }

  wgpuInstanceRequestAdapter(
      app->gpu_instance,
      &(WGPURequestAdapterOptions){
        .compatibleSurface = app->gpu_surface,
        .powerPreference = WGPUPowerPreference_HighPerformance,
      },
      (WGPURequestAdapterCallbackInfo){
        .mode = callback_mode,
        .callback = gpu_adapter_callback,
        .userdata1 = &app->gpu_adapter,
      }
  );

#if defined(PLATFORM_WEB)
  while (!app->gpu_adapter) {
    log_error(&app->logger, "Waiting for adapter...");
    wgpuInstanceProcessEvents(app->gpu_instance);
    emscripten_sleep(10);
  }
#endif

  // FIXME(nico): probably not valid on the web
  if (app->gpu_adapter == nullptr) {
    log_error(&app->logger, "Failed to query a WGPU adapter");
    return false;
  }

  wgpuAdapterRequestDevice(
      app->gpu_adapter,
      &(WGPUDeviceDescriptor){
        .requiredLimits = nullptr,
        .uncapturedErrorCallbackInfo =
            (WGPUUncapturedErrorCallbackInfo){
              .callback = gpu_error_callback,
              .userdata1 = app,
            },
      },
      (WGPURequestDeviceCallbackInfo){
        .mode = callback_mode,
        .callback = gpu_device_callback,
        .userdata1 = &app->gpu_device,
      }
  );

#if defined(PLATFORM_WEB)
  while (!app->gpu_device) {
    log_error(&app->logger, "Waiting for device...");
    wgpuInstanceProcessEvents(app->gpu_instance);
    emscripten_sleep(10);
  }
#endif

  if (app->gpu_device == nullptr) {
    log_error(&app->logger, "Failed to query a WGPU device");
    return false;
  }

  app->gpu_queue = wgpuDeviceGetQueue(app->gpu_device);
  if (app->gpu_queue == nullptr) {
    log_error(&app->logger, "Failed to create a WGPU queue from device");
    return false;
  }

  glfwPollEvents();

  WGPUSurfaceCapabilities surface_capabilities = {0};
  WGPUStatus surface_capabilities_status = wgpuSurfaceGetCapabilities(
      app->gpu_surface, app->gpu_adapter, &surface_capabilities
  );

  if (surface_capabilities_status != WGPUStatus_Success ||
      surface_capabilities.formatCount == 0) {
    log_error(&app->logger, "Failed to query a Surface capacity");
    return false;
  }

  i32 fb_width = 0, fb_height = 0;
  glfwGetFramebufferSize(app->window_handle, &fb_width, &fb_height);

  app->gpu_surface_format = surface_capabilities.formats[0];
  wgpuSurfaceConfigure(
      app->gpu_surface,
      &(WGPUSurfaceConfiguration){
        .device = app->gpu_device,
        .format = app->gpu_surface_format,
        .usage = WGPUTextureUsage_RenderAttachment, // NOTE(nico): What is this?
        .width = (u32)fb_width,
        .height = (u32)fb_height,
        .presentMode = WGPUPresentMode_Fifo,        // NOTE(nico): What is this?
        .alphaMode = WGPUCompositeAlphaMode_Opaque, // NOTE(nico): What is this?
      }
  );

  app->running = true;

#if !defined(PLATFORM_WEB)
  app->time_frequency = glfwGetTimerFrequency();
#endif

  app->current_time_ns = platform_get_timer_now_ns(app);
  app->last_time = glfwGetTime();

  f64 mx = 0., my = 0.;
  glfwGetCursorPos(app->window_handle, &mx, &my);
  app->mouse_position = (Vec2){.x = (f32)mx, .y = (f32)my};

  _app = app;

  return true;
}

void close_app(App *app) {
  delete_string(app->window_title, app->allocator);

  wgpuInstanceRelease(app->gpu_instance);
  wgpuSurfaceRelease(app->gpu_surface);
  wgpuAdapterRelease(app->gpu_adapter);
  wgpuDeviceRelease(app->gpu_device);
  wgpuQueueRelease(app->gpu_queue);

  glfwDestroyWindow(app->window_handle);
  glfwTerminate();
}

bool32 app_update(App *app) {
  app->running &= !glfwWindowShouldClose(app->window_handle);

  f64 mx = 0., my = 0.;
  glfwGetCursorPos(app->window_handle, &mx, &my);

  app->previous_mouse_position = app->mouse_position;
  app->mouse_position = (Vec2){.x = (f32)mx, .y = (f32)my};
  app->mouse_scoll = 0.f;
  app->char_buffer_len = 0;

  for (usize i = 0; i < APP_MOUSE_BUTTON_CAP; i += 1) {
    app->mouse[i].previous = app->mouse[i].current;
  }

  for (usize i = 0; i < APP_KEYBOARD_KEY_CAP; i += 1) {
    app->keys[i].previous = app->keys[i].current;
    app->keys[i].presses = 0;
  }

  glfwPollEvents();
  app->last_time_ns = app->current_time_ns;
  app->current_time_ns = platform_get_timer_now_ns(app);

  f64 current_time = glfwGetTime();
  app->elapsed_time = current_time - app->last_time;
  app->last_time = current_time;

  app->frame_allocator.free_all(app->frame_allocator);

  return app->running;
}

void app_begin_frame(App *app) {
  // TODO(nico): Probably good to do error handling for this

  WGPUSurfaceTexture surface_texture;
  wgpuSurfaceGetCurrentTexture(app->gpu_surface, &surface_texture);
  app->gpu_default_surface_texture = surface_texture.texture;
  app->gpu_default_frame_texture_view =
      wgpuTextureCreateView(app->gpu_default_surface_texture, nullptr);
  app->gpu_current_command_encoder =
      wgpuDeviceCreateCommandEncoder(app->gpu_device, nullptr);
}

void app_end_frame(App *app) {

  WGPUCommandBuffer cmd_buf =
      wgpuCommandEncoderFinish(app->gpu_current_command_encoder, nullptr);
  wgpuQueueSubmit(app->gpu_queue, 1, &cmd_buf);
  wgpuCommandBufferRelease(cmd_buf);
  wgpuCommandEncoderRelease(app->gpu_current_command_encoder);

#if !defined(PLATFORM_WEB)
  wgpuSurfacePresent(app->gpu_surface);
#endif

  wgpuTextureViewRelease(app->gpu_default_frame_texture_view);
  wgpuTextureRelease(app->gpu_default_surface_texture);
}

u64 app_get_current_time_ns() {
  return _app->current_time_ns;
}

u64 app_get_last_time_ns() {
  return _app->last_time_ns;
}

f32 app_get_elapsed_time() {
  return (f32)_app->elapsed_time;
}

Vec2 app_mouse_position() {
  return _app->mouse_position;
}

Vec2 app_mouse_delta() {
  return (Vec2){
    .x = _app->previous_mouse_position.x - _app->mouse_position.x,
    .y = _app->previous_mouse_position.y - _app->mouse_position.y,
  };
}

f32 app_mouse_scroll() {
  return _app->mouse_scoll;
}

void app_capture_mouse(bool32 on) {
  glfwSetInputMode(
      _app->window_handle,
      GLFW_CURSOR,
      on ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL
  );
}

bool8 app_mouse_pressed(Mouse_Button button) {
  return _app->mouse[button].current;
}

bool8 app_mouse_just_pressed(Mouse_Button button) {
  return _app->mouse[button].current && !_app->mouse[button].previous;
}

bool8 app_key_pressed(Keyboard_Key key) {
  return _app->keys[key].current;
}

u32 app_key_press_count(Keyboard_Key key) {
  return (u32)_app->keys[key].presses;
}

bool8 app_key_just_pressed(Keyboard_Key key) {
  return _app->keys[key].current && !_app->keys[key].previous;
}

Text_Array app_chars_pressed() {
  return (Text_Array){
    .items = _app->char_buffer,
    .len = _app->char_buffer_len,
  };
}

/////////////////////////////////////
// All the various callbacks
/////////////////////////////////////
static void gpu_adapter_callback(
    WGPURequestAdapterStatus status,
    WGPUAdapter adapter,
    WGPUStringView message,
    WGPU_NULLABLE void *userdata1,
    WGPU_NULLABLE void *userdata2
) {
  (void)userdata2;
  if (status != WGPURequestAdapterStatus_Success) {
    printf("[GPU Error] Failed to request Adapter: %s\n", message.data);
    return;
  }

  WGPUAdapter *adapter_prt = (WGPUAdapter *)userdata1;
  *adapter_prt = adapter;
}

static void gpu_device_callback(
    WGPURequestDeviceStatus status,
    WGPUDevice device,
    WGPUStringView message,
    WGPU_NULLABLE void *userdata1,
    WGPU_NULLABLE void *userdata2
) {
  (void)userdata2;
  if (status != WGPURequestDeviceStatus_Success) {
    printf("[GPU Error] Failed to request Device: %s\n", message.data);
    return;
  }

  WGPUDevice *device_ptr = (WGPUDevice *)userdata1;
  *device_ptr = device;
}

static void gpu_error_callback(
    WGPUDevice const *device,
    WGPUErrorType type,
    WGPUStringView message,
    WGPU_NULLABLE void *userdata1,
    WGPU_NULLABLE void *userdata2
) {
  (void)device;
  (void)userdata1;
  (void)userdata2;

  // FIXME(nico): use a user-provided logger. Printf is fine for now
  // Hopefully it is null terminated
  // App *app = (App *)userdata1;
  printf("[GPU Error] %d: %s\n", type, message.data);
}

static void input_key_callback(
    GLFWwindow *window, i32 key, i32 scancode, i32 action, i32 mods
) {
  (void)window;
  (void)scancode;
  (void)mods;
  if (key >= 0 && key < APP_KEYBOARD_KEY_CAP) {
    _app->keys[key].current = action == GLFW_PRESS || action == GLFW_REPEAT;
    if (_app->keys[key].current) {
      _app->keys[key].presses =
          (u8)min_u32((u32)_app->keys[key].presses + 1, 255);
    }
  }
}

static void input_mouse_button_callback(
    GLFWwindow *window, i32 button, i32 action, i32 mods
) {
  (void)window;
  (void)mods;
  if (button >= 0 && button < APP_MOUSE_BUTTON_CAP) {
    _app->mouse[button].current = action == GLFW_PRESS;
  }
}

static void
input_mouse_scroll_callback(GLFWwindow *window, f64 xoffset, f64 yoffset) {
  (void)window;
  (void)xoffset;
  _app->mouse_scoll = (f32)yoffset;
}

static void
input_char_pressed_callback(GLFWwindow *window, utf8_char codepoint) {
  (void)window;
  if (_app->char_buffer_len >= APP_CHAR_BUFFER_CAP) {
    log_debug(&_app->logger, "Char buffer capacity reached");
    return;
  }

  _app->char_buffer[_app->char_buffer_len++] = codepoint;
}

/////////////////////////////
// GPU Buffer management
/////////////////////////////
static usize gpu_buffer_required_alignment(GPU_Buffer_Usage usage) {
  WGPULimits limits = {0};
  WGPUStatus status = wgpuDeviceGetLimits(_app->gpu_device, &limits);
  assert(status == WGPUStatus_Success);

  if (usage & WGPUBufferUsage_Uniform) {
    return (usize)limits.minUniformBufferOffsetAlignment;
  }

  if (usage & WGPUBufferUsage_Storage) {
    return (usize)limits.minStorageBufferOffsetAlignment;
  }

  return 4;
}

GPU_Buffer make_gpu_buffer(GPU_Buffer_Create_Info *info) {
  GPU_Buffer buffer = {0};

  assert(_app != nullptr);
  assert(_app->gpu_device != nullptr && _app->gpu_queue != nullptr);

  buffer.handle = wgpuDeviceCreateBuffer(
      _app->gpu_device,
      &(WGPUBufferDescriptor){
        .usage = info->usage,
        .size = (u64)info->size,
        .mappedAtCreation = false,
      }
  );
  buffer.usage = info->usage;
  buffer.cap = info->size;
  buffer.align = gpu_buffer_required_alignment(info->usage);

  assert(buffer.handle != nullptr);

  return buffer;
}

void destroy_gpu_buffer(GPU_Buffer buffer) {
  assert(buffer.handle != nullptr);
  wgpuBufferRelease(buffer.handle);
}

bool32 gpu_buffer_is_valid(GPU_Buffer buffer) {
  return buffer.handle != nullptr;
}

bool32 gpu_buffer_memory_is_valid(GPU_Buffer_Memory memory) {
  return gpu_buffer_is_valid(*memory.buffer) && memory.size > 0;
}

void gpu_buffer_reset(GPU_Buffer *buffer) {
  buffer->used = 0;
}

GPU_Buffer_Memory gpu_buffer_alloc(GPU_Buffer *buffer, usize size) {
  if (buffer == nullptr || buffer->handle == nullptr || size == 0) {
    return (GPU_Buffer_Memory){0};
  }

  usize offset = buffer->used;
  if (buffer->align > 0) {
    usize remainder = offset % buffer->align;

    if (remainder > 0) {
      offset += buffer->align - remainder;
    }
  }

  if (offset + size > buffer->cap) {
    return (GPU_Buffer_Memory){0};
  }

  GPU_Buffer_Memory memory = {
    .buffer = buffer,
    .offset = offset,
    .size = size,
  };

  buffer->used = offset + size;

  return memory;
}

GPU_Buffer_Memory
gpu_buffer_append(GPU_Buffer *buffer, void *data, usize size) {
  GPU_Buffer_Memory memory = gpu_buffer_alloc(buffer, size);

  if (memory.size > 0) {
    gpu_buffer_write(memory, data, size);
  }

  return memory;
}

void gpu_buffer_write(GPU_Buffer_Memory memory, void *data, usize size) {
  assert(_app != nullptr && _app->gpu_queue != nullptr);
  assert(memory.buffer != nullptr && memory.buffer->handle != nullptr);
  assert(size <= memory.size);

  wgpuQueueWriteBuffer(
      _app->gpu_queue, memory.buffer->handle, (u64)memory.offset, data, size
  );
}

/////////////////////////////
// GPU Texture management
/////////////////////////////
GPU_Texture make_gpu_texture(GPU_Texture_Create_Info *info) {
  if (_app == nullptr || _app->gpu_device == nullptr ||
      _app->gpu_queue == nullptr) {
    return (GPU_Texture){0};
  }

  byte *data = nullptr;
  i32 channels = 0;

  GPU_Texture texture = {0};
  switch (info->kind) {
  case GPU_Texture_Create_Info_Empty:
    texture.width = info->empty.width;
    texture.height = info->empty.height;
    texture.space = info->space;
    channels = (i32)info->empty.channels;
    break;
  case GPU_Texture_Create_Info_File: {
    // FIXME(nico): check if the file path is null terminated
    i32 width, height;
    data = stbi_load(info->file_path.data, &width, &height, &channels, 4);

    if (data == nullptr) {
      return (GPU_Texture){0};
    }

    texture.width = (u32)width;
    texture.height = (u32)height;
    texture.space = info->space;
  } break;
  case GPU_Texture_Create_Info_Memory:
    assert(false);
    break;
  case GPU_Texture_Create_Info_Raw_Memory: {
    if (info->raw.channels != 4) {
      return (GPU_Texture){0};
    }

    data = info->raw.data;
    channels = (i32)info->raw.channels;
    texture.width = info->raw.width;
    texture.height = info->raw.height;
    texture.space = info->space;
  } break;
  }

  switch (channels) {
  case 1:
    texture.format = GPU_Texture_Format_R8Unorm;
    break;
  case 2:
    texture.format = GPU_Texture_Format_RG8Unorm;
    break;
  case 3:
    channels = 4;
    texture.format = info->space == GPU_Texture_Space_sRGB
                         ? GPU_Texture_Format_RGBA8UnormSrgb
                         : GPU_Texture_Format_RGBA8Unorm;
    break;
  case 4:
    texture.format = info->space == GPU_Texture_Space_sRGB
                         ? GPU_Texture_Format_RGBA8UnormSrgb
                         : GPU_Texture_Format_RGBA8Unorm;
    break;
  default:
    return (GPU_Texture){0};
  }

  texture.channels = (u32)channels;
  texture.handle = wgpuDeviceCreateTexture(
      _app->gpu_device,
      &(WGPUTextureDescriptor){
        .size =
            (WGPUExtent3D){
              .width = (u32)texture.width,
              .height = (u32)texture.height,
              .depthOrArrayLayers =
                  1, // NOTE(nico): what is this? For 3d textures?
            },
        .mipLevelCount = 1, // NOTE(nico): This is very specific for 2d renders
        .sampleCount = 1,
        .dimension = WGPUTextureDimension_2D,
        .format = (WGPUTextureFormat)texture.format,
        .usage = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst,
      }
  );

  if (texture.handle == nullptr) {
    return (GPU_Texture){0};
  }

  if (data != nullptr) {
    wgpuQueueWriteTexture(
        _app->gpu_queue,
        &(WGPUTexelCopyTextureInfo){
          .texture = texture.handle,
          .mipLevel = 0,
          .aspect = WGPUTextureAspect_All,
        },
        data,
        (usize)(texture.width * texture.height * (u32)channels),
        &(WGPUTexelCopyBufferLayout){
          .bytesPerRow = (u32)(texture.width * (u32)channels),
          .rowsPerImage = (u32)texture.height,
        },
        &(WGPUExtent3D){
          .width = (u32)texture.width,
          .height = (u32)texture.height,
          .depthOrArrayLayers = 1,
        }
    );
  }

  // Only the file path owns its pixels (loaded by stbi); raw memory data
  // stays owned by the caller
  if (info->kind == GPU_Texture_Create_Info_File) {
    stbi_image_free(data);
  }

  return texture;
}

GPU_Texture make_gpu_depth_texture(u32 width, u32 height) {
  if (_app == nullptr || _app->gpu_device == nullptr) {
    return (GPU_Texture){0};
  }

  GPU_Texture texture = {
    .format = GPU_Texture_Format_Depth24Plus,
    .width = width,
    .height = height,
  };

  texture.handle = wgpuDeviceCreateTexture(
      _app->gpu_device,
      &(WGPUTextureDescriptor){
        .size =
            (WGPUExtent3D){
              .width = width, .height = height, .depthOrArrayLayers = 1
            },
        .mipLevelCount = 1,
        .sampleCount = 1,
        .dimension = WGPUTextureDimension_2D,
        .format = WGPUTextureFormat_Depth24Plus,
        .usage =
            WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_TextureBinding,
      }
  );

  return texture;
}

void destroy_gpu_texture(GPU_Texture texture) {
  assert(gpu_texture_is_valid(texture));
  wgpuTextureRelease(texture.handle);
}

bool32 gpu_texture_is_valid(GPU_Texture texture) {
  return texture.handle != nullptr;
}

bool32 gpu_texture_write(GPU_Texture texture, GPU_Texture_Write_Info *info) {
  if (_app == nullptr || _app->gpu_queue == nullptr) {
    return false;
  }

  if (info->width == 0 || info->height == 0) {
    return false;
  }

  // FIXME(nico): can overflow but don't have safe math for this type and I'm
  // lazy af
  u32 offx = (u32)info->offset.x;
  u32 offy = (u32)info->offset.y;
  if (offx + info->width > texture.width ||
      offy + info->height > texture.height) {
    return false;
  }

  u32 src_bytes_per_row = texture.channels * info->width;

  wgpuQueueWriteTexture(
      _app->gpu_queue,
      &(WGPUTexelCopyTextureInfo){
        .texture = texture.handle,
        .origin = {.x = offx, .y = offy},
        .mipLevel = 0,
        .aspect = WGPUTextureAspect_All,
      },
      info->data,
      src_bytes_per_row * info->height,
      &(WGPUTexelCopyBufferLayout){
        .offset = 0,
        .bytesPerRow = src_bytes_per_row,
        .rowsPerImage = info->height
      },
      &(WGPUExtent3D){
        .width = info->width,
        .height = info->height,
        .depthOrArrayLayers = 1,
      }
  );

  return true;
}

WGPUTextureView gpu_texture_derive_view(GPU_Texture texture) {
  return wgpuTextureCreateView(texture.handle, nullptr);
}

GPU_Sampler make_gpu_sampler(GPU_Sampler_Create_Info *info) {
  if (_app == nullptr || _app->gpu_device == nullptr) {
    return (GPU_Sampler){0};
  }

  GPU_Sampler sampler = {
    .handle = wgpuDeviceCreateSampler(
        _app->gpu_device,
        &(WGPUSamplerDescriptor){
          .addressModeU = (WGPUAddressMode)info->wrap,
          .addressModeV = (WGPUAddressMode)info->wrap,
          .addressModeW = (WGPUAddressMode)info->wrap,
          .magFilter = (WGPUFilterMode)info->filter,
          .minFilter = (WGPUFilterMode)info->filter,
          .mipmapFilter = WGPUMipmapFilterMode_Linear,
          .maxAnisotropy = 1,
        }
    ),
    .filter = info->filter,
    .wrap = info->wrap,
  };

  if (sampler.handle == nullptr) {
    return (GPU_Sampler){0};
  }

  return sampler;
}

void destroy_gpu_sampler(GPU_Sampler sampler) {
  wgpuSamplerRelease(sampler.handle);
}

bool32 gpu_sampler_is_value(GPU_Sampler sampler) {
  return sampler.handle != nullptr;
}

/////////////////////////////
// GPU Pipeline management
/////////////////////////////
GPU_Pipeline
make_gpu_pipeline(GPU_Pipeline_Create_Info *info, Allocator allocator) {
  GPU_Pipeline pipeline = {0};
  pipeline.allocator = allocator;

  if (_app == nullptr || _app->gpu_device == nullptr) {
    return (GPU_Pipeline){0};
  }

  const char *shader_src = nullptr;

  switch (info->shader_source.kind) {
  case GPU_Shader_Source_File: {
    FILE *f = fopen(info->shader_source.file_path.data, "rbe");
    assert(f != nullptr);
    fseek(f, 0, SEEK_END);
    usize sz = (usize)ftell(f);
    rewind(f);

    byte *file_buf =
        (byte *)_app->frame_allocator.alloc(_app->frame_allocator, sz + 1)
            .allocation;

    fread(file_buf, 1, sz, f);
    file_buf[sz] = '\0';
    fclose(f);
    shader_src = (const char *)file_buf;
  } break;
  case GPU_Shader_Source_Raw:
    shader_src = info->shader_source.data.data;
    break;
  }

  if (shader_src == nullptr) {
    return (GPU_Pipeline){0};
  }

  WGPUShaderSourceWGSL wgsl_source = {
    .chain = {.sType = WGPUSType_ShaderSourceWGSL},
    .code = {.data = shader_src, .length = SIZE_MAX},
  };
  WGPUShaderModule shader_module = wgpuDeviceCreateShaderModule(
      _app->gpu_device,
      &(WGPUShaderModuleDescriptor){.nextInChain = &wgsl_source.chain}
  );

  if (shader_module == nullptr) {
    return (GPU_Pipeline){0};
  }

  pipeline.bind_group_infos =
      make_array(pipeline.bind_group_infos, info->bind_groups.len, allocator);

  WGPUBindGroupLayout *layouts =
      (WGPUBindGroupLayout *)_app->frame_allocator
          .alloc(
              _app->frame_allocator,
              sizeof(WGPUBindGroupLayout) * info->bind_groups.len
          )
          .allocation;

  for (usize i = 0; i < info->bind_groups.len; i++) {
    GPU_Bind_Group_Create_Info bgi = array_get(info->bind_groups, i);
    usize si_len = bgi.shader_data_infos.len;

    WGPUBindGroupLayoutEntry *entries =
        (WGPUBindGroupLayoutEntry *)_app->frame_allocator
            .alloc(
                _app->frame_allocator, sizeof(WGPUBindGroupLayoutEntry) * si_len
            )
            .allocation;

    pipeline.bind_group_infos.items[i].shader_data_infos = make_array(
        pipeline.bind_group_infos.items[i].shader_data_infos, si_len, allocator
    );

    for (usize j = 0; j < si_len; j++) {
      GPU_Shader_Data_Info sdi = array_get(bgi.shader_data_infos, j);
      entries[j] = (WGPUBindGroupLayoutEntry){
        .binding = sdi.binding_index,
        .visibility = WGPUShaderStage_Vertex | WGPUShaderStage_Fragment |
                      WGPUShaderStage_Compute,
      };

      switch (sdi.kind) {
      case GPU_Shader_Data_Kind_Uniform:
        entries[j].buffer =
            (WGPUBufferBindingLayout){.type = WGPUBufferBindingType_Uniform};
        break;
      case GPU_Shader_Data_Kind_Storage:
        entries[j].buffer = (WGPUBufferBindingLayout){
          .type = WGPUBufferBindingType_ReadOnlyStorage
        };
        break;
      case GPU_Shader_Data_Kind_Texture:
        entries[j].texture = (WGPUTextureBindingLayout){
          .sampleType = WGPUTextureSampleType_Float,
          .viewDimension = WGPUTextureViewDimension_2D,
        };
        break;
      case GPU_Shader_Data_Kind_Sampler:
        entries[j].sampler = (WGPUSamplerBindingLayout){
          .type = WGPUSamplerBindingType_Filtering
        };
        break;
      }

      array_set(pipeline.bind_group_infos.items[i].shader_data_infos, j, sdi);
    }

    pipeline.bind_group_infos.items[i].group_index = (u32)i;
    pipeline.bind_group_infos.items[i].handle = wgpuDeviceCreateBindGroupLayout(
        _app->gpu_device,
        &(WGPUBindGroupLayoutDescriptor){
          .entryCount = si_len,
          .entries = entries,
        }
    );
    layouts[i] = pipeline.bind_group_infos.items[i].handle;
  }

  pipeline.layout_handle = wgpuDeviceCreatePipelineLayout(
      _app->gpu_device,
      &(WGPUPipelineLayoutDescriptor){
        .bindGroupLayoutCount = info->bind_groups.len,
        .bindGroupLayouts = layouts,
      }
  );

  bool32 use_surface = info->color_targets.len == 0;
  usize target_count = use_surface ? 1 : info->color_targets.len;

  Array(WGPUColorTargetState) color_targets;
  color_targets =
      make_array(color_targets, target_count, _app->frame_allocator);

  if (use_surface) {
    array_set(
        color_targets,
        0,
        ((WGPUColorTargetState){
          .format = (WGPUTextureFormat)_app->gpu_surface_format,
          .writeMask = WGPUColorWriteMask_All,
          .blend = (WGPUBlendState *)info->blend_state,
        })
    );
  } else {
    for (usize i = 0; i < info->color_targets.len; i++) {
      WGPUTextureFormat format =
          (WGPUTextureFormat)array_get(info->color_targets, i);

      array_set(
          color_targets,
          i,
          ((WGPUColorTargetState){
            .format = format,
            .writeMask = WGPUColorWriteMask_All,
            .blend = (WGPUBlendState *)info->blend_state,
          })
      );
    }
  }

  WGPUDepthStencilState depth_stencil_state = {0};
  if (info->depth_test) {
    depth_stencil_state = (WGPUDepthStencilState){
      .format = WGPUTextureFormat_Depth24Plus,
      .depthWriteEnabled = true,
      .depthCompare = WGPUCompareFunction_Less,
    };
  }

  pipeline.handle = wgpuDeviceCreateRenderPipeline(
      _app->gpu_device,
      &(WGPURenderPipelineDescriptor){
        .layout = pipeline.layout_handle,
        .vertex =
            (WGPUVertexState){
              .module = shader_module,
              .entryPoint = {.data = "vs_main", .length = SIZE_MAX},
              .bufferCount = 1,
              .buffers =
                  &(WGPUVertexBufferLayout){
                    .arrayStride = (u64)info->vertex_stride,
                    .stepMode = WGPUVertexStepMode_Vertex,
                    .attributeCount = info->vertex_attribute_count,
                    .attributes =
                        (const WGPUVertexAttribute *)info->vertex_attributes,
                  },
            },
        .fragment =
            &(WGPUFragmentState){
              .module = shader_module,
              .entryPoint = {.data = "fs_main", .length = SIZE_MAX},
              .targetCount = color_targets.len,
              .targets = color_targets.items,
            },
        .primitive =
            (WGPUPrimitiveState){
              .topology = info->primitive.some
                              ? (WGPUPrimitiveTopology)info->primitive.value
                              : WGPUPrimitiveTopology_TriangleList
            },
        .depthStencil = info->depth_test ? &depth_stencil_state : nullptr,
        .multisample = (WGPUMultisampleState){.count = 1, .mask = 0xFFFFFFFF},
      }
  );

  wgpuShaderModuleRelease(shader_module);

  return pipeline;
}

void destroy_gpu_pipeline(GPU_Pipeline pipeline) {
  for (usize i = 0; i < pipeline.bind_group_infos.len; i++) {
    GPU_Bind_Group_Info info = pipeline.bind_group_infos.items[i];

    wgpuBindGroupLayoutRelease(info.handle);
    delete_array(info.shader_data_infos);
  }

  delete_array(pipeline.bind_group_infos);
  wgpuPipelineLayoutRelease(pipeline.layout_handle);
  wgpuRenderPipelineRelease(pipeline.handle);
}

bool32 gpu_pipeline_is_valid(GPU_Pipeline pipeline) {
  return pipeline.handle != nullptr;
}

GPU_Bind_Group gpu_pipeline_derive_bind_group(
    GPU_Pipeline pipeline,
    GPU_Shader_Data_Source_Array sources,
    u32 group_index,
    Allocator allocator
) {
  assert(_app != nullptr && _app->gpu_device != nullptr);
  assert(group_index < pipeline.bind_group_infos.len);

  GPU_Bind_Group_Info bgi = array_get(pipeline.bind_group_infos, group_index);
  usize entry_count = bgi.shader_data_infos.len;
  assert(sources.len == entry_count);

  GPU_Bind_Group bind_group;
  bind_group.shader_datas =
      make_array(bind_group.shader_datas, entry_count, allocator);
  bind_group.group_index = bgi.group_index;

  Array(WGPUBindGroupEntry) entries;
  entries = make_array(entries, entry_count, _app->frame_allocator);

  for (usize j = 0; j < entry_count; j += 1) {
    GPU_Shader_Data_Info sdi = array_get(bgi.shader_data_infos, j);
    GPU_Shader_Data_Source source = array_get(sources, j);

    WGPUBindGroupEntry entry = {
      .binding = sdi.binding_index, .size = WGPU_WHOLE_SIZE
    };

    GPU_Shader_Data data = {
      .kind = sdi.kind,
      .binding_index = sdi.binding_index,
    };

    switch (sdi.kind) {
    case GPU_Shader_Data_Kind_Uniform:
    case GPU_Shader_Data_Kind_Storage:
      assert(source.buffer != nullptr && gpu_buffer_is_valid(*source.buffer));

      if (source.variant == GPU_Shader_Data_Source_Memory) {
        data.memory = source.memory;
        entry.buffer = source.memory.buffer->handle;
      } else if (source.variant == GPU_Shader_Data_Source_Buffer) {
        data.memory = gpu_buffer_alloc(source.buffer, sdi.associated_size);
        assert(gpu_buffer_memory_is_valid(data.memory));

        entry.buffer = source.buffer->handle;
      } else {
        assert(false);
      }

      entry.offset = data.memory.offset;
      entry.size = data.memory.size;
      break;
    case GPU_Shader_Data_Kind_Texture:
      assert(source.variant == GPU_Shader_Data_Source_Texture);
      assert(gpu_texture_is_valid(source.texture));
      data.texture_view = gpu_texture_derive_view(source.texture);
      entry.textureView = data.texture_view;
      break;
    case GPU_Shader_Data_Kind_Sampler:
      assert(source.variant == GPU_Shader_Data_Source_Sampler);
      data.sampler = source.sampler;
      entry.sampler = source.sampler.handle;
      break;
    }

    array_set(bind_group.shader_datas, j, data);
    array_set(entries, j, entry);
  }

  bind_group.handle = wgpuDeviceCreateBindGroup(
      _app->gpu_device,
      &(WGPUBindGroupDescriptor){
        .layout = bgi.handle,
        .entryCount = entries.len,
        .entries = entries.items,
      }
  );

  return bind_group;
}

GPU_Bind_Group_Array gpu_pipeline_derive_bind_group_array(
    GPU_Pipeline pipeline,
    GPU_Shader_Data_Source_Array_2D sources,
    Allocator allocator
) {
  assert(_app != nullptr && _app->gpu_device != nullptr);
  assert(sources.len == pipeline.bind_group_infos.len);

  GPU_Bind_Group_Array result;
  result = make_array(result, sources.len, allocator);

  for (usize i = 0; i < sources.len; i += 1) {
    array_set(
        result,
        i,
        gpu_pipeline_derive_bind_group(
            pipeline, array_get(sources, i), (u32)i, allocator
        )
    );
  }

  return result;
}

void destroy_gpu_bind_group(GPU_Bind_Group bind_group) {
  delete_array(bind_group.shader_datas);
}

void destroy_gpu_bind_groups(GPU_Bind_Group_Array bind_groups) {
  for (usize i = 0; i < bind_groups.len; i += 1) {
    destroy_gpu_bind_group(bind_groups.items[i]);
  }
  delete_array(bind_groups);
}

////////////////////////////////////
// GPU Render target management
////////////////////////////////////
GPU_Render_Target make_gpu_render_target(GPU_Render_Target_Create_Info *info) {
  GPU_Render_Target target = {0};

  if (_app == nullptr || _app->gpu_device == nullptr) {
    return (GPU_Render_Target){0};
  }

  assert(info->color_formats.len > RENDER_TARGET_COLOR_ATTACHMENT_CAP);

  target.color_attachment_len = info->color_formats.len;
  target.width = info->width;
  target.height = info->height;

  for (usize i = 0; i < info->color_formats.len; i += 1) {
    GPU_Texture_Format format = array_get(info->color_formats, i);

    WGPUTexture texture = wgpuDeviceCreateTexture(
        _app->gpu_device,
        &(WGPUTextureDescriptor){
          .size =
              (WGPUExtent3D){
                .width = info->width,
                .height = info->height,
                .depthOrArrayLayers = 1, // FIXME(nico)
              },
          .mipLevelCount = 1, // FIXME(nico)
          .sampleCount = 1,   // FIXME(nico)
          .dimension = WGPUTextureDimension_2D,
          .format = (WGPUTextureFormat)format,
          .usage = WGPUTextureUsage_TextureBinding |
                   WGPUTextureUsage_RenderAttachment,
        }
    );

    assert(texture != nullptr);

    WGPUTextureView view = wgpuTextureCreateView(texture, nullptr);
    assert(view != nullptr);

    target.color_textures[i] = texture;
    target.color_views[i] = view;
    target.color_formats[i] = format;
  }

  if (info->depth) {
    target.depth_texture = wgpuDeviceCreateTexture(
        _app->gpu_device,
        &(WGPUTextureDescriptor){
          .size =
              (WGPUExtent3D){
                .width = info->width,
                .height = info->height,
                .depthOrArrayLayers = 1,
              },
          .mipLevelCount = 1,
          .sampleCount = 1,
          .dimension = WGPUTextureDimension_2D,
          .format = WGPUTextureFormat_Depth24Plus, // FIXME(nico): hardcoded,
                                                   // fine for now
          .usage = WGPUTextureUsage_TextureBinding |
                   WGPUTextureUsage_RenderAttachment,
        }
    );

    assert(target.depth_texture != nullptr);

    target.depth_view = wgpuTextureCreateView(target.depth_texture, nullptr);
    assert(target.depth_view);
  }

  return target;
}

void destroy_gpu_render_target(GPU_Render_Target target) {
  for (usize i = 0; i < target.color_attachment_len; i += 1) {
    wgpuTextureViewRelease(target.color_views[i]);
    wgpuTextureRelease(target.color_textures[i]);
  }

  if (target.depth_view != nullptr) {
    wgpuTextureViewRelease(target.depth_view);
  }

  if (target.depth_texture != nullptr) {
    wgpuTextureRelease(target.depth_texture);
  }
}

bool32 gpu_render_target_is_valid(GPU_Render_Target target) {
  for (usize i = 0; i < target.color_attachment_len; i += 1) {
    if (target.color_textures[i] == nullptr ||
        target.color_views[i] == nullptr) {
      return false;
    }
  }

  // FIXME(nico): handle depth texture error

  return true;
}

GPU_Texture
gpu_render_target_color_texture(GPU_Render_Target *target, u32 index) {
  GPU_Texture texture = {0};

  if (index >= target->color_attachment_len) {
    return (GPU_Texture){0};
  }

  texture.handle = target->color_textures[index];
  texture.format = target->color_formats[index];
  texture.width = target->width;
  texture.height = target->height;

  return texture;
}

////////////////////////////////////
// GPU Render pass management
////////////////////////////////////
// NOTE(nico): for now we aggressively assert here. Could be better to handle it
// more gracefully
GPU_Render_Pass gpu_render_pass_begin(GPU_Render_Pass_Create_Info *info) {
  GPU_Render_Pass pass = {0};

  assert(_app != nullptr && _app->gpu_current_command_encoder != nullptr);

  if (info->target.some) {
    GPU_Render_Target target = info->target.value;

    WGPURenderPassColorAttachment
        color_attachments[RENDER_TARGET_COLOR_ATTACHMENT_CAP] = {{0}};

    for (usize i = 0; i < target.color_attachment_len; i += 1) {
      Color clear_color = info->clear_colors[i];

      color_attachments[i] = (WGPURenderPassColorAttachment){
        .view = target.color_views[i],
        .depthSlice = WGPU_DEPTH_SLICE_UNDEFINED, // NOTE(nico): wtf is this?
        .loadOp = info->clear ? WGPULoadOp_Clear : WGPULoadOp_Load,
        .storeOp = WGPUStoreOp_Store,
        .clearValue = {
          .r = (f64)clear_color.r,
          .g = (f64)clear_color.g,
          .b = (f64)clear_color.b,
          .a = (f64)clear_color.a,
        }
      };
    }

    WGPURenderPassDepthStencilAttachment *depth_attachment = nullptr;
    if (target.depth_texture != nullptr) {
      depth_attachment = &(WGPURenderPassDepthStencilAttachment){
        .view = target.depth_view,
        .depthLoadOp = info->clear ? WGPULoadOp_Clear : WGPULoadOp_Load,
        .depthStoreOp = WGPUStoreOp_Store,
        .depthClearValue = 1.0,
      };
    }

    pass.handle = wgpuCommandEncoderBeginRenderPass(
        _app->gpu_current_command_encoder,
        &(WGPURenderPassDescriptor){
          .colorAttachmentCount = target.color_attachment_len,
          .colorAttachments = color_attachments,
          .depthStencilAttachment = depth_attachment,
        }
    );
  } else {
    Color clear_color = info->clear_colors[0];
    if (color_is_zero(clear_color)) {
      clear_color.a = 1.f;
    }

    WGPURenderPassDepthStencilAttachment *depth_attachment = nullptr;
    if (info->depth.some && gpu_texture_is_valid(info->depth.value)) {
      pass._depth_view =
          wgpuTextureCreateView(info->depth.value.handle, nullptr);
      depth_attachment = &(WGPURenderPassDepthStencilAttachment){
        .view = pass._depth_view,
        .depthLoadOp = info->clear ? WGPULoadOp_Clear : WGPULoadOp_Load,
        .depthStoreOp = WGPUStoreOp_Store,
        .depthClearValue = 1.0f,
      };
    }

    pass.handle = wgpuCommandEncoderBeginRenderPass(
        _app->gpu_current_command_encoder,
        &(WGPURenderPassDescriptor){
          .colorAttachmentCount = 1,
          .colorAttachments =
              &(WGPURenderPassColorAttachment){
                .view = _app->gpu_default_frame_texture_view,
                .depthSlice = WGPU_DEPTH_SLICE_UNDEFINED,
                .loadOp = info->clear ? WGPULoadOp_Clear : WGPULoadOp_Load,
                .storeOp = WGPUStoreOp_Store,
                .clearValue =
                    {
                      .r = (f64)clear_color.r,
                      .g = (f64)clear_color.g,
                      .b = (f64)clear_color.b,
                      .a = (f64)clear_color.a,
                    },
              },
          .depthStencilAttachment = depth_attachment,
        }
    );
  }

  assert(pass.handle != nullptr);

  return pass;
}

void gpu_render_pass_end(GPU_Render_Pass pass) {
  assert(_app != nullptr && _app->gpu_queue != nullptr);
  assert(pass.handle != nullptr);

  wgpuRenderPassEncoderEnd(pass.handle);
  wgpuRenderPassEncoderRelease(pass.handle);

  if (pass._depth_view != nullptr) {
    wgpuTextureViewRelease(pass._depth_view);
  }
}

void gpu_render_pass_bind_pipeline(
    GPU_Render_Pass pass, GPU_Pipeline pipeline
) {
  wgpuRenderPassEncoderSetPipeline(pass.handle, pipeline.handle);
}

void gpu_render_pass_bind_group(GPU_Render_Pass pass, GPU_Bind_Group group) {
  wgpuRenderPassEncoderSetBindGroup(
      pass.handle, group.group_index, group.handle, 0, nullptr
  );
}

void gpu_render_pass_bind_groups(
    GPU_Render_Pass pass, GPU_Bind_Group_Array groups
) {
  for (usize i = 0; i < groups.len; i += 1) {
    gpu_render_pass_bind_group(pass, array_get(groups, i));
  }
}

void gpu_render_pass_draw_indexed(
    GPU_Render_Pass pass,
    GPU_Buffer_Memory vertices,
    GPU_Buffer_Memory indices,
    usize draw_count,
    usize instance_index
) {
  // NOTE(nico): What is the slot used for?
  wgpuRenderPassEncoderSetVertexBuffer(
      pass.handle,
      0,
      vertices.buffer->handle,
      (u64)vertices.offset,
      (u64)vertices.size
  );

  wgpuRenderPassEncoderSetIndexBuffer(
      pass.handle,
      indices.buffer->handle,
      WGPUIndexFormat_Uint32,
      (u64)indices.offset,
      (u64)indices.size
  );

  wgpuRenderPassEncoderDrawIndexed(
      pass.handle, (u32)draw_count, 1, 0, 0, (u32)instance_index
  );
}

void gpu_render_pass_draw(
    GPU_Render_Pass pass, GPU_Buffer_Memory vertices, usize vertex_count
) {
  wgpuRenderPassEncoderSetVertexBuffer(
      pass.handle,
      0,
      vertices.buffer->handle,
      (u64)vertices.offset,
      (u64)vertices.size
  );
  wgpuRenderPassEncoderDraw(pass.handle, (u32)vertex_count, 1, 0, 0);
}
