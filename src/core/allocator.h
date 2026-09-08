#ifndef CORE_ALLOCATOR_H
#define CORE_ALLOCATOR_H

#include "core/types.h"

#define KILOBYTE 1024
#define MEGABYTE (1024 * KILOBYTE)

typedef enum Allocation_Error {
  Allocation_Error_None,
  Allocation_Error_Out_Of_Memory,
  Allocation_Error_Op_Not_Implemented,
} Allocation_Error;

typedef struct Allocation_Result {
  Allocation_Error err;
  rawptr allocation;
} Allocation_Result;

typedef struct Allocator Allocator;
struct Allocator {
  rawptr ptr;
  usize align;
  Allocation_Result (*alloc)(Allocator allocator, usize size);
  Allocation_Result (*free)(Allocator allocator, rawptr old_data);
  Allocation_Result (*free_all)(Allocator allocator);
};

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

Allocator heap_allocator(void);

#endif