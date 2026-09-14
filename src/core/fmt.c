#include "core/fmt.h"

#include "core/array.h"
#include "core/strings.h"
#include "core/types.h"

#include <assert.h>

#define BLOB_STORAGE_CAP 64
static_assert(sizeof((Fmt_Arg){0}.blob) == BLOB_STORAGE_CAP);

void fmt_printb_impl(
    String_Builder *b, const char *fmt_str, Fmt_Arg_Buffer args
) {
  static char true_cstr[4] = {'t', 'r', 'u', 'e'};
  static char false_cstr[5] = {'f', 'a', 'l', 's', 'e'};
  // NOTE(nico): All the builtin types use the fast path so no function pointer
  // call

  String fmt = from_cstring(fmt_str);
  if (fmt.len == 0) {
    return;
  }

  String_Reader reader = (String_Reader){
    .input = fmt,
    .current = 0,
  };

  usize token_count = 0;
  while (!string_reader_is_eof(&reader)) {
    char c = string_reader_advance(&reader);

    if (c == '{' && !string_reader_is_eof(&reader) &&
        string_reader_peek(&reader) == '}') {

      string_reader_advance(&reader);
      if (token_count >= args.len) {
        builder_write_string(b, from_cstring("{invalid argument}"));
        continue;
      }
      Fmt_Arg *arg = array_get_ptr(args, token_count);

      switch (arg->type_info.kind) {
      case Type_Kind_String:
        builder_write_string(b, arg->str);
        break;
      case Type_Kind_Cstring:
        builder_write_cstring(b, arg->cstr, cstring_len(arg->cstr));
        break;
      case Type_Kind_Char:
        builder_write_char(b, arg->c);
        break;
      case Type_Kind_Bool8:
        if (arg->b8) {
          builder_write_cstring(b, true_cstr, 4);
        } else {
          builder_write_cstring(b, false_cstr, 5);
        }
        break;
      case Type_Kind_Bool32:
        if (arg->b32) {
          builder_write_cstring(b, true_cstr, 4);
        } else {
          builder_write_cstring(b, false_cstr, 5);
        }
        break;
      case Type_Kind_Byte:
      case Type_Kind_U8:
      case Type_Kind_U16:
      case Type_Kind_U32:
        builder_write_i64(b, (i64)arg->u);
        break;
      case Type_Kind_U64:
      case Type_Kind_Usize:
        // NOTE(nico): unsupported and I'm too lazy to deal with it for now
        assert(false);
        break;
      case Type_Kind_I16:
      case Type_Kind_I32:
        builder_write_i32(b, (i32)arg->i);
        break;
      case Type_Kind_I64:
        builder_write_i64(b, arg->i);
        break;
      case Type_Kind_F32:
        builder_write_f32(b, arg->f32v, arg->precision);
        break;
      case Type_Kind_F64:
        builder_write_f64(b, arg->f64v, arg->precision);
        break;
      case Type_Kind_Struct:
        assert(false);
        break;
      }

      token_count += 1;
    } else {
      // FIXME(nico): can batch the non-token char.
      // Optimization for later though
      builder_write_char(b, c);
    }
  }
}
