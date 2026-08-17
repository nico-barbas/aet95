#ifndef CORE_MATH_H
#define CORE_MATH_H

#include "core/types.h"

//////////////////////////////////
// Other math
//////////////////////////////////
i32 sign_extend_i32(u32 value, u32 bits);
u64 hash_fnv1a(void *data, usize size);

//////////////////////////////////
// Generic math
//////////////////////////////////
f32 to_radians_f32(f32 degrees);
f32 to_degrees_f32(f32 radians);

f32 clamp_f32(f32 v, f32 min, f32 max);
f32 min_f32(f32 a, f32 b);
f32 max_f32(f32 a, f32 b);
f32 sign_f32(f32 val);
f32 abs_f32(f32 val);
f32 floor_f32(f32 val);
f32 lerp_f32(f32 a, f32 b, f32 t);
// Returns 0 when val is below edge, 1 otherwise.
f32 step_f32(f32 edge, f32 val);
f32 rand_f32(void);

i32 clamp_i32(i32 v, i32 min, i32 max);
i32 min_i32(i32 a, i32 b);
i32 max_i32(i32 a, i32 b);

u64 min_u64(u64 a, u64 b);

//////////////////////////////////
// Linear algebra
//////////////////////////////////
typedef struct Vec2 {
  union {
    f32 raw[2];
    struct {
      f32 x;
      f32 y;
    };
  };
} Vec2;

typedef struct Vec4 {
  union {
    f32 raw[4];
    struct {
      f32 x;
      f32 y;
      f32 z;
      f32 w;
    };
  };
} Vec4;

typedef struct Vec3Int {
  union {
    i32 raw[3];
    struct {
      i32 x;
      i32 y;
      i32 z;
    };
  };
} Vec3Int;

typedef struct Quat {
  union {
    f32 raw[4];
    struct {
      f32 x;
      f32 y;
      f32 z;
      f32 w;
    };
  };
} Quat;

typedef struct Mat4 {
  union {
    f32 raw[16];
    struct {
      // column 0    column 1    column 2    column 3
      f32 m00, m10, m20, m30; // right(x)
      f32 m01, m11, m21, m31; // up(y)
      f32 m02, m12, m22, m32; // forward(z, into screen)
      f32 m03, m13, m23, m33; // translation
    };
  };
} Mat4;

typedef struct Color {
  union {
    f32 raw[4];
    struct {
      f32 r;
      f32 g;
      f32 b;
      f32 a;
    };
  };
} Color;

#define vec2_is_zero(v) ((v).x == 0.f && (v).y == 0.f)

Vec2 vec2_normalize(Vec2 v);

typedef struct Vec3 {
  union {
    f32 raw[3];
    struct {
      f32 x;
      f32 y;
      f32 z;
    };
  };
} Vec3;

#define VEC3_UP ((Vec3){.raw = {0.f, 1.f, 0.f}})
#define VEC3_DOWN ((Vec3){.raw = {0.f, -1.f, 0.f}})
#define VEC3_FORWARD ((Vec3){.raw = {0.f, 0.f, 1.f}})
#define VEC3_RIGHT ((Vec3){.raw = {-1.f, 0.f, 0.f}})
#define vec3_is_zero(v) ((v).x == 0.f && (v).y == 0.f && (v).z == 0.f)

Vec3 vec3(f32 x, f32 y, f32 z);
Vec3 vec3_negate(Vec3 v);
Vec3 vec3_add(Vec3 v1, Vec3 v2);
Vec3 vec3_sub(Vec3 v1, Vec3 v2);
Vec3 vec3_scale(Vec3 v1, f32 s);
Vec3 vec3_hadamard_mul(Vec3 v1, Vec3 v2);
Vec3 vec3_hadamard_div(Vec3 v1, Vec3 v2);

f32 vec3_dot(Vec3 v1, Vec3 v2);
Vec3 vec3_cross(Vec3 v1, Vec3 v2);
Vec3 vec3_normalize(Vec3 v);
f32 vec3_length(Vec3 v);
Vec3 vec3_lerp(Vec3 a, Vec3 b, f32 t);
Vec3 vec3_rotate_by_quat(Vec3 v, Quat q);

#define vec3int_is_zero(v) ((v).x == 0 && (v).y == 0 && (v).z == 0)

Vec3Int vec3int(i32 x, i32 y, i32 z);
Vec3Int vec3int_add(Vec3Int a, Vec3Int b);
Vec3Int vec3int_sub(Vec3Int a, Vec3Int b);
bool32 vec3int_eq(Vec3Int a, Vec3Int b);
void vec3int_swap(Vec3Int *a, Vec3Int *b);

Vec4 vec4(f32 x, f32 y, f32 z, f32 w);

Quat quat(f32 x, f32 y, f32 z, f32 w);
Quat quat_identity(void);
Quat quat_normalize(Quat q);
Quat quat_from_axis_angle(Vec3 axis, f32 angle);
Quat quat_add(Quat a, Quat b);
Quat quat_scale(Quat q, f32 s);
Quat quat_mul(Quat a, Quat b);
Quat quat_conjugate(Quat q);

Mat4 mat4_identity();
Mat4 mat4_mul(Mat4 m1, Mat4 m2);
Mat4 mat4_from_trs(Vec3 translation, Quat rotation, Vec3 scale);
Mat4 mat4_translate(Vec3 v);
Mat4 mat4_scale(Vec3 v);
// See the kind of mat rotation that's the most flexible. Probably should
// implement quaternions

Mat4 mat4_ortho(f32 left, f32 right, f32 bottom, f32 top, f32 near, f32 far);
Mat4 mat4_persp(f32 fovy, f32 aspect, f32 near, f32 far);
Mat4 mat4_look_at(Vec3 eye, Vec3 center, Vec3 up);
Mat4 mat4_inverse(Mat4 m);
Mat4 mat4_transpose(Mat4 m);
Vec4 mat4_mul_vec4(Mat4 m, Vec4 v);

#define color_is_zero(c)                                                       \
  ((c).r == 0.0f && (c).g == 0.0f && (c).b == 0.0f && (c).a == 0.0f)

Color color(f32 r, f32 g, f32 b, f32 a);

#endif
