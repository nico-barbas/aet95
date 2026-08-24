#ifndef CORE_TYPES_H
#define CORE_TYPES_H

#include <assert.h>

typedef char bool8;
typedef unsigned int bool32;

typedef void *rawptr;
typedef __UINTPTR_TYPE__ uintptr;
static_assert(
    sizeof(uintptr) == sizeof(void *), "uintptr must be pointer-sized"
);

typedef unsigned char byte;
typedef unsigned char u8;
typedef unsigned short int u16;
typedef unsigned int u32;
typedef unsigned long long int u64;

typedef short int i16;
typedef int i32;
typedef long long int i64;
typedef float f32;
typedef double f64;

typedef u32 utf8_char;

typedef __SIZE_TYPE__ usize;
static_assert(sizeof(usize) == sizeof(void *), "usize must be pointer-sized");

#define countof(array) (sizeof(array) / sizeof((array)[0]))
#define concat_impl_(a, b) a##b
#define concat_(a, b) concat_impl_(a, b)

static inline u32 bitmask_from_values_u32(u32 *values, usize len) {
  u32 mask = 0;

  for (usize i = 0; i < len; i += 1) {
    assert(values[i] < 32);
    mask |= 1u << values[i];
  }

  return mask;
}

#define bitmask(...)                                                           \
  bitmask_from_values_u32(                                                     \
      (u32[]){__VA_ARGS__}, sizeof((u32[]){__VA_ARGS__}) / sizeof(unsigned)    \
  )

#define Option(type)                                                           \
  struct {                                                                     \
    bool32 some;                                                               \
    type value;                                                                \
  }

#define some(T, val) ((T){.some = true, .value = (val)})
#define none(T) ((T){.some = false})

#define Result(T, E)                                                           \
  struct {                                                                     \
    bool32 ok;                                                                 \
    union {                                                                    \
      T value;                                                                 \
      E error;                                                                 \
    };                                                                         \
  }
#define ok(T, val) ((T){.ok = true, .value = (val)})
#define err(T, err) ((T){.ok = false, .error = (err)})

#define try(T, expr) try_impl_(T, (expr), concat_(try_value_, __LINE__))
#define try_impl_(T, expr, res)                                                \
  __extension__({                                                              \
    typeof(expr)(res) = (expr);                                                \
    if (!(res).ok) {                                                           \
      return err(T, (res).error);                                              \
    }                                                                          \
    (res).value;                                                               \
  })

#define or_return(expr, return_expr)                                           \
  or_return_impl_(expr, return_expr, concat_(or_return_value_, __LINE__))
#define or_return_impl_(expr, return_expr, res)                                \
  __extension__({                                                              \
    typeof(expr)(res) = (expr);                                                \
    if (!(res).ok) {                                                           \
      return (return_expr);                                                    \
    }                                                                          \
    (res).value;                                                               \
  })

// NOTE(nico): failure is a bug, not a condition to propagate
#define unwrap(expr) unwrap_impl_((expr), concat_(unwrap_value_, __LINE__))
#define unwrap_impl_(expr, res)                                                \
  __extension__({                                                              \
    typeof(expr)(res) = (expr);                                                \
    assert((res).ok);                                                          \
    (res).value;                                                               \
  })

#endif