#include "./allocator.h"

#include "./types.h"
#include "stdlib.h"

static Allocation_Result arena_alloc(Allocator allocator, usize size) {
  Arena_Data *arena = (Arena_Data *)allocator.ptr;
  Allocation_Result result = {0};

  if (size >= (arena->size - arena->offset)) {
    result.err = Allocation_Error_Out_Of_Memory;
    return result;
  }

  result.allocation = arena->buf + arena->offset;
  arena->offset += size;

  return result;
}

static Allocation_Result arena_free(Allocator allocator, void *old_data) {
  (void)allocator;
  (void)old_data;
  return (Allocation_Result){
    .err = Allocation_Error_Op_Not_Implemented,
  };
}

static Allocation_Result arena_free_all(Allocator allocator) {
  Arena_Data *arena = (Arena_Data *)allocator.ptr;
  arena->offset = 0;
  return (Allocation_Result){
    .err = Allocation_Error_None,
  };
}

void init_arena(Arena_Data *arena, byte *buf, usize size) {
  *arena = (Arena_Data){.buf = buf, .size = size, .offset = 0};
}

Allocator arena_allocator(Arena_Data *arena) {
  Allocator allocator = (Allocator){
    .ptr = arena,
    .alloc = arena_alloc,
    .free = arena_free,
    .free_all = arena_free_all,
  };
  return allocator;
}

static Allocation_Result heap_alloc(Allocator allocator, usize size) {
  (void)allocator;
  void *mem = malloc(size);

  Allocation_Result result = (Allocation_Result){
    .allocation = mem,
    .err = mem == 0 ? Allocation_Error_Out_Of_Memory : Allocation_Error_None,
  };

  return result;
}

static Allocation_Result heap_free(Allocator allocator, void *old_data) {
  (void)allocator;
  free(old_data);

  return (Allocation_Result){
    .err = Allocation_Error_None,
  };
}

static Allocation_Result heap_free_all(Allocator allocator) {
  (void)allocator;
  return (Allocation_Result){
    .err = Allocation_Error_Op_Not_Implemented,
  };
}

Allocator heap_allocator() {
  Allocator allocator = (Allocator){
    .ptr = 0,
    .alloc = heap_alloc,
    .free = heap_free,
    .free_all = heap_free_all,
  };

  return allocator;
}