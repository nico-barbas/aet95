#ifndef CORE_ALLOCATOR_H
#define CORE_ALLOCATOR_H

#include "core/types.h"

#define KILOBYTE 1024
#define MEGABYTE (1024 * KILOBYTE)

// NOTE(nico): The free naming is sad but need it to not collide with libc's
// free
#define alloc(allocator, size)                                                 \
  ((allocator).alloc_proc((allocator), size, __FILE__, __LINE__))
#define free_(allocator, old_data)                                             \
  ((allocator).free_proc((allocator), old_data))
#define free_all(allocator) ((allocator).free_all_proc((allocator)))

typedef enum Allocation_Error {
  Allocation_Error_None,
  Allocation_Error_Out_Of_Memory,
  Allocation_Error_Op_Not_Implemented,
  Allocation_Error_Bad_Free,
} Allocation_Error;

typedef Result(rawptr, Allocation_Error) Allocation_Result;

typedef struct Allocator Allocator;
struct Allocator {
  rawptr ptr;
  usize align;
  Allocation_Result (*alloc_proc)(
      Allocator allocator, usize size, const char *file, usize line
  );
  Allocation_Error (*free_proc)(Allocator allocator, rawptr old_data);
  Allocation_Error (*free_all_proc)(Allocator allocator);
};

////////////////////////////////////
// Arena allocator
////////////////////////////////////

typedef struct Arena_Data {
  byte *buf;
  usize size;
  usize offset;
  usize transient_count;
} Arena_Data;

typedef struct Arena_Transient_Memory {
  Arena_Data *arena;
  usize previous_offset;
} Arena_Transient_Memory;

void init_arena(Arena_Data *arena, byte *buf, usize size);
Arena_Transient_Memory arena_begin_transient_memory(Arena_Data *arena);
void arena_end_transient_memory(Arena_Transient_Memory mem);

Allocator arena_allocator(Arena_Data *arena);

////////////////////////////////////
// Tracking allocator
////////////////////////////////////
typedef struct Tracking_Allocator_Entry Tracking_Allocator_Entry;
struct Tracking_Allocator_Entry {
  const char *file;
  usize line;
  rawptr ptr;
  usize size;
  usize align;
  Tracking_Allocator_Entry *previous;
  Tracking_Allocator_Entry *next;
};

typedef struct Tracking_Allocator {
  Allocator backing;
  Tracking_Allocator_Entry *first;
  Tracking_Allocator_Entry *last;

  // NOTE(nico): track more stuff, like total memory, peak memory, number of bad
  // frees, etc..
} Tracking_Allocator;

void init_tracking_allocator(Tracking_Allocator *data, Allocator backing);
Allocator tracking_allocator(Tracking_Allocator *data);

////////////////////////////////////
// General purpose malloc wrapper
////////////////////////////////////
Allocator heap_allocator(void);

#endif