#ifndef CORE_CAMERA_H
#define CORE_CAMERA_H

#include "./math.h"
#include "./physics.h"
#include "./types.h"

typedef struct Raw_Camera {
  Vec3 position;
  Vec3 target;
  Vec3 up;
  f32 fovy;
  f32 z_near;
  f32 z_far;
  Mat4 mat_proj;
  Mat4 mat_view;
  Mat4 mat_proj_view;
  f32 frame_width;
  f32 frame_height;
} Raw_Camera;

typedef struct Raw_Camera_State {
  Raw_Camera current;
  Raw_Camera previous;
} Raw_Camera_State;

void update_raw_camera(Raw_Camera *camera, f32 frame_w, f32 frame_h);
Raw_Camera lerp_raw_cameras(Raw_Camera *a, Raw_Camera *b, f32 t);

// Unprojects a point in window pixels (origin top-left, y down) into a world
// space ray spanning the near to far plane. Uses the camera's matrices, so it
// reads the state from the last update_raw_camera call.
Ray ray_from_screen(Raw_Camera *camera, Vec2 screen_pos);

typedef struct Debug_Camera {
  Raw_Camera base;
  f32 pitch;
  f32 yaw;
  f32 distance;
  f32 pan_speed;
  f32 orbit_speed;
  f32 scroll_speed;
  f32 mouse_sensitivity;
  f32 min_pitch;
  f32 max_pitch;
  f32 min_distance;
} Debug_Camera;

Debug_Camera debug_camera(void);
void update_debug_camera(
    Debug_Camera *camera, f32 frame_w, f32 frame_h, f32 dt
);

typedef struct First_Person_Camera {
  Raw_Camera base;
  Vec3 forward;
  f32 yaw;
  f32 pitch;
  f32 roll;

  // Camera tweaking, for procedural animations
  f32 mouse_sensitivity;
  f32 yaw_offset;
  f32 pitch_offset;
} First_Person_Camera;

First_Person_Camera first_person_camera(void);
void update_first_person_camera(
    First_Person_Camera *camera, f32 frame_w, f32 frame_h, f32 dt
);

typedef struct Orbit_Camera {
  Raw_Camera base;

  // States
  f32 pitch;
  f32 yaw;
  f32 distance;
  f32 next_pitch;
  f32 next_yaw;
  f32 next_distance;
  Vec3 next_target;
  f32 scroll_velocity;
  Vec3 orbit_pivot;

  // Config
  f32 pan_speed;
  f32 orbit_speed;
  f32 scroll_speed;
  f32 scroll_decay;
  f32 mouse_sensitivity;
  f32 min_pitch;
  f32 max_pitch;
  f32 min_distance;
  f32 max_distance;
} Orbit_Camera;

Orbit_Camera orbit_camera(void);
void update_orbit_camera(
    Orbit_Camera *camera, f32 frame_w, f32 frame_h, f32 dt
);

typedef struct Chase_Camera {
  Raw_Camera base;

  Vec3 target_position;
  Quat target_rotation;
  f32 lookahead;
  f32 distance;
  f32 height;
} Chase_Camera;

Chase_Camera chase_camera(
    Vec3 target_position,
    Quat target_rotation,
    f32 initial_distance,
    f32 lookahead
);
void update_chase_camera(
    Chase_Camera *camera,
    Vec3 target_position,
    Quat target_rotation,
    f32 frame_w,
    f32 frame_h,
    f32 dt
);

#endif