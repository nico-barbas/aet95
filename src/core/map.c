#include "core/map.h"

#include "core/allocator.h"
#include "core/math.h"
#include "core/types.h"
#include "string.h"

static usize align_up(usize n, usize align) {
  return (n + align - 1) & ~(align - 1);
}

Open_Map open_map_init(
    usize key_size,
    usize value_size,
    usize value_align,
    usize cap,
    Open_Map_Hash_Proc hash_proc,
    Open_Map_Key_Eq eq_proc,
    Allocator allocator
) {
  void *ptr = nullptr;
  if (allocator.alloc == nullptr) {
    return ptr;
  }

  usize value_offset =
      align_up(sizeof(Open_Map_Slot_State) + key_size, value_align);
  usize slot_size = align_up(value_offset + value_size, value_align);
  usize total_size = slot_size * cap + sizeof(Open_Map_Header);

  Allocation_Result alloc = allocator.alloc(allocator, total_size);
  if (alloc.err != Allocation_Error_None) {
    return ptr;
  }

  memset(alloc.allocation, 0, total_size);

  Open_Map_Header *header = (Open_Map_Header *)alloc.allocation;
  header->allocator = allocator;
  header->cap = cap;
  header->len = 0;
  header->key_size = key_size;
  header->value_size = value_size;
  header->value_align = value_align;
  header->slot_size = slot_size;
  header->hash = hash_proc;
  header->key_eq = eq_proc;

  ptr = header + 1;
  return ptr;
}

void delete_open_map(Open_Map map) {
  Open_Map_Header *header = open_map_header(map);
  header->allocator.free(header->allocator, header);
}

static void *open_map_get_key_ptr(void *slot) {
  return (byte *)slot + sizeof(Open_Map_Slot_State);
}

static void *open_map_get_value_ptr(void *slot, Open_Map_Header *header) {
  usize value_offset = align_up(
      sizeof(Open_Map_Slot_State) + header->key_size, header->value_align
  );
  return (byte *)slot + value_offset;
}

Open_Map open_map_ensure_cap(Open_Map map, usize item_count) {
#define MAX_LOAD_FACTOR 0.75f
  Open_Map_Header *header = open_map_header(map);

  if ((f32)(header->len + item_count) / (f32)header->cap < MAX_LOAD_FACTOR) {
    return map;
  }

  Open_Map new_map = open_map_init(
      header->key_size,
      header->value_size,
      header->value_align,
      header->cap * 2,
      header->hash,
      header->key_eq,
      header->allocator
  );

  // FIXME(nico): if realloc fails we cannot recover. Don't really know what it
  // is an acceptable way to handle this

  for (usize i = 0; i < header->cap; i += 1) {
    void *slot = (byte *)map + (i * header->slot_size);

    Open_Map_Slot_State *slot_state = (Open_Map_Slot_State *)slot;
    if (*slot_state == Open_Map_Slot_State_Occupied) {
      void *key = open_map_get_key_ptr(slot);
      void *value = open_map_get_value_ptr(slot, header);

      open_map_insert_kv(new_map, key, value);
    }
  }

  delete_open_map(map);
  return new_map;
}

// FIXME(nico): Possibility of phantom duplicate if tombstone before actual slot
// for given key
bool32 open_map_insert_kv(Open_Map map, void *key, void *value) {
  Open_Map_Header *header = open_map_header(map);

  u64 hash = header->hash(key, header->key_size);
  usize index = (usize)(hash % header->cap);

  usize starting_index = index;
  do {
    void *slot = (byte *)map + (index * header->slot_size);

    Open_Map_Slot_State *slot_state = (Open_Map_Slot_State *)slot;

    bool32 is_clear_slot = *slot_state == Open_Map_Slot_State_Empty ||
                           *slot_state == Open_Map_Slot_State_Tombstone;

    if (is_clear_slot || header->key_eq(key, open_map_get_key_ptr(slot))) {
      *slot_state = Open_Map_Slot_State_Occupied;
      memcpy((byte *)slot + sizeof(Open_Map_Slot_State), key, header->key_size);
      memcpy(open_map_get_value_ptr(slot, header), value, header->value_size);
      header->len += is_clear_slot ? 1 : 0;
      return true;
    }

    index = (index + 1) % header->cap;
  } while (index != starting_index);

  return false;
}

void *internal_open_map_get(Open_Map map, void *key) {
  Open_Map_Header *header = open_map_header(map);

  u64 hash = header->hash(key, header->key_size);
  usize index = (usize)(hash % header->cap);

  usize starting_index = index;
  do {
    void *slot = (byte *)map + (index * header->slot_size);

    Open_Map_Slot_State *slot_state = (Open_Map_Slot_State *)slot;

    if (*slot_state == Open_Map_Slot_State_Empty) {
      return nullptr;
    }

    if (*slot_state == Open_Map_Slot_State_Occupied &&
        header->key_eq(key, open_map_get_key_ptr(slot))) {
      return open_map_get_value_ptr(slot, header);
    }

    index = (index + 1) % header->cap;
  } while (index != starting_index);

  return nullptr;
}

Open_Map_Iterator open_map_iterator(Open_Map map) {
  Open_Map_Header *header = open_map_header(map);
  usize index = 0;
  while (index < header->cap) {
    void *slot = (byte *)map + index * header->slot_size;

    if (*(Open_Map_Slot_State *)slot == Open_Map_Slot_State_Occupied) {
      break;
    }
    index += 1;
  }
  return (Open_Map_Iterator){._map = map, ._index = index};
}

bool32 open_map_has_next(Open_Map_Iterator *it) {
  Open_Map_Header *header = open_map_header(it->_map);
  return it->_index < header->cap;
}

void *open_map_next(Open_Map_Iterator *it) {
  Open_Map_Header *header = open_map_header(it->_map);
  void *slot = (byte *)it->_map + it->_index * header->slot_size;
  it->key = open_map_get_key_ptr(slot);
  it->value = open_map_get_value_ptr(slot, header);

  it->_index += 1;
  while (it->_index < header->cap) {
    void *next_slot = (byte *)it->_map + it->_index * header->slot_size;

    if (*(Open_Map_Slot_State *)next_slot == Open_Map_Slot_State_Occupied) {
      break;
    }
    it->_index += 1;
  }

  return it->value;
}

bool32 open_map_remove_raw(Open_Map map, void *key) {
  Open_Map_Header *header = open_map_header(map);

  u64 hash = header->hash(key, header->key_size);
  usize index = (usize)(hash % header->cap);

  usize starting_index = index;

  do {
    void *slot = (byte *)map + (index * header->slot_size);

    Open_Map_Slot_State *slot_state = (Open_Map_Slot_State *)slot;

    if (*slot_state == Open_Map_Slot_State_Empty) {
      return false;
    }

    if (*slot_state == Open_Map_Slot_State_Occupied &&
        header->key_eq(key, open_map_get_key_ptr(slot))) {
      *slot_state = Open_Map_Slot_State_Tombstone;
      header->len -= 1;
      return true;
    }

    index = (index + 1) % header->cap;

  } while (index != starting_index);

  return false;
}

//////////////////////////////
// Provided defaults
//////////////////////////////
u64 open_map_u32_hash(void *key, usize key_size) {
  (void)key_size;
  return hash_fnv1a(key, sizeof(u32));
}

bool32 open_map_u32_eq(void *k1, void *k2) {
  u32 *a = (u32 *)k1;
  u32 *b = (u32 *)k2;
  return (*a) == (*b);
}