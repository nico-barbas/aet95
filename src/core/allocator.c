#include "core/allocator.h"

#include "core/types.h"

#include <assert.h>
#include <stdlib.h>

static Allocation_Result
arena_alloc(Allocator allocator, usize size, const char *file, usize line) {
  (void)file;
  (void)line;

  Arena_Data *arena = (Arena_Data *)allocator.ptr;

  usize offset = arena->offset;
  if (allocator.align > 0) {
    uintptr addr = (uintptr)(arena->buf + offset);
    usize remainder = (usize)(addr % allocator.align);

    if (remainder > 0) {
      offset += allocator.align - remainder;
    }
  }

  if (offset > arena->size || size > arena->size - offset) {
    return err(Allocation_Result, Allocation_Error_Out_Of_Memory);
  }

  rawptr ptr = arena->buf + offset;
  arena->offset = offset + size;

  return ok(Allocation_Result, ptr);
}

static Allocation_Error arena_free(Allocator allocator, void *old_data) {
  (void)allocator;
  (void)old_data;
  return Allocation_Error_Op_Not_Implemented;
}

static Allocation_Error arena_free_all(Allocator allocator) {
  Arena_Data *arena = (Arena_Data *)allocator.ptr;
  arena->offset = 0;

  return Allocation_Error_None;
}

void init_arena(Arena_Data *arena, byte *buf, usize size) {
  *arena = (Arena_Data){.buf = buf, .size = size, .offset = 0};
}

Arena_Transient_Memory arena_begin_transient_memory(Arena_Data *arena) {
  Arena_Transient_Memory mem = {
    .arena = arena,
    .previous_offset = arena->offset,
  };
  arena->transient_count += 1;
  return mem;
}

void arena_end_transient_memory(Arena_Transient_Memory mem) {
  assert(mem.arena->offset >= mem.previous_offset);
  assert(mem.arena->transient_count > 0);
  mem.arena->offset = mem.previous_offset;
  mem.arena->transient_count -= 1;
}

Allocator arena_allocator(Arena_Data *arena) {
  Allocator allocator = (Allocator){
    .ptr = arena,
    .align = 2 * sizeof(void *),
    .alloc_proc = arena_alloc,
    .free_proc = arena_free,
    .free_all_proc = arena_free_all,
  };
  return allocator;
}

////////////////////////////////////
// Tracking allocator
////////////////////////////////////

static usize tracking_header_size(Tracking_Allocator *data) {
  usize align = data->backing.align > 0 ? data->backing.align
                                        : alignof(Tracking_Allocator_Entry);
  return (sizeof(Tracking_Allocator_Entry) + align - 1) & ~(align - 1);
}

static Tracking_Allocator_Entry *
tracking_entry_from_ptr(Tracking_Allocator *data, rawptr ptr) {
  return (Tracking_Allocator_Entry *)((uintptr)ptr -
                                      tracking_header_size(data));
}

static Allocation_Result
tracking_alloc(Allocator allocator, usize size, const char *file, usize line) {
  Tracking_Allocator *data = (Tracking_Allocator *)allocator.ptr;

  usize header_size = tracking_header_size(data);
  Tracking_Allocator_Entry *entry = (Tracking_Allocator_Entry *)try(
      Allocation_Result, alloc(data->backing, header_size + size)
  );

  entry->file = file;
  entry->line = line;
  // NOTE(nico): This seems super useless since one cannot go without the other
  // and as long as we have one we can find the other
  entry->ptr = (byte *)entry + header_size;
  entry->size = size;
  entry->align = data->backing.align;
  entry->previous = data->last;
  entry->next = nullptr;

  if (data->last != nullptr) {
    data->last->next = entry;
  }
  if (data->first == nullptr) {
    data->first = entry;
  }
  data->last = entry;

  return ok(Allocation_Result, entry->ptr);
}

static Allocation_Error tracking_free(Allocator allocator, rawptr old_data) {
  // FIXME(nico): It sounds super dodgy to get the entry from an untrusted
  // pointer but since we do not know the strategy of the backing allocator, it
  // is a bit hard to validate it.
  Tracking_Allocator *data = (Tracking_Allocator *)allocator.ptr;

  if (old_data == nullptr) {
    return Allocation_Error_Bad_Free;
  }

  Tracking_Allocator_Entry *entry = tracking_entry_from_ptr(data, old_data);
  Tracking_Allocator_Entry *current = data->first;
  while (current != nullptr) {
    if (entry == current) {
      if (entry->previous != nullptr) {
        entry->previous->next = entry->next;
      } else {
        data->first = entry->next;
      }
      if (entry->next != nullptr) {
        entry->next->previous = entry->previous;
      } else {
        data->last = entry->previous;
      }

      return free_(data->backing, entry);
    }

    current = current->next;
  }

  // FIXME(nico): This is where we keep track of bad frees
  return Allocation_Error_Bad_Free;
}

static Allocation_Error tracking_free_all(Allocator allocator) {
  Tracking_Allocator *data = (Tracking_Allocator *)allocator.ptr;

  Allocation_Error err = free_all(data->backing);
  if (err == Allocation_Error_Op_Not_Implemented) {
    err = Allocation_Error_None;

    Tracking_Allocator_Entry *current = data->first;
    while (current != nullptr) {
      Tracking_Allocator_Entry *next = current->next;

      Allocation_Error free_err = free_(data->backing, current);
      if (free_err != Allocation_Error_None) {
        err = free_err;
      }
      current = next;
    }
  }

  data->first = nullptr;
  data->last = nullptr;

  return err;
}

void init_tracking_allocator(Tracking_Allocator *data, Allocator backing) {
  *data = (Tracking_Allocator){.backing = backing};
}

Allocator tracking_allocator(Tracking_Allocator *data) {
  return (Allocator){
    .ptr = data,
    .align = data->backing.align,
    .alloc_proc = tracking_alloc,
    .free_proc = tracking_free,
    .free_all_proc = tracking_free_all,
  };
}

////////////////////////////////////
// General purpose malloc wrapper
////////////////////////////////////

static Allocation_Result
heap_alloc(Allocator allocator, usize size, const char *file, usize line) {
  (void)file;
  (void)line;
  (void)allocator;
  rawptr mem = malloc(size);

  if (mem == nullptr) {
    return err(Allocation_Result, Allocation_Error_Out_Of_Memory);
  }

  return ok(Allocation_Result, mem);
}

static Allocation_Error heap_free(Allocator allocator, void *old_data) {
  (void)allocator;
  free(old_data);

  return Allocation_Error_None;
}

static Allocation_Error heap_free_all(Allocator allocator) {
  (void)allocator;
  return Allocation_Error_Op_Not_Implemented;
}

Allocator heap_allocator() {
  Allocator allocator = (Allocator){
    .ptr = 0,
    .align = 2 * sizeof(void *),
    .alloc_proc = heap_alloc,
    .free_proc = heap_free,
    .free_all_proc = heap_free_all,
  };

  return allocator;
}