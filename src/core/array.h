#ifndef CORE_ARRAY_H
#define CORE_ARRAY_H

#include "core/allocator.h"

#define Array(T)                                                               \
  struct {                                                                     \
    Allocator allocator;                                                       \
    T *items;                                                                  \
    usize len;                                                                 \
    bool8 is_dynamically_allocated;                                            \
  }

#define make_array(array_target, cap, _allocator)                              \
  ((typeof(array_target)){                                                     \
    .allocator = (_allocator),                                                 \
    .items = (_allocator)                                                      \
                 .alloc((_allocator), (cap) * sizeof(*((array_target).items))) \
                 .allocation,                                                  \
    .len = (cap),                                                              \
    .is_dynamically_allocated = false,                                         \
  })

#define delete_array(array)                                                    \
  do {                                                                         \
    if (((array).is_dynamically_allocated))                                    \
      (array).allocator.free((array).allocator, (array).items);                \
  } while (0)

#define array_get(array, i) ((array).items[(i)])
#define array_get_ptr(array, i) (&(array).items[(i)])
#define array_set(array, i, value) ((array).items[(i)] = (value))

// Inline array literal with auto-counted length. Expands to a brace
// initializer — use as a field initializer or prefix with a compound-literal
// type for function arguments: (GPU_Bind_Group_Create_Info)ARRAY_LIT(...).
// Allocator and is_dynamically_allocated are left zero-initialised.
#define ARRAY_LIT(T, ...)                                                      \
  {                                                                            \
    .items = (T[]){__VA_ARGS__},                                               \
    .len = sizeof((T[]){__VA_ARGS__}) / sizeof(T),                             \
  }

#endif