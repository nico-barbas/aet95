#ifndef CORE_PHYSICS_H
#define CORE_PHYSICS_H

#include "core/math.h"

//////////////////////////////////
// Physics
//////////////////////////////////
typedef struct Ray {
  Vec3 origin;
  Vec3 dir;
  f32 length;
} Ray;

typedef struct Ray_Collision_Result {
  bool32 hit;
  Vec3 in;
  Vec3 out;
  f32 t_in;
  f32 t_out;
  Vec3 normal;
} Ray_Collision_Result;

typedef struct OBB_Collider {
  Vec3 center;
  Vec3 axis[3];
  Vec3 half;
} OBB_Collider;

typedef struct AABB_Collider {
  Vec3 min;
  Vec3 max;
} AABB_Collider;

bool32 point_in_aabb(AABB_Collider *collider, Vec3 p);

Ray_Collision_Result ray_intersect_obb(Ray *ray, OBB_Collider *collider);
Ray_Collision_Result ray_intersect_aabb(Ray *ray, AABB_Collider *collider);

// Intersects against the infinite plane defined by any point on it and its
// normal. Hits from both sides; the returned normal always faces the ray.
Ray_Collision_Result ray_intersect_plane(Ray *ray, Vec3 point, Vec3 normal);

#endif