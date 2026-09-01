#include "core/math.h"

#include "core/types.h"

#include <limits.h>
#include <math.h>
#include <string.h>

//////////////////////////////////
// Other math
//////////////////////////////////
i32 sign_extend_i32(u32 value, u32 bits) {
  u32 shift = 32 - bits;
  return (i32)(value << shift) >> shift;
}

u64 hash_fnv1a(const void *data, usize size) {
  u64 hash = 14695981039346656037ULL; // FNV offset basis
  const byte *buf = (const byte *)data;

  for (usize i = 0; i < size; i += 1) {
    hash ^= buf[i];
    hash *= 1099511628211ULL; // FNV prime
  }
  return hash;
}

//////////////////////////////////
// Generic math
//////////////////////////////////
#define PI 3.14159265358979323846f

f32 to_radians_f32(f32 degrees) {
  return degrees * (PI / 180.0f);
}
f32 to_degrees_f32(f32 radians) {
  return radians * (180.0f / PI);
}

f32 clamp_f32(f32 v, f32 min, f32 max) {
  const f32 t = v < min ? min : v;
  return t > max ? max : t;
}

f32 min_f32(f32 a, f32 b) {
  return a > b ? b : a;
}

f32 max_f32(f32 a, f32 b) {
  return a > b ? a : b;
}

f32 sign_f32(f32 val) {
  if (val == 0.0f) {
    return 0.0f;
  }

  return val > 0.0f ? 1.0f : -1.0f;
}

f32 abs_f32(f32 val) {
  return val > 0.0f ? val : -val;
}

f32 floor_f32(f32 val) {
  i32 int_part = (i32)val;

  if (val >= 0.f || val == (f32)int_part) {
    return (f32)int_part;
  }

  return (f32)(int_part - 1);
}

f32 lerp_f32(f32 a, f32 b, f32 t) {
  return a + t * (b - a);
}

f32 step_f32(f32 edge, f32 val) {
  return val < edge ? 0.0f : 1.0f;
}

i32 clamp_i32(i32 v, i32 min, i32 max) {
  const i32 t = v < min ? min : v;
  return t > max ? max : t;
}

i32 min_i32(i32 a, i32 b) {
  return a > b ? b : a;
}

i32 max_i32(i32 a, i32 b) {
  return a > b ? a : b;
}

f32 rand_f32(void) {
  // return ((f32)rand() / (f32)(RAND_MAX));
  return 0.f;
}

u32 min_u32(u32 a, u32 b) {
  return a > b ? b : a;
}

u32 max_u32(u32 a, u32 b) {
  return a > b ? a : b;
}

u64 min_u64(u64 a, u64 b) {
  return a > b ? b : a;
}

usize max_usize(usize a, usize b) {
  return a > b ? a : b;
}

usize min_usize(usize a, usize b) {
  return a > b ? b : a;
}

usize clamp_usize(usize v, usize min, usize max) {
  usize t = v < min ? min : v;
  return t > max ? max : t;
}

//////////////////////////////////
// Safe math
//////////////////////////////////
Safe_Math_I64_Result safe_add_i64(i64 a, i64 b) {
  if ((b > 0 && a > LLONG_MAX - b) || (b < 0 && a < LLONG_MIN - b)) {
    return err(Safe_Math_I64_Result, Safe_Math_Error_Signed_Overflow);
  }
  return ok(Safe_Math_I64_Result, a + b);
}

Safe_Math_I64_Result safe_mul_i64(i64 a, i64 b) {
  if (a > 0) {
    if (b > 0 && a > LLONG_MAX / b) {
      return err(Safe_Math_I64_Result, Safe_Math_Error_Signed_Overflow);
    }
    if (b < 0 && b < LLONG_MIN / a) {
      return err(Safe_Math_I64_Result, Safe_Math_Error_Signed_Overflow);
    }
  } else if (a < 0) {
    if (b > 0 && a < LLONG_MIN / b) {
      return err(Safe_Math_I64_Result, Safe_Math_Error_Signed_Overflow);
    }
    if (b < 0 && a < LLONG_MAX / b) {
      return err(Safe_Math_I64_Result, Safe_Math_Error_Signed_Overflow);
    }
  }

  return ok(Safe_Math_I64_Result, a * b);
}

Safe_Math_U64_Result safe_add_u64(u64 a, u64 b) {
  if (a > ((u64)-1) - b) {
    return err(Safe_Math_U64_Result, Safe_Math_Error_Unsigned_Overflow);
  }
  return ok(Safe_Math_U64_Result, a + b);
}

Safe_Math_U64_Result safe_mul_u64(u64 a, u64 b) {
  if (a != 0 && b > ((u64)-1) / a) {
    return err(Safe_Math_U64_Result, Safe_Math_Error_Unsigned_Overflow);
  }
  return ok(Safe_Math_U64_Result, a * b);
}

//////////////////////////////////
// Geometry
//////////////////////////////////

bool32 rect_point_in(Rectangle r, f32 x, f32 y) {
  return x > r.x && x < r.x + r.width && y > r.y && y < r.y + r.height;
}

//////////////////////////////////
// Linear algebra
//////////////////////////////////
Vec2 vec2(f32 x, f32 y) {
  return (Vec2){.raw = {x, y}};
}

Vec2 vec2_negate(Vec2 v) {
  return (Vec2){.raw = {-v.x, -v.y}};
}

Vec2 vec2_add(Vec2 v1, Vec2 v2) {
  return (Vec2){.raw = {v1.x + v2.x, v1.y + v2.y}};
}

Vec2 vec2_sub(Vec2 v1, Vec2 v2) {
  return (Vec2){.raw = {v1.x - v2.x, v1.y - v2.y}};
}

Vec2 vec2_normalize(Vec2 v) {
  f32 il = 1.0f / sqrtf(v.x * v.x + v.y * v.y);
  return (Vec2){
    .x = v.x * il,
    .y = v.y * il,
  };
}

Vec2 vec2_rotate_around(Vec2 v, Vec2 center, f32 angle) {
  f32 s = sinf(angle);
  f32 c = cosf(angle);
  f32 dx = v.x - center.x;
  f32 dy = v.y - center.y;
  return (Vec2){
    .raw = {
      center.x + dx * c - dy * s,
      center.y + dx * s + dy * c,
    }
  };
}

Vec2Int vec2int(i32 x, i32 y) {
  return (Vec2Int){.raw = {x, y}};
}

bool32 vec2int_eq(Vec2Int a, Vec2Int b) {
  return a.x == b.x && a.y == b.y;
}

Vec3 vec3(f32 x, f32 y, f32 z) {
  return (Vec3){.raw = {x, y, z}};
}

Vec3 vec3_negate(Vec3 v) {
  return (Vec3){.raw = {-v.x, -v.y, -v.z}};
}

Vec3 vec3_add(Vec3 a, Vec3 b) {
  return (Vec3){.raw = {a.x + b.x, a.y + b.y, a.z + b.z}};
}

Vec3 vec3_sub(Vec3 a, Vec3 b) {
  return (Vec3){.raw = {a.x - b.x, a.y - b.y, a.z - b.z}};
}

Vec3 vec3_scale(Vec3 v, f32 s) {
  return (Vec3){.raw = {v.x * s, v.y * s, v.z * s}};
}

Vec3 vec3_hadamard_mul(Vec3 v1, Vec3 v2) {
  return (Vec3){.raw = {v1.x * v2.x, v1.y * v2.y, v1.z * v2.z}};
}

Vec3 vec3_hadamard_div(Vec3 v1, Vec3 v2) {
  return (Vec3){.raw = {v1.x / v2.x, v1.y / v2.y, v1.z / v2.z}};
}

f32 vec3_dot(Vec3 a, Vec3 b) {
  return a.x * b.x + a.y * b.y + a.z * b.z;
}

Vec3 vec3_cross(Vec3 a, Vec3 b) {
  return (Vec3){
    .raw = {
      a.y * b.z - a.z * b.y,
      a.z * b.x - a.x * b.z,
      a.x * b.y - a.y * b.x,
    }
  };
}

f32 vec3_length(Vec3 v) {
  return sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
}

Vec3 vec3_normalize(Vec3 v) {
  f32 len = vec3_length(v);
  if (len == 0.0f)
    return (Vec3){0};
  return vec3_scale(v, 1.0f / len);
}

Vec3 vec3_lerp(Vec3 a, Vec3 b, f32 t) {
  return (Vec3){
    .raw = {
      lerp_f32(a.x, b.x, t),
      lerp_f32(a.y, b.y, t),
      lerp_f32(a.z, b.z, t),
    }
  };
}

Vec3 vec3_rotate_by_quat(Vec3 v, Quat rotation) {
  Quat q = quat_normalize(rotation);
  Vec3 u = vec3(q.x, q.y, q.z);
  Vec3 uv = vec3_cross(u, v);
  Vec3 uuv = vec3_cross(u, uv);

  return vec3_add(
      v, vec3_add(vec3_scale(uv, 2.0f * q.w), vec3_scale(uuv, 2.0f))
  );
}

Vec3Int vec3int(i32 x, i32 y, i32 z) {
  return (Vec3Int){.raw = {x, y, z}};
}

Vec3Int vec3int_add(Vec3Int a, Vec3Int b) {
  return (Vec3Int){.raw = {a.x + b.x, a.y + b.y, a.z + b.z}};
}

Vec3Int vec3int_sub(Vec3Int a, Vec3Int b) {
  return (Vec3Int){.raw = {a.x - b.x, a.y - b.y, a.z - b.z}};
}

bool32 vec3int_eq(Vec3Int a, Vec3Int b) {
  return a.x == b.x && a.y == b.y && a.z == b.z;
}

void vec3int_swap(Vec3Int *a, Vec3Int *b) {
  Vec3Int tmp = *a;
  *a = *b;
  *b = tmp;
}

Vec4 vec4(f32 x, f32 y, f32 z, f32 w) {
  return (Vec4){.raw = {x, y, z, w}};
}

Quat quat(f32 x, f32 y, f32 z, f32 w) {
  return (Quat){.raw = {x, y, z, w}};
}

Quat quat_identity(void) {
  return (Quat){.raw = {0.0f, 0.0f, 0.0f, 1.0f}};
}

Quat quat_from_axis_angle(Vec3 axis, f32 angle) {
  Vec3 normalized_axis = vec3_normalize(axis);
  if (vec3_is_zero(normalized_axis)) {
    return quat_identity();
  }

  f32 half_angle = angle * 0.5f;
  f32 s = sinf(half_angle);
  return (Quat){
    .raw = {
      normalized_axis.x * s,
      normalized_axis.y * s,
      normalized_axis.z * s,
      cosf(half_angle),
    },
  };
}

Quat quat_add(Quat a, Quat b) {
  return (Quat){.raw = {a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w}};
}

Quat quat_scale(Quat q, f32 s) {
  return (Quat){.raw = {q.x * s, q.y * s, q.z * s, q.w * s}};
}

Quat quat_mul(Quat a, Quat b) {
  return (Quat){
    .raw = {
      a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
      a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
      a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w,
      a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z,
    },
  };
}

Quat quat_conjugate(Quat q) {
  return (Quat){.raw = {-q.x, -q.y, -q.z, q.w}};
}

Quat quat_normalize(Quat q) {
  f32 len = sqrtf(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
  if (len == 0.0f) {
    return quat_identity();
  }

  f32 inv_len = 1.0f / len;
  return (Quat){
    .raw = {q.x * inv_len, q.y * inv_len, q.z * inv_len, q.w * inv_len},
  };
}

// --- Mat4 ---
// Storage: column-major. Naming: m[row][col], so raw index = col*4 + row.
// Columns: col0=right(x), col1=up(y), col2=forward(z), col3=translation.

Mat4 mat4_identity() {
  Mat4 m = {0};
  m.m00 = m.m11 = m.m22 = m.m33 = 1.0f;
  return m;
}

Mat4 mat4_mul(Mat4 a, Mat4 b) {
  Mat4 c = {0};
  for (int col = 0; col < 4; col++) {
    for (int row = 0; row < 4; row++) {
      f32 sum = 0.0f;
      for (int k = 0; k < 4; k++) {
        sum += a.raw[k * 4 + row] * b.raw[col * 4 + k];
      }
      c.raw[col * 4 + row] = sum;
    }
  }
  return c;
}

Mat4 mat4_from_trs(Vec3 translation, Quat rotation, Vec3 scale) {
  Quat q = quat_normalize(rotation);

  f32 xx = q.x * q.x;
  f32 yy = q.y * q.y;
  f32 zz = q.z * q.z;
  f32 xy = q.x * q.y;
  f32 xz = q.x * q.z;
  f32 yz = q.y * q.z;
  f32 xw = q.x * q.w;
  f32 yw = q.y * q.w;
  f32 zw = q.z * q.w;

  Mat4 m = {0};
  m.m00 = (1.0f - 2.0f * yy - 2.0f * zz) * scale.x;
  m.m10 = (2.0f * xy + 2.0f * zw) * scale.x;
  m.m20 = (2.0f * xz - 2.0f * yw) * scale.x;

  m.m01 = (2.0f * xy - 2.0f * zw) * scale.y;
  m.m11 = (1.0f - 2.0f * xx - 2.0f * zz) * scale.y;
  m.m21 = (2.0f * yz + 2.0f * xw) * scale.y;

  m.m02 = (2.0f * xz + 2.0f * yw) * scale.z;
  m.m12 = (2.0f * yz - 2.0f * xw) * scale.z;
  m.m22 = (1.0f - 2.0f * xx - 2.0f * yy) * scale.z;

  m.m03 = translation.x;
  m.m13 = translation.y;
  m.m23 = translation.z;
  m.m33 = 1.0f;
  return m;
}

Mat4 mat4_translate(Vec3 v) {
  Mat4 m = mat4_identity();
  m.m03 = v.x;
  m.m13 = v.y;
  m.m23 = v.z;
  return m;
}

Mat4 mat4_scale(Vec3 v) {
  Mat4 m = {0};
  m.m00 = v.x;
  m.m11 = v.y;
  m.m22 = v.z;
  m.m33 = 1.0f;
  return m;
}

// Orthographic projection. Clip Z in [0, 1] (WebGPU convention).
Mat4 mat4_ortho(f32 left, f32 right, f32 bottom, f32 top, f32 _near, f32 _far) {
  f32 rcp_w = 1.0f / (right - left);
  f32 rcp_h = 1.0f / (top - bottom);
  f32 rcp_d = 1.0f / (_near - _far);

  Mat4 m = {0};
  m.m00 = 2.0f * rcp_w;
  m.m11 = 2.0f * rcp_h;
  m.m22 = rcp_d;
  m.m03 = -(right + left) * rcp_w;
  m.m13 = -(top + bottom) * rcp_h;
  m.m23 = _near * rcp_d;
  m.m33 = 1.0f;
  return m;
}

// Perspective projection, right-handed, clip Z in [0, 1] (WebGPU convention).
Mat4 mat4_persp(f32 fovy, f32 aspect, f32 _near, f32 _far) {
  f32 f = 1.0f / tanf(fovy * 0.5f);
  Mat4 m = {0};
  m.m00 = f / aspect;
  m.m11 = f;
  m.m22 = _far / (_near - _far); // maps view-z to [0,1]
  m.m32 = -1.0f;                 // perspective divide: w_clip = -z_view
  m.m23 = (_near * _far) / (_near - _far); // z offset
  return m;
}

// View matrix, right-handed (camera looks down -Z).
Mat4 mat4_look_at(Vec3 eye, Vec3 center, Vec3 up) {
  Vec3 f = vec3_normalize(vec3_sub(center, eye)); // look direction
  Vec3 s = vec3_normalize(vec3_cross(f, up));     // right
  Vec3 u = vec3_cross(s, f);                      // adjusted up

  Mat4 m = {0};
  m.m00 = s.x;
  m.m01 = s.y;
  m.m02 = s.z;
  m.m10 = u.x;
  m.m11 = u.y;
  m.m12 = u.z;
  m.m20 = -f.x;
  m.m21 = -f.y;
  m.m22 = -f.z;
  m.m03 = -vec3_dot(s, eye);
  m.m13 = -vec3_dot(u, eye);
  m.m23 = vec3_dot(f, eye);
  m.m33 = 1.0f;
  return m;
}

// General 4x4 inverse via cofactor expansion. Returns identity when the
// matrix is singular (never the case for view/projection matrices).
Mat4 mat4_inverse(Mat4 m) {
  f32 *a = m.raw;
  Mat4 out;
  f32 *inv = out.raw;

  inv[0] = a[5] * a[10] * a[15] - a[5] * a[11] * a[14] - a[9] * a[6] * a[15] +
           a[9] * a[7] * a[14] + a[13] * a[6] * a[11] - a[13] * a[7] * a[10];
  inv[4] = -a[4] * a[10] * a[15] + a[4] * a[11] * a[14] + a[8] * a[6] * a[15] -
           a[8] * a[7] * a[14] - a[12] * a[6] * a[11] + a[12] * a[7] * a[10];
  inv[8] = a[4] * a[9] * a[15] - a[4] * a[11] * a[13] - a[8] * a[5] * a[15] +
           a[8] * a[7] * a[13] + a[12] * a[5] * a[11] - a[12] * a[7] * a[9];
  inv[12] = -a[4] * a[9] * a[14] + a[4] * a[10] * a[13] + a[8] * a[5] * a[14] -
            a[8] * a[6] * a[13] - a[12] * a[5] * a[10] + a[12] * a[6] * a[9];
  inv[1] = -a[1] * a[10] * a[15] + a[1] * a[11] * a[14] + a[9] * a[2] * a[15] -
           a[9] * a[3] * a[14] - a[13] * a[2] * a[11] + a[13] * a[3] * a[10];
  inv[5] = a[0] * a[10] * a[15] - a[0] * a[11] * a[14] - a[8] * a[2] * a[15] +
           a[8] * a[3] * a[14] + a[12] * a[2] * a[11] - a[12] * a[3] * a[10];
  inv[9] = -a[0] * a[9] * a[15] + a[0] * a[11] * a[13] + a[8] * a[1] * a[15] -
           a[8] * a[3] * a[13] - a[12] * a[1] * a[11] + a[12] * a[3] * a[9];
  inv[13] = a[0] * a[9] * a[14] - a[0] * a[10] * a[13] - a[8] * a[1] * a[14] +
            a[8] * a[2] * a[13] + a[12] * a[1] * a[10] - a[12] * a[2] * a[9];
  inv[2] = a[1] * a[6] * a[15] - a[1] * a[7] * a[14] - a[5] * a[2] * a[15] +
           a[5] * a[3] * a[14] + a[13] * a[2] * a[7] - a[13] * a[3] * a[6];
  inv[6] = -a[0] * a[6] * a[15] + a[0] * a[7] * a[14] + a[4] * a[2] * a[15] -
           a[4] * a[3] * a[14] - a[12] * a[2] * a[7] + a[12] * a[3] * a[6];
  inv[10] = a[0] * a[5] * a[15] - a[0] * a[7] * a[13] - a[4] * a[1] * a[15] +
            a[4] * a[3] * a[13] + a[12] * a[1] * a[7] - a[12] * a[3] * a[5];
  inv[14] = -a[0] * a[5] * a[14] + a[0] * a[6] * a[13] + a[4] * a[1] * a[14] -
            a[4] * a[2] * a[13] - a[12] * a[1] * a[6] + a[12] * a[2] * a[5];
  inv[3] = -a[1] * a[6] * a[11] + a[1] * a[7] * a[10] + a[5] * a[2] * a[11] -
           a[5] * a[3] * a[10] - a[9] * a[2] * a[7] + a[9] * a[3] * a[6];
  inv[7] = a[0] * a[6] * a[11] - a[0] * a[7] * a[10] - a[4] * a[2] * a[11] +
           a[4] * a[3] * a[10] + a[8] * a[2] * a[7] - a[8] * a[3] * a[6];
  inv[11] = -a[0] * a[5] * a[11] + a[0] * a[7] * a[9] + a[4] * a[1] * a[11] -
            a[4] * a[3] * a[9] - a[8] * a[1] * a[7] + a[8] * a[3] * a[5];
  inv[15] = a[0] * a[5] * a[10] - a[0] * a[6] * a[9] - a[4] * a[1] * a[10] +
            a[4] * a[2] * a[9] + a[8] * a[1] * a[6] - a[8] * a[2] * a[5];

  f32 det = a[0] * inv[0] + a[1] * inv[4] + a[2] * inv[8] + a[3] * inv[12];
  if (det == 0.0f) {
    return mat4_identity();
  }

  f32 inv_det = 1.0f / det;
  for (i32 i = 0; i < 16; i += 1) {
    inv[i] *= inv_det;
  }

  return out;
}

Mat4 mat4_transpose(Mat4 m) {
  Mat4 out;
  for (i32 col = 0; col < 4; col += 1) {
    for (i32 row = 0; row < 4; row += 1) {
      out.raw[col * 4 + row] = m.raw[row * 4 + col];
    }
  }
  return out;
}

Vec4 mat4_mul_vec4(Mat4 m, Vec4 v) {
  Vec4 r = {0};
  for (i32 row = 0; row < 4; row += 1) {
    r.raw[row] = m.raw[0 * 4 + row] * v.x + m.raw[1 * 4 + row] * v.y +
                 m.raw[2 * 4 + row] * v.z + m.raw[3 * 4 + row] * v.w;
  }
  return r;
}

Color color(f32 r, f32 g, f32 b, f32 a) {
  return (Color){.raw = {r, g, b, a}};
}

Color color_add(Color a, Color b) {
  return (Color){.simd = a.simd + b.simd};
}

Color color_scale(Color c, f32 s) {
  return (Color){.simd = c.simd / s};
}

Color color_lerp(Color a, Color b, f32 t) {
  return (Color){.simd = a.simd + (b.simd - a.simd) * t};
}
