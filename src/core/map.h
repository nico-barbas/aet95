#ifndef CORE_MAP
#define CORE_MAP

#include "core/allocator.h"
#include "core/types.h"

/*
  FIXME(nico):
  - Should return a result so it is safer to check for allocation error (see
  list.h)
*/

typedef u64 (*Open_Map_Hash_Proc)(const void *key, usize key_size);
typedef bool32 (*Open_Map_Key_Eq)(void *k1, void *k2);

typedef enum Open_Map_Slot_State {
  Open_Map_Slot_State_Empty,
  Open_Map_Slot_State_Occupied,
  Open_Map_Slot_State_Tombstone,
} Open_Map_Slot_State;

// NOTE(nico): a slot is laid out as [state][pad][key][pad][value][pad]. Both
// payload offsets are baked at init so no accessor has to guess the padding
typedef struct Open_Map_Header {
  usize len;
  usize cap;
  usize key_size;
  usize key_align;
  usize key_offset;
  usize value_size;
  usize value_align;
  usize value_offset;
  usize slot_size;
  Allocator allocator;
  Open_Map_Hash_Proc hash;
  Open_Map_Key_Eq key_eq;
} Open_Map_Header;
typedef void *Open_Map;

#define make_open_map(K, V, cap, hash_proc, eq_proc, allocator)                \
  open_map_init(                                                               \
      sizeof(K),                                                               \
      _Alignof(K),                                                             \
      sizeof(V),                                                               \
      _Alignof(V),                                                             \
      cap,                                                                     \
      hash_proc,                                                               \
      eq_proc,                                                                 \
      allocator                                                                \
  )
#define open_map_header(m) (((Open_Map_Header *)(m)) - 1)
#define open_map_set(m, k, v)                                                  \
  do {                                                                         \
    (m) = open_map_ensure_cap(m, 1);                                           \
    typeof(k) key = k;                                                         \
    typeof(v) value = v;                                                       \
    open_map_insert_kv(m, &key, &value);                                       \
  } while (0)
#define open_map_get(m, k) internal_open_map_get((m), &(k))
#define open_map_remove(m, k) open_map_remove_raw((m), &(k))
#define open_map_len(m) (open_map_header(m))->len

typedef struct Open_Map_Iterator {
  Open_Map _map;
  usize _index;
  void *key;
  void *value;
} Open_Map_Iterator;

Open_Map open_map_init(
    usize key_size,
    usize key_align,
    usize value_size,
    usize value_align,
    usize cap,
    Open_Map_Hash_Proc hash_proc,
    Open_Map_Key_Eq eq_proc,
    Allocator allocator
);
void delete_open_map(Open_Map map);
Open_Map open_map_ensure_cap(Open_Map map, usize item_count);
bool32 open_map_insert_kv(Open_Map map, void *key, void *value);
void *internal_open_map_get(Open_Map map, void *key);
bool32 open_map_remove_raw(Open_Map map, void *key);

Open_Map_Iterator open_map_iterator(Open_Map map);
bool32 open_map_has_next(Open_Map_Iterator *it);
void *open_map_next(Open_Map_Iterator *it);

//////////////////////////////
// Provided defaults
//////////////////////////////
u64 open_map_u32_hash(const void *key, usize key_size);
bool32 open_map_u32_eq(void *k1, void *k2);

u64 open_map_string_hash(const void *key, usize key_size);
bool32 open_map_string_eq(void *k1, void *k2);
#define make_string_open_map(V, cap, _allocator)                               \
  make_open_map(                                                               \
      String, V, cap, open_map_string_hash, open_map_string_eq, _allocator     \
  )

#endif