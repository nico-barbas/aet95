#ifndef CORE_PLATEFORM_H
#define CORE_PLATEFORM_H

#include "core/strings.h"

typedef enum App_Window_Backend {
  App_Window_Backend_Auto,
  App_Window_Backend_X11,
  App_Window_Backend_Wayland,
} App_Window_Backend;

typedef enum App_GPU_Backend {
  App_GPU_Backend_Auto,
  App_GPU_Backend_Vulkan,
  App_GPU_Backend_DX12,
} App_GPU_Backend;

typedef struct App_Create_Info {
  String title;
  u32 width;
  u32 height;

  App_Window_Backend window_backend;
  App_GPU_Backend gpu_backend;
} App_Create_Info;

#endif