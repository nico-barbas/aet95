#ifndef CORE_FMT_H
#define CORE_FMT_H

#include "core/array.h"
#include "core/strings.h"
#include "core/types.h"

typedef struct Fmt_Arg Fmt_Arg;
struct Fmt_Arg {
  Type_Info type_info;
  u32 precision;
  void (*write)(String_Builder *b, Fmt_Arg *args);
  union {
    byte blob[64];
    String str;
    const char *cstr;
    char c;
    bool8 b8;
    bool32 b32;
    i64 i;
    u64 u;
    f32 f32v;
    f64 f64v;
  };
};

typedef Array(Fmt_Arg) Fmt_Arg_Buffer;

#define FMT_ARG_MAKE(fn, T, KIND, field)                                       \
  static inline Fmt_Arg fn(T v) {                                              \
    return (Fmt_Arg){                                                          \
      .type_info = {.kind = (KIND), .name = #T},                               \
      .field = v,                                                              \
    };                                                                         \
  }

FMT_ARG_MAKE(string_fmt, String, Type_Kind_String, str)
FMT_ARG_MAKE(char_fmt, char, Type_Kind_Char, c)
FMT_ARG_MAKE(bool8_fmt, bool8, Type_Kind_Bool8, b8)
FMT_ARG_MAKE(bool32_fmt, bool32, Type_Kind_Bool32, b32)
FMT_ARG_MAKE(u8_fmt, u8, Type_Kind_U8, u)
FMT_ARG_MAKE(u16_fmt, u16, Type_Kind_U16, u)
FMT_ARG_MAKE(u32_fmt, u32, Type_Kind_U32, u)
FMT_ARG_MAKE(u64_fmt, u64, Type_Kind_U64, u)
FMT_ARG_MAKE(usize_fmt, usize, Type_Kind_Usize, u)
FMT_ARG_MAKE(i16_fmt, i16, Type_Kind_I16, i)
FMT_ARG_MAKE(i32_fmt, i32, Type_Kind_I32, i)
FMT_ARG_MAKE(i64_fmt, i64, Type_Kind_I64, i)
FMT_ARG_MAKE(f32_fmt, f32, Type_Kind_F32, f32v)
FMT_ARG_MAKE(f64_fmt, f64, Type_Kind_F64, f64v)

#undef FMT_ARG_MAKE

static inline Fmt_Arg cstring_fmt(const char *v) {
  return (Fmt_Arg){
    .type_info = {.kind = Type_Kind_Cstring, .name = "cstring"},
    .cstr = v,
  };
}

static inline Fmt_Arg fmt_arg_fmt(Fmt_Arg v) {
  return v;
}

struct Fmt_Unsupported_Type;
Fmt_Arg unsupported_fmt(void (*)(struct Fmt_Unsupported_Type));

#define FMT(value)                                                                                                                                                                                                                                                                                                                           \
  _Generic((value), String: string_fmt, char *: cstring_fmt, const char *: cstring_fmt, char: char_fmt, bool8: bool8_fmt, bool32: bool32_fmt, u8: u8_fmt, u16: u16_fmt, u32: u32_fmt, u64: u64_fmt, usize: usize_fmt, i16: i16_fmt, i32: i32_fmt, i64: i64_fmt, f32: f32_fmt, f64: f64_fmt, Fmt_Arg: fmt_arg_fmt, default: unsupported_fmt)( \
      value                                                                                                                                                                                                                                                                                                                                  \
  )

#define PARENS ()

#define EXPAND(...) EXPAND4(EXPAND4(EXPAND4(EXPAND4(__VA_ARGS__))))
#define EXPAND4(...) EXPAND3(EXPAND3(EXPAND3(EXPAND3(__VA_ARGS__))))
#define EXPAND3(...) EXPAND2(EXPAND2(EXPAND2(EXPAND2(__VA_ARGS__))))
#define EXPAND2(...) EXPAND1(EXPAND1(EXPAND1(EXPAND1(__VA_ARGS__))))
#define EXPAND1(...) __VA_ARGS__

#define FOR_EACH(macro, ...)                                                   \
  __VA_OPT__(EXPAND(FOR_EACH_HELPER(macro, __VA_ARGS__)))

#define FOR_EACH_HELPER(macro, a1, ...)                                        \
  macro(a1) __VA_OPT__(, FOR_EACH_AGAIN PARENS(macro, __VA_ARGS__))

#define FOR_EACH_AGAIN() FOR_EACH_HELPER

void fmt_printb_impl(
    String_Builder *b, const char *fmt_str, Fmt_Arg_Buffer args
);

#define fmt_printb(b, fmt_str, ...)                                            \
  fmt_printb_impl(                                                             \
      (b),                                                                     \
      (fmt_str),                                                               \
      ((Fmt_Arg_Buffer){                                                       \
        .items = ((Fmt_Arg[]){FOR_EACH(FMT, __VA_ARGS__)}),                    \
        .len =                                                                 \
            sizeof((Fmt_Arg[]){FOR_EACH(FMT, __VA_ARGS__)}) / sizeof(Fmt_Arg), \
      })                                                                       \
  )

#endif