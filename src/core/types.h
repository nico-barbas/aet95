#ifndef CORE_TYPES_H
#define CORE_TYPES_H

#include <assert.h>

typedef unsigned _BitInt(1) bool8;
typedef _BitInt(32) bool32;
static_assert(sizeof(bool8) == 1, "bool8 must be 1 byte");
static_assert(sizeof(bool32) == 4, "bool32 must be 4 byte");

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
static_assert(sizeof(byte) == 1, "byte must be 1 byte");
static_assert(sizeof(u64) == 8, "byte must be 1 byte");

typedef short int i16;
typedef int i32;
typedef long long int i64;

typedef float f32;
static_assert(sizeof(float) == sizeof(u32));
static_assert(__FLT_RADIX__ == 2);
static_assert(__FLT_MANT_DIG__ == 24);
static_assert(__FLT_MAX_EXP__ == 128);
static_assert(__DBL_MANT_DIG__ == 53);

#define INF_F32 __builtin_inff()

#ifdef __FAST_MATH__
#error ("ffast-math not supported")
#endif

typedef double f64;

typedef unsigned _BitInt(32) utf8_char;
static_assert(sizeof(utf8_char) == 4, "utf8_char must be 1 byte");

typedef __SIZE_TYPE__ usize;
static_assert(sizeof(usize) == sizeof(void *), "usize must be pointer-sized");

typedef enum Type_Kind : u16 {
  Type_Kind_String,
  Type_Kind_Cstring,
  Type_Kind_Char,
  Type_Kind_Bool8,
  Type_Kind_Bool32,
  Type_Kind_Byte,
  Type_Kind_U8,
  Type_Kind_U16,
  Type_Kind_U32,
  Type_Kind_U64,
  Type_Kind_Usize,
  Type_Kind_I16,
  Type_Kind_I32,
  Type_Kind_I64,
  Type_Kind_F32,
  Type_Kind_F64,
  Type_Kind_Struct,
} Type_Kind;

typedef struct Type_Info {
  Type_Kind kind;
  const char *name;
} Type_Info;

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

typedef struct Gen_Handle {
  u32 id;
  u32 generation;
} Gen_Handle;
typedef Option(Gen_Handle) Gen_Handle_Option;

static inline u64 gen_handle_pack(Gen_Handle handle) {
  return (u64)handle.generation << 32 | (u64)handle.id;
}

#define gen_handle_eq(h1, h2)                                                  \
  ((h1).generation == (h2).generation && (h1).id == (h2).id)

#endif