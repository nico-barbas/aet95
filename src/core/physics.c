#include "core/physics.h"

#include "core/math.h"

bool32 point_in_aabb(AABB_Collider *collider, Vec3 p) {
  return (p.x >= collider->min.x && p.x < collider->max.x) &&
         (p.y >= collider->min.y && p.y < collider->max.y) &&
         (p.z >= collider->min.z && p.z < collider->max.z);
}

Ray_Collision_Result ray_intersect_plane(Ray *ray, Vec3 point, Vec3 normal) {
  const f32 eps = 1e-6f;

  Vec3 n = vec3_normalize(normal);
  Vec3 d = vec3_normalize(ray->dir);

  f32 denom = vec3_dot(d, n);
  if (abs_f32(denom) < eps) {
    return (Ray_Collision_Result){0};
  }

  f32 t = vec3_dot(vec3_sub(point, ray->origin), n) / denom;
  if (t < 0.f || t > ray->length) {
    return (Ray_Collision_Result){0};
  }

  Vec3 hit_point = vec3_add(ray->origin, vec3_scale(d, t));
  return (Ray_Collision_Result){
    .hit = true,
    .in = hit_point,
    .out = hit_point,
    .normal = denom < 0.f ? n : vec3_scale(n, -1.f),
  };
}

Ray_Collision_Result ray_intersect_obb(Ray *ray, OBB_Collider *collider) {
  const f32 eps = 1e-6f;

  Vec3 p = vec3_sub(ray->origin, collider->center);
  Vec3 r = vec3_normalize(ray->dir);

  Vec3 ro = vec3(
      vec3_dot(p, collider->axis[0]),
      vec3_dot(p, collider->axis[1]),
      vec3_dot(p, collider->axis[2])
  );
  Vec3 rd = vec3(
      vec3_dot(r, collider->axis[0]),
      vec3_dot(r, collider->axis[1]),
      vec3_dot(r, collider->axis[2])
  );

  Vec3 bmin = vec3(-collider->half.x, -collider->half.y, -collider->half.z);
  Vec3 bmax = vec3(collider->half.x, collider->half.y, collider->half.z);

  f32 tmin = 0.f;
  f32 tmax = ray->length;

  for (usize i = 0; i < 3; i += 1) {
    f32 _o = ro.raw[i];
    f32 _d = rd.raw[i];
    f32 _min = bmin.raw[i];
    f32 _max = bmax.raw[i];

    if (abs_f32(_d) < eps) {
      if (_o < _min || _o > _max) {
        return (Ray_Collision_Result){0};
      }

      continue;
    }

    f32 t1 = (_min - _o) / _d;
    f32 t2 = (_max - _o) / _d;

    if (t1 > t2) {
      f32 _tmp = t1;
      t1 = t2;
      t2 = _tmp;
    }

    tmin = max_f32(tmin, t1);
    tmax = min_f32(tmax, t2);

    if (tmin > tmax) {
      return (Ray_Collision_Result){0};
    }
  }

  if (tmax < 0.f) {
    return (Ray_Collision_Result){0};
  }

  return (Ray_Collision_Result){
    .hit = true,
    .in = vec3_add(ray->origin, vec3_scale(r, tmin)),
    .out = vec3_add(ray->origin, vec3_scale(r, tmax)),
    // TODO(nico): hit normal
  };
}

Ray_Collision_Result ray_intersect_aabb(Ray *ray, AABB_Collider *collider) {
  const f32 eps = 1e-6f;

  Vec3 r = vec3_normalize(ray->dir);

  f32 t_in = 0.f;
  f32 t_out = ray->length;
  i32 entry_axis = -1;

  for (usize i = 0; i < 3; i += 1) {
    f32 _o = ray->origin.raw[i];
    f32 _d = r.raw[i];
    f32 _min = collider->min.raw[i];
    f32 _max = collider->max.raw[i];

    if (abs_f32(_d) < eps) {
      // Ray parallel to this pair of slabs: only overlaps if the origin already
      // sits between them.
      if (_o < _min || _o > _max) {
        return (Ray_Collision_Result){0};
      }

      continue;
    }

    f32 t1 = (_min - _o) / _d;
    f32 t2 = (_max - _o) / _d;

    if (t1 > t2) {
      f32 _tmp = t1;
      t1 = t2;
      t2 = _tmp;
    }

    // Track which axis produced the latest entry: that slab is the face the
    // ray crosses to get in.
    if (t1 > t_in) {
      t_in = t1;
      entry_axis = (i32)i;
    }
    t_out = min_f32(t_out, t2);

    if (t_in > t_out) {
      return (Ray_Collision_Result){0};
    }
  }

  // Entry face normal points back along the ray on the entry axis. Left zero
  // when the origin starts inside (entry_axis stays -1).
  Vec3 normal = {0};
  if (entry_axis >= 0) {
    normal.raw[entry_axis] = r.raw[entry_axis] > 0.f ? -1.f : 1.f;
  }

  return (Ray_Collision_Result){
    .hit = true,
    .in = vec3_add(ray->origin, vec3_scale(r, t_in)),
    .out = vec3_add(ray->origin, vec3_scale(r, t_out)),
    .t_in = t_in,
    .t_out = t_out,
    .normal = normal,
  };
}