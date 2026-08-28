#ifndef CORE_LIST_H
#define CORE_LIST_H

#include "core/allocator.h"
#include "core/types.h"

#include <string.h>

typedef enum List_Error {
  List_Error_None,
  List_Error_Failed_To_Allocate,
  List_Error_Failed_To_Grow,
  List_Error_Failed_To_Insert,
  List_Error_Failed_To_Remove_Range,
} List_Error;

#endif

#ifndef LIST_TYPE
#error "LIST_TYPE must be defined before include list.h"
#endif

#ifndef LIST_TYPE_NAME
#error "LIST_TYPE_NAME must be defined before include list.h"
#endif

#ifndef LIST_FUNCTION_PREFIX
#error "LIST_FUNCTION_PREFIX must be defined before include list.h"
#endif

#define DEFAULT_LIST_SIZE 32

#define CONCAT_RAW(a, b) a##b
#define CONCAT(a, b) CONCAT_RAW(a, b)
#define LIST_FUNCTION(name) CONCAT(LIST_FUNCTION_PREFIX, name)

#define MAKE_RESULT CONCAT(LIST_TYPE_NAME, _Make_Result)
#define PUSH_RESULT CONCAT(LIST_TYPE_NAME, _Push_Result)
#define INSERT_RESULT CONCAT(LIST_TYPE_NAME, _Insert_Result)
#define LIST_TYPE_OPTION CONCAT(LIST_TYPE_NAME, _Pop_Option)

typedef struct LIST_TYPE_NAME {
  Allocator allocator;
  LIST_TYPE *items;
  usize cap;
  usize len;
} LIST_TYPE_NAME;

typedef Result(LIST_TYPE_NAME, List_Error) MAKE_RESULT;
typedef Result(usize, List_Error) PUSH_RESULT;
typedef Result(usize, List_Error) INSERT_RESULT;
typedef Option(LIST_TYPE) LIST_TYPE_OPTION;

static inline MAKE_RESULT
CONCAT(make_, LIST_FUNCTION_PREFIX)(usize cap, Allocator allocator) {
  LIST_TYPE_NAME list = {
    .allocator = allocator,
    .len = 0,
    .cap = cap,
  };

  if (cap > 0) {
    Allocation_Result alloc =
        allocator.alloc(allocator, cap * sizeof(LIST_TYPE));
    if (alloc.err != Allocation_Error_None) {
      return err(MAKE_RESULT, List_Error_Failed_To_Allocate);
    }
    list.items = (LIST_TYPE *)alloc.allocation;
  }

  return ok(MAKE_RESULT, list);
}

static inline bool32
CONCAT(delete_, LIST_FUNCTION_PREFIX)(LIST_TYPE_NAME *list) {
  Allocation_Result free = list->allocator.free(list->allocator, list->items);
  if (free.err == Allocation_Error_None ||
      free.err == Allocation_Error_Op_Not_Implemented) {
    list->cap = 0;
    list->len = 0;
    list->items = nullptr;

    return true;
  } else {
    return false;
  }
}

static inline bool32
LIST_FUNCTION(_reserve)(LIST_TYPE_NAME *list, usize new_cap) {
  if (new_cap <= list->cap) {
    return true;
  }

  if (new_cap > 0) {
    Allocation_Result alloc =
        list->allocator.alloc(list->allocator, new_cap * sizeof(LIST_TYPE));
    if (alloc.err != Allocation_Error_None) {
      return false;
    }

    LIST_TYPE *old_data = list->items;
    list->items = (LIST_TYPE *)alloc.allocation;
    memcpy(list->items, old_data, list->len * sizeof(LIST_TYPE));

    // NOTE(nico): Whether it frees successfully or not is not really important
    // here and is an error with the allocator itself, but maybe we could handle
    // it. The one OK status should be NONE and NOT_IMPLEMENTED
    list->allocator.free(list->allocator, old_data);
  }

  list->cap = new_cap;
  return true;
}

static inline PUSH_RESULT
LIST_FUNCTION(_push)(LIST_TYPE_NAME *list, LIST_TYPE *item) {
  if (list->len >= list->cap) {
    usize new_cap = list->cap == 0 ? DEFAULT_LIST_SIZE : list->cap * 2;

    if (!LIST_FUNCTION(_reserve)(list, new_cap)) {
      return err(PUSH_RESULT, List_Error_Failed_To_Grow);
    }
  }

  usize i = list->len;
  list->items[list->len++] = *item;
  return ok(PUSH_RESULT, i);
}

// NOTE(nico): A bit unsure about this behavior, but for my personal need, I
// think I prefer it to error when inserting outside of the current length
static inline INSERT_RESULT
LIST_FUNCTION(_insert_at)(LIST_TYPE_NAME *list, LIST_TYPE *item, usize i) {
  if (i > list->len) {
    return err(INSERT_RESULT, List_Error_Failed_To_Insert);
  }

  if (i == list->len) {
    usize index = try(INSERT_RESULT, LIST_FUNCTION(_push)(list, item));
    return ok(INSERT_RESULT, index);
  }

  if (list->len >= list->cap) {
    usize new_cap = list->cap == 0 ? DEFAULT_LIST_SIZE : list->cap * 2;

    if (!LIST_FUNCTION(_reserve)(list, new_cap)) {
      return err(INSERT_RESULT, List_Error_Failed_To_Grow);
    }
  }

  usize copy_len = list->len - i;
  memmove(list->items + i + 1, list->items + i, copy_len * sizeof(LIST_TYPE));
  list->items[i] = *item;
  list->len += 1;
  return ok(INSERT_RESULT, i);
}

static inline List_Error LIST_FUNCTION(_ordered_remove_range)(
    LIST_TYPE_NAME *list, usize i, usize count
) {
  if (i + count > list->len) {
    return List_Error_Failed_To_Remove_Range;
  }

  usize rem = list->len - (i + count);
  memmove(list->items + i, list->items + i + count, rem * sizeof(LIST_TYPE));
  list->len -= count;
  return List_Error_None;
}

static inline LIST_TYPE_OPTION LIST_FUNCTION(_pop)(LIST_TYPE_NAME *list) {
  if (list->len == 0) {
    return none(LIST_TYPE_OPTION);
  }

  return some(LIST_TYPE_OPTION, list->items[--list->len]);
}

#undef LIST_TYPE_OPTION
#undef PUSH_RESULT
#undef MAKE_RESULT
#undef LIST_FUNCTION
#undef LIST_FUNCTION_PREFIX
#undef LIST_TYPE
#undef LIST_TYPE_NAME
#undef CONCAT
#undef CONCAT_RAW
#undef DEFAULT_LIST_SIZE