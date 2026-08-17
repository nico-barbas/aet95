#include "strings.h"

#include "core/allocator.h"
#include "core/types.h"

#include <assert.h>
#include <math.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

////////////////////////////////////////
// String operations
////////////////////////////////////////
void delete_string(String str) {
  if (!str.allocator.some || str.ptr == nullptr) {
    return;
  }

  Allocator allocator = str.allocator.value;
  allocator.free(allocator, str.ptr);
}

usize c_str_len(const char *c_str) {
  usize len = 0;
  while (c_str[len] != '\0') {
    len += 1;
  }

  return len;
}

String from_c_str(const char *str) {
  return (String){
    .data = str,
    .len = c_str_len(str),
  };
}

String string_slice(String str, usize lo, usize hi) {
  assert(lo < str.len && hi > 0 && hi <= str.len && lo < hi);
  return (String){
    .allocator = {.some = false},
    .data = &str.data[lo],
    .len = hi - lo,
  };
}

// FIXME(nico): This is terrible.. This proc should not null terminate the
// cloned string. If it does, it should be a parameter.
String string_clone(String str, Allocator allocator) {
  Allocation_Result alloc =
      allocator.alloc(allocator, sizeof(char) * str.len + 1);
  if (alloc.err != Allocation_Error_None) {
    return (String){0};
  }

  char *result = (char *)alloc.allocation;

  memcpy(result, str.data, sizeof(char) * str.len);
  result[str.len] = '\0';
  return (String){
    .allocator = {.some = true, .value = allocator},
    .ptr = result,
    .data = result,
    .len = str.len + 1,
  };
}

bool32 string_is_terminated(String str) {
  return (str.data[str.len] == '\0');
}

bool32 string_equal(String s1, String s2) {
  if (s1.len != s2.len) {
    return false;
  }

  return memcmp(s1.data, s2.data, s1.len * sizeof(char)) == 0;
}

bool32 string_to_u16(String str, u16 *out) {
  u16 n = 0;
  for (usize i = 0; i < str.len; i += 1) {
    if (!char_is_number(str.data[i])) {
      return false;
    }

    n = n * 10 + (u16)(str.data[i] - '0');
  }

  *out = n;
  return true;
}

bool32 string_to_u32(String str, u32 *out) {
  u32 n = 0;
  for (usize i = 0; i < str.len; i += 1) {
    if (!char_is_number(str.data[i])) {
      return false;
    }

    n = n * 10 + (u32)(str.data[i] - '0');
  }

  *out = n;
  return true;
}

bool32 string_to_i64(String str, i64 *out) {
  i64 n = 0;
  i64 sign = 1;

  usize i = 0;
  if (str.data[0] == '-') {
    sign = -1;
    i += 1;
  }

  for (; i < str.len; i += 1) {
    if (!char_is_number(str.data[i])) {
      return false;
    }

    n = n * 10 + (i64)(str.data[i] - '0');
  }

  *out = n * sign;
  return true;
}

////////////////////////////////////////
// String builder
////////////////////////////////////////
typedef enum String_Builder_Format {
  String_Builder_Format_Invalid,
  String_Builder_Format_Integer32,
  String_Builder_Format_Float32,
  String_Builder_Format_Char,
  String_Builder_Format_C_String,
  String_Builder_Format_String,
} String_Builder_Format;

String_Builder make_builder_from_buf(char *buf, usize cap) {
  String_Builder b = (String_Builder){
    .buf = buf,
    .cap = cap,
    .len = 0,
  };

  return b;
}

void builder_reset(String_Builder *b) {
  b->len = 0;
  b->buf[0] = '\0';
}

// QUALITY(nico): 8/10
// - Handling of the last part of the format string is not the best
// - Doesn't really handle errors if too may arguments are passed through
bool32 builder_write(String_Builder *b, const char *fmt_str, ...) {
  String fmt = from_c_str(fmt_str);
  String_Reader reader = (String_Reader){
    .input = fmt,
    .current = 0,
  };

  i32 token_count = 0;
  struct {
    String_Builder_Format format;
    usize start;
    usize end;
  } tokens[16] = {0};

  while (!string_reader_is_eof(&reader) && token_count < 16) {
    char c = string_reader_advance(&reader);

    if (c == '%') {
      if (string_reader_is_eof(&reader)) {
        break;
      }

      usize start = reader.current - 1;
      char next = string_reader_advance(&reader);

      switch (next) {
      case 'd':
        tokens[token_count].format = String_Builder_Format_Integer32;
        break;
      case 'f':
        tokens[token_count].format = String_Builder_Format_Float32;
        break;
      case 'c':
        tokens[token_count].format = String_Builder_Format_Char;
        break;
      case 's': {
        bool32 is_cstr = !string_reader_is_eof(&reader) &&
                         string_reader_peek(&reader) == 's';
        tokens[token_count].format = is_cstr ? String_Builder_Format_C_String
                                             : String_Builder_Format_String;

        if (is_cstr) {
          string_reader_advance(&reader);
        }
      } break;
      default:
        return false;
      }

      tokens[token_count].start = start;
      tokens[token_count].end = reader.current;
      token_count += 1;
    }
  }

  va_list arg_ptr;
  va_start(arg_ptr, fmt_str);

  usize current = 0;
  for (i32 i = 0; i < token_count; i += 1) {
    if (tokens[i].start > current) {
      builder_write_string(b, string_slice(fmt, current, tokens[i].start));
    }
    current = tokens[i].end;

    switch (tokens[i].format) {
    case String_Builder_Format_Invalid:
      assert(false);
      break;
    case String_Builder_Format_Integer32: {
      i32 val = va_arg(arg_ptr, i32);
      builder_write_i32(b, val);
    } break;
    case String_Builder_Format_Float32: {
      // TODO(nico): support higher precision
      f32 val = (f32)va_arg(arg_ptr, f64);
      builder_write_f32(b, val, 2);
    } break;
    case String_Builder_Format_Char: {
      char val = (char)va_arg(arg_ptr, i32);
      builder_write_char(b, val);
    } break;
    case String_Builder_Format_C_String: {
      char *val = va_arg(arg_ptr, char *);
      builder_write_string(b, from_c_str(val));
    } break;
    case String_Builder_Format_String: {
      String val = va_arg(arg_ptr, String);
      builder_write_string(b, val);
    } break;
    }
  }

  va_end(arg_ptr);

  if (current < fmt.len) {
    builder_write_string(b, string_slice(fmt, current, fmt.len));
  }

  return true;
}

void builder_write_i32(String_Builder *b, i32 n) {
  if (n == 0) {
    b->buf[b->len] = '0';
    b->len += 1;
    return;
  }

  if (n < 0) {
    b->buf[b->len] = '-';
    b->len += 1;
  }
  n = abs(n);

  i32 count = 0;
  i32 rem = n;
  while (rem > 0) {
    count += 1;
    rem /= 10;
  }

  rem = n;
  i32 divisor = (i32)(powf(10, (f32)(count - 1)));
  for (i32 i = 0; i < count; i += 1) {
    if (b->len >= b->cap) {
      builder_terminate_string(b);
      return;
    }

    i32 digit = rem / divisor;
    b->buf[b->len] = (char)(48 + digit);
    b->len += 1;
    rem %= divisor;
    divisor /= 10;
  }
}

void builder_write_f32(String_Builder *b, f32 f, i32 precision) {
  builder_write_i32(b, (i32)f);

  i32 fract = abs((i32)((f - (f32)((i32)f)) * (powf(10.0f, (f32)precision))));
  if (fract > 0) {
    builder_write_char(b, '.');
    builder_write_i32(b, fract);
  }
}

void builder_write_char(String_Builder *b, char c) {
  if (b->len >= b->cap) {
    builder_terminate_string(b);
    return;
  }

  b->buf[b->len] = c;
  b->len += 1;
}

void builder_write_raw_string(String_Builder *b, char *buf, usize size) {
  for (usize i = 0; i < size; i += 1) {
    if (b->len >= b->cap) {
      builder_terminate_string(b);
      return;
    }
    b->buf[b->len] = buf[i];
    b->len += 1;
  }
}

void builder_write_string(String_Builder *b, String str) {
  for (usize i = 0; i < str.len; i += 1) {
    if (b->len >= b->cap) {
      builder_terminate_string(b);
      return;
    }
    b->buf[b->len] = str.data[i];
    b->len += 1;
  }
}

char *builder_terminate_string(String_Builder *b) {
  if (b->len >= b->cap) {
    b->len = b->cap - 1;
  }

  b->buf[b->len] = '\0';

  return b->buf;
}

String builder_get_string(String_Builder *b) {
  return (String){
    .data = b->buf,
    .len = b->len,
  };
}

String builder_clone_string(String_Builder *b, Allocator allocator) {
  Allocation_Result alloc = allocator.alloc(allocator, sizeof(char) * b->len);
  if (alloc.err != Allocation_Error_None) {
    return (String){0};
  }

  memcpy(alloc.allocation, b->buf, sizeof(char) * b->len);
  return (String){
    .data = alloc.allocation,
    .len = b->len,
  };
}

bool32 string_reader_is_eof(String_Reader *reader) {
  return reader->current >= reader->input.len;
}

char string_reader_advance(String_Reader *reader) {
  return reader->input.data[reader->current++];
}

char string_reader_peek(String_Reader *reader) {
  return reader->input.data[reader->current];
}

String filepath_get_dir(String path) {
  usize len = path.len;

  for (i32 i = (i32)path.len - 1; i >= 0; i -= 1) {
    len -= 1;
    if (path.data[i] == '/' || path.data[i] == '\\') {
      break;
    }
  }

  return (String){
    .data = path.data,
    .len = len,
  };
}

////////////////////////////////////////
// Helper operations
////////////////////////////////////////
bool32 char_is_number(char c) {
  return c >= '0' && c <= '9';
}
