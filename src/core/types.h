#ifndef CORE_TYPES_H
#define CORE_TYPES_H

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

typedef __SIZE_TYPE__ usize;
static_assert(sizeof(usize) == sizeof(void *), "usize must be pointer-sized");

#define countof(array) (sizeof(array) / sizeof((array)[0]))
#define concat_impl_(a, b) a##b
#define concat_(a, b) concat_impl_(a, b)

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

#define try(T, expr) try_impl_(T, (expr), concat_(try_value_, __COUNTER__))
#define try_impl_(T, expr, res)                                                \
  __extension__({                                                              \
    typeof(expr) res = (expr);                                                 \
    if (!res.ok) {                                                             \
      return err(T, res.error);                                                \
    }                                                                          \
    res.value;                                                                 \
  })

// NOTE(nico): failure is a bug, not a condition to propagate
#define unwrap(expr) unwrap_impl_((expr), concat_(unwrap_value_, __COUNTER__))
#define unwrap_impl_(expr, res)                                                \
  __extension__({                                                              \
    typeof(expr) res = (expr);                                                 \
    assert(res.ok);                                                            \
    res.value;                                                                 \
  })

#endif