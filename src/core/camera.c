#include "core/camera.h"

// FIXME(nico): make the cameras take input as arguments instead of relying on
// the platform layer
#include "core/platform.h"

#include <math.h>

static Vec3 rotate_around_pivot_y(Vec3 point, Vec3 pivot, f32 angle_deg) {
  f32 angle = to_radians_f32(angle_deg);
  f32 _cos = cosf(angle);
  f32 _sin = sinf(angle);

  f32 ox = point.x - pivot.x;
  f32 oz = point.z - pivot.z;

  f32 rx = ox * _cos - oz * _sin;
  f32 rz = ox * _sin + oz * _cos;

  return vec3(pivot.x + rx, point.y, pivot.z + rz);
}

Ray ray_from_screen(Raw_Camera *camera, Vec2 screen_pos) {
  f32 ndc_x = (2.f * screen_pos.x / camera->frame_width) - 1.f;
  f32 ndc_y = 1.f - (2.f * screen_pos.y / camera->frame_height);

  Mat4 inv_proj_view = mat4_inverse(camera->mat_proj_view);

  // Clip Z in [0, 1] (WebGPU convention): 0 = near plane, 1 = far plane
  Vec4 near_h = mat4_mul_vec4(inv_proj_view, vec4(ndc_x, ndc_y, 0.f, 1.f));
  Vec4 far_h = mat4_mul_vec4(inv_proj_view, vec4(ndc_x, ndc_y, 1.f, 1.f));

  Vec3 near_p = vec3_scale(vec3(near_h.x, near_h.y, near_h.z), 1.f / near_h.w);
  Vec3 far_p = vec3_scale(vec3(far_h.x, far_h.y, far_h.z), 1.f / far_h.w);

  Vec3 span = vec3_sub(far_p, near_p);
  f32 len = vec3_length(span);

  return (Ray){
    .origin = near_p,
    .dir = vec3_scale(span, 1.f / len),
    .length = len,
  };
}

void update_raw_camera(Raw_Camera *camera, f32 frame_w, f32 frame_h) {
  camera->frame_width = frame_w;
  camera->frame_height = frame_h;

  camera->mat_proj = mat4_persp(
      to_radians_f32(camera->fovy),
      frame_w / frame_h,
      camera->z_near,
      camera->z_far
  );
  camera->mat_view = mat4_look_at(camera->position, camera->target, camera->up);
  camera->mat_proj_view = mat4_mul(camera->mat_proj, camera->mat_view);
}

Raw_Camera lerp_raw_cameras(Raw_Camera *a, Raw_Camera *b, f32 t) {
  return (Raw_Camera){
    .position = vec3_lerp(a->position, b->position, t),
    .target = vec3_lerp(a->target, b->target, t),
    .up = vec3_normalize(vec3_lerp(a->up, b->up, t)),
    .fovy = lerp_f32(a->fovy, b->fovy, t),
    .z_near = b->z_near,
    .z_far = b->z_far,
  };
}

static void recompute_debug_camera(Debug_Camera *camera) {
  f32 pitch_in_rad = to_radians_f32(camera->pitch);
  f32 yaw_in_rad = to_radians_f32(camera->yaw);
  f32 h_dist = camera->distance * cosf(pitch_in_rad);
  f32 v_dist = camera->distance * sinf(pitch_in_rad);

  camera->base.position = vec3(
      camera->base.target.x - (h_dist * cosf(yaw_in_rad)),
      camera->base.target.y + v_dist,
      camera->base.target.z - (h_dist * sinf(yaw_in_rad))
  );

  Vec3 f = vec3_normalize(vec3_sub(camera->base.target, camera->base.position));
  Vec3 r = vec3_normalize(vec3_cross(f, VEC3_UP));
  camera->base.up = vec3_normalize(vec3_cross(r, f));
}

Debug_Camera debug_camera(void) {
  Debug_Camera camera = {
    .base =
        {
          .target = {.raw = {0}},
          .fovy = 45.f,
          .z_near = 0.1f,
          .z_far = 1000.f,
        },
    .pitch = 45.f,
    .yaw = 90.f,
    .distance = 10.f,
    .pan_speed = 12.5f,
    .orbit_speed = 90.f,
    .scroll_speed = 12.5f,
    .mouse_sensitivity = 0.5f,
    .min_pitch = -80.f,
    .max_pitch = 80.f,
    .min_distance = 2.f,
  };

  recompute_debug_camera(&camera);
  return camera;
}

void update_debug_camera(
    Debug_Camera *camera, f32 frame_w, f32 frame_h, f32 dt
) {
  Vec3 displacement = {.raw = {0}};

  bool8 m_right = app_mouse_pressed(Mouse_Button_Right);

  if (m_right) {
    Vec2 m_delta = app_mouse_delta();
    camera->yaw += m_delta.x * camera->mouse_sensitivity;
    camera->pitch -= m_delta.y * camera->mouse_sensitivity;
  } else if (app_key_pressed(Keyboard_Key_Q)) {
    camera->yaw += camera->orbit_speed * dt;
  } else if (app_key_pressed(Keyboard_Key_E)) {
    camera->yaw -= camera->orbit_speed * dt;
  }

  if (app_key_pressed(Keyboard_Key_W)) {
    displacement.z = 1.f;
  } else if (app_key_pressed(Keyboard_Key_S)) {
    displacement.z = -1.f;
  }

  if (app_key_pressed(Keyboard_Key_A)) {
    displacement.x = -1.f;
  } else if (app_key_pressed(Keyboard_Key_D)) {
    displacement.x = 1.f;
  }

  if (!vec3_is_zero(displacement)) {
    Vec3 forward = vec3_sub(camera->base.target, camera->base.position);
    forward = vec3_normalize(vec3(forward.x, 0.f, forward.z));
    Vec3 right = vec3_normalize(vec3_cross(forward, VEC3_UP));

    f32 s = dt * camera->pan_speed;
    displacement = vec3_normalize(displacement);

    camera->base.target = vec3_add(
        camera->base.target,
        vec3_add(
            vec3_scale(forward, displacement.z * s),
            vec3_scale(right, displacement.x * s)
        )
    );
  }

  // TODO(nico): implement mouse scroll before this is available

  camera->pitch =
      clamp_f32(camera->pitch, camera->min_pitch, camera->max_pitch);

  recompute_debug_camera(camera);
  update_raw_camera(&camera->base, frame_w, frame_h);
}

First_Person_Camera first_person_camera(void) {
  First_Person_Camera camera = {
    .base =
        {
          .fovy = 45.f,
          .z_near = 0.1f,
          .z_far = 100.f,
          .position = vec3(-4.f, 1.f, -4.f),
          .target = vec3(0.f, 0.f, 0.f),
          .up = VEC3_UP,
        },
    .mouse_sensitivity = 0.1f,
  };

  Vec3 dir = vec3_normalize(vec3_sub(camera.base.target, camera.base.position));
  f32 yaw_in_rad = atan2f(dir.z, dir.x);
  f32 pitch_in_rad = asinf(dir.y);

  camera.yaw = to_degrees_f32(yaw_in_rad);
  camera.pitch = to_degrees_f32(pitch_in_rad);
  camera.forward = vec3_normalize(vec3(
      cosf(yaw_in_rad) * cosf(pitch_in_rad),
      sinf(pitch_in_rad),
      sinf(yaw_in_rad) * cosf(pitch_in_rad)
  ));
  camera.base.target = vec3_add(camera.base.position, camera.forward);

  return camera;
}

void update_first_person_camera(
    First_Person_Camera *camera, f32 frame_w, f32 frame_h, f32 dt
) {
  (void)dt;

  const f32 min_pitch = -80.f;
  const f32 max_pitch = 80.f;

  Vec2 m_delta = app_mouse_delta();

  camera->yaw -= m_delta.x * camera->mouse_sensitivity;
  camera->pitch += m_delta.y * camera->mouse_sensitivity;
  camera->pitch = clamp_f32(camera->pitch, min_pitch, max_pitch);

  f32 yaw_in_rad = to_radians_f32(camera->yaw + camera->yaw_offset);
  f32 pitch_in_rad = to_radians_f32(camera->pitch + camera->pitch_offset);

  camera->forward = vec3_normalize(vec3(
      cosf(yaw_in_rad) * cosf(pitch_in_rad),
      sinf(pitch_in_rad),
      sinf(yaw_in_rad) * cosf(pitch_in_rad)
  ));
  camera->base.target = vec3_add(camera->base.position, camera->forward);
  update_raw_camera(&camera->base, frame_w, frame_h);
}

//////////////////////////////
// Orbit Camera
//////////////////////////////

static void recompute_orbit_camera(Orbit_Camera *camera) {
  f32 pitch_in_rad = to_radians_f32(camera->pitch);
  f32 yaw_in_rad = to_radians_f32(camera->yaw);
  f32 h_dist = camera->distance * cosf(pitch_in_rad);
  f32 v_dist = camera->distance * sinf(pitch_in_rad);

  camera->base.position = vec3(
      camera->base.target.x - (h_dist * cosf(yaw_in_rad)),
      camera->base.target.y + v_dist,
      camera->base.target.z - (h_dist * sinf(yaw_in_rad))
  );

  Vec3 f = vec3_normalize(vec3_sub(camera->base.target, camera->base.position));
  Vec3 r = vec3_normalize(vec3_cross(f, VEC3_UP));
  camera->base.up = vec3_normalize(vec3_cross(r, f));
}

Orbit_Camera orbit_camera(void) {
  Orbit_Camera camera = {
    .base =
        {
          .target = {.raw = {0}},
          .fovy = 45.f,
          .z_near = 0.1f,
          .z_far = 1000.f,
        },
    .pitch = 45.f,
    .yaw = 90.f,
    .distance = 10.f,
    .next_pitch = 45.f,
    .next_yaw = 90.f,
    .next_distance = 10.f,

    .pan_speed = 12.5f,
    .orbit_speed = 90.f,
    .scroll_speed = 1.5f,
    .scroll_decay = 10.f,
    .mouse_sensitivity = 0.5f,
    .min_pitch = 10.f,
    .max_pitch = 80.f,
    .min_distance = 2.f,
    .max_distance = 50.f,
  };

  recompute_orbit_camera(&camera);
  return camera;
}

void update_orbit_camera(
    Orbit_Camera *camera, f32 frame_w, f32 frame_h, f32 dt
) {
  Vec3 displacement = {.raw = {0}};

  bool8 m_right = app_mouse_pressed(Mouse_Button_Right);
  // bool8 m_middle = app_mouse_pressed(Mouse_Button_Middle);
  f32 m_scroll = app_mouse_scroll();
  Ray m_ray = ray_from_screen(&camera->base, app_mouse_position());
  Ray_Collision_Result m_ray_gound_hit =
      ray_intersect_plane(&m_ray, (Vec3){0}, VEC3_UP);

  // NOTE(nico): There is no guard for the collision failing
  // The only case would be if the camera is pointing to the sky
  // but the camera limits should prevent it from ever happening
  if (app_mouse_just_pressed(Mouse_Button_Right) ||
      app_key_just_pressed(Keyboard_Key_Q) ||
      app_key_just_pressed(Keyboard_Key_E)) {
    camera->orbit_pivot = m_ray_gound_hit.in;
  }

  if (m_right) {
    Vec2 m_delta = app_mouse_delta();
    f32 dyaw = m_delta.x * camera->mouse_sensitivity;

    camera->next_yaw += dyaw;
    camera->next_pitch -= m_delta.y * camera->mouse_sensitivity;

    camera->next_target =
        rotate_around_pivot_y(camera->next_target, camera->orbit_pivot, dyaw);
  } else if (app_key_pressed(Keyboard_Key_Q)) {
    f32 dyaw = camera->orbit_speed * dt;

    camera->next_yaw += dyaw;
    camera->next_target =
        rotate_around_pivot_y(camera->next_target, camera->orbit_pivot, dyaw);
  } else if (app_key_pressed(Keyboard_Key_E)) {
    f32 dyaw = -camera->orbit_speed * dt;

    camera->next_yaw += dyaw;
    camera->next_target =
        rotate_around_pivot_y(camera->next_target, camera->orbit_pivot, dyaw);
  }

  if (m_scroll != 0.f) {
    camera->scroll_velocity -= m_scroll * camera->scroll_speed;
  }

  if (app_key_pressed(Keyboard_Key_W)) {
    displacement.z = 1.f;
  } else if (app_key_pressed(Keyboard_Key_S)) {
    displacement.z = -1.f;
  }

  if (app_key_pressed(Keyboard_Key_A)) {
    displacement.x = -1.f;
  } else if (app_key_pressed(Keyboard_Key_D)) {
    displacement.x = 1.f;
  }

  if (!vec3_is_zero(displacement)) {
    Vec3 forward = vec3_sub(camera->base.target, camera->base.position);
    forward = vec3_normalize(vec3(forward.x, 0.f, forward.z));
    Vec3 right = vec3_normalize(vec3_cross(forward, VEC3_UP));

    f32 s = dt * camera->pan_speed;
    displacement = vec3_normalize(displacement);

    camera->next_target = vec3_add(
        camera->next_target,
        vec3_add(
            vec3_scale(forward, displacement.z * s),
            vec3_scale(right, displacement.x * s)
        )
    );
  }

  // Integration
  camera->next_distance *= expf(camera->scroll_velocity * dt);
  camera->scroll_velocity *= expf(-camera->scroll_decay * dt);
  if (fabsf(camera->scroll_velocity) < 0.0001f) {
    camera->scroll_velocity = 0.f;
  }

  // State validity control
  camera->next_pitch =
      clamp_f32(camera->next_pitch, camera->min_pitch, camera->max_pitch);

  f32 clamped_distance = clamp_f32(
      camera->next_distance, camera->min_distance, camera->max_distance
  );
  if (clamped_distance != camera->next_distance) {
    camera->next_distance = clamped_distance;
    camera->scroll_velocity = 0.f;
  }

  // Smoothing between current and target state values
  f32 pan_t = 1.f - expf(-15.f * dt);
  f32 zoom_t = 1.f - expf(-10.f * dt);

  camera->pitch = lerp_f32(camera->pitch, camera->next_pitch, pan_t);
  camera->yaw = lerp_f32(camera->yaw, camera->next_yaw, pan_t);
  camera->base.target =
      vec3_lerp(camera->base.target, camera->next_target, pan_t);

  camera->distance = lerp_f32(camera->distance, camera->next_distance, zoom_t);

  recompute_orbit_camera(camera);
  update_raw_camera(&camera->base, frame_w, frame_h);
}

//////////////////////////////
// Chase Camera
//////////////////////////////
static void recompute_chase_camera(Chase_Camera *camera) {
  Vec3 f = vec3_rotate_by_quat(VEC3_FORWARD, camera->target_rotation);
  Vec3 r = vec3_normalize(vec3_cross(f, VEC3_UP));

  camera->base.up = vec3_normalize(vec3_cross(r, f));
  camera->base.target =
      vec3_add(camera->target_position, vec3_scale(f, camera->lookahead));
  camera->base.position = vec3_add(
      vec3_add(
          camera->base.target, vec3_scale(vec3_negate(f), camera->distance)
      ),
      vec3_scale(camera->base.up, camera->height)
  );
}

Chase_Camera chase_camera(
    Vec3 target_position,
    Quat target_rotation,
    f32 initial_distance,
    f32 lookahead
) {
  Chase_Camera camera = {
    .base =
        {
          .fovy = 45.f,
          .z_near = 0.1f,
          .z_far = 1000.f,
        },
    .target_position = target_position,
    .target_rotation = target_rotation,
    .lookahead = lookahead,
    .distance = initial_distance,
    .height = 2.5f,
  };

  recompute_chase_camera(&camera);
  return camera;
}

void update_chase_camera(
    Chase_Camera *camera,
    Vec3 target_position,
    Quat target_rotation,
    f32 frame_w,
    f32 frame_h,
    f32 dt
) {
  (void)dt;
  camera->target_position = target_position;
  camera->target_rotation = target_rotation;

  recompute_chase_camera(camera);
  update_raw_camera(&camera->base, frame_w, frame_h);
}