#ifndef CORE_STRINGS_H
#define CORE_STRINGS_H

#include "core/allocator.h"
#include "core/types.h"

typedef struct String {
  rawptr ptr;
  const char *data;
  usize len;
  bool8 is_owned;
  bool8 is_dynamically_allocated;
} String;

typedef struct String_Builder {
  char *buf;
  usize cap;
  usize len;
} String_Builder;

typedef struct String_Reader {
  String input;
  usize current;
} String_Reader;

void delete_string(String str, Allocator allocator);

usize c_str_len(const char *c_str);
String from_c_str(const char *str);
String string_slice(String src, usize lo, usize hi);
String string_clone(String str, Allocator allocator);
String string_clone_terminated(String str, Allocator allocator);
bool32 string_is_terminated(String str);
bool32 string_equal(String s1, String s2);
// FIXME(nico): Conversion procedures need to handle overflows gracefully
bool32 string_to_u16(String str, u16 *out);
bool32 string_to_u32(String str, u32 *out);
bool32 string_to_i64(String str, i64 *out);

String_Builder make_builder_from_buf(char *buf, usize cap);
void builder_reset(String_Builder *b);
bool32 builder_write(String_Builder *b, const char *fmt, ...);
void builder_write_i32(String_Builder *b, i32 n);
void builder_write_f32(String_Builder *b, f32 f, i32 precision);
void builder_write_char(String_Builder *b, char c);
void builder_write_raw_string(String_Builder *b, char *buf, usize size);
void builder_write_string(String_Builder *b, String str);
char *builder_terminate_string(String_Builder *b);
String builder_get_string(String_Builder *b);
String builder_clone_string(String_Builder *b, Allocator allocator);

bool32 string_reader_is_eof(String_Reader *reader);
char string_reader_advance(String_Reader *reader);
char string_reader_peek(String_Reader *reader);

String filepath_get_dir(String path);

// NOTE(nico): Useful char and string methods for parsers
bool32 char_is_number(char c);

#endif
