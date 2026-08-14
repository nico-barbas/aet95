#ifndef CORE_ALLOCATOR_H
#define CORE_ALLOCATOR_H

#include "./types.h"

#define KILOBYTE 1024
#define MEGABYTE (1024 * KILOBYTE)

typedef enum Allocation_Error {
  Allocation_Error_None,
  Allocation_Error_Out_Of_Memory,
  Allocation_Error_Op_Not_Implemented,
} Allocation_Error;

typedef struct Allocation_Result {
  Allocation_Error err;
  void *allocation;
} Allocation_Result;

typedef struct Allocator Allocator;
struct Allocator {
  void *ptr;
  Allocation_Result (*alloc)(Allocator allocator, usize size);
  Allocation_Result (*free)(Allocator allocator, void *old_data);
  Allocation_Result (*free_all)(Allocator allocator);
};

typedef struct Arena_Data {
  byte *buf;
  usize size;
  usize offset;
} Arena_Data;

void init_arena(Arena_Data *arena, byte *buf, usize size);
Allocator arena_allocator(Arena_Data *arena);

Allocator heap_allocator(void);

#endif