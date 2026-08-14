#ifndef CORE_TYPES_H
#define CORE_TYPES_H

typedef char bool8;
typedef unsigned int bool32;

// #define false 0
// #define true 1

typedef void *rawptr;

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

typedef unsigned long long int usize;

#define countof(array) (sizeof(array) / sizeof((array)[0]))

#define Option(type)                                                           \
  struct {                                                                     \
    bool32 some;                                                               \
    type value;                                                                \
  }

#define some(T, val) ((T){.some = true, .value = (val)})
#define none(T) ((T){.some = false})

#define Result(type)                                                           \
  struct {                                                                     \
    bool32 ok;                                                                 \
    type value;                                                                \
  }
#define ok(T, val) ((T){.ok = true, .value = (val)})
#define err(T) ((T){.ok = false})

#endif