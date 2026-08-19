#ifndef UI_CONTEXT_H
#define UI_CONTEXT_H

#include "core/allocator.h"
#include "core/types.h"

#define LIST_TYPE usize
#define LIST_TYPE_NAME Element_Index_List
#define LIST_FUNCTION_PREFIX element_index_list
#include "core/list.h"

typedef enum Element_Context_Flag {
  Element_Context_Flag_Validation_Layer = 1 << 0,
} Element_Context_Flag;

typedef struct Element_Context {
  Allocator allocator;
  Element_Context_Flag flags;

} Element_Context;

#endif