#include "view.h"

#include "core/allocator.h"
#include "core/array.h"
#include "core/imgui.h"
#include "core/math.h"
#include "core/platform.h"
#include "core/strings.h"
#include "core/types.h"
#include "db.h"
#include "document.h"
#include "font.h"
#include "render2d.h"

#include <assert.h>
#include <math.h>
#include <string.h>

// static Window_Data tmp_code_editor = {0};

#define VIEW_MODEL_FRAME_ARENA_SIZE (MEGABYTE * 64)
#define VIEW_MODEL_EVENT_CAP 256

#define WINDOW_MANAGER_CAP 32
#define WINDOW_SLOT_BACKING_INDEX_MASK 0x7FFFFFFFu
#define WINDOW_SLOT_FREE_BIT_MASK 0x80000000u
#define WINDOW_TITLE_BAR_ID 0x00001

typedef enum Window_Error {
  Window_Error_None,
  Window_Error_Capacity_Reached,
  Window_Error_Failed_To_Open,
  Window_Error_Invalid_Handle,
} Window_Error;

typedef u32 Window_Events;
typedef enum Window_Event {
  Window_Event_Gained_Focus = 1 << 0,
  Window_Event_Lost_Focus = 1 << 1,
  Window_Event_Began_Drag = 1 << 2,
} Window_Event;

typedef struct Code_Editor {
  Text_Screen screen;

  bool32 capture_input;
  f32 caret_blink_time;
  Document_Position cursor;
  usize horizontal_scroll;
  usize vertical_scroll;

  // NOTE(nico): This will need to move to an array once I want to support
  // multi-tabs
  Document document;
} Code_Editor;

typedef enum Window_Flag : u32 {
  Window_Flag_Focused = 1 << 0,
  Window_Flag_Dragged = 1 << 1,
} Window_Flag;

typedef u32 Window_Flags;

// NOTE(nico): FUCK X11 AGAIN
typedef struct Window_Data {
  u32 backing_index;
  u32 slot_index;

  String title;
  Window_Flags flags;
  Window_Kind kind;

  Vec2 position;
  f32 width;
  f32 height;

  union {
    Code_Editor editor;
  };
} Window_Data;

typedef struct Window_Slot {
  u32 generation;
  u32 packed;
} Window_Slot;

typedef Result(Window_Handle, Window_Error) Window_Open_Result;
typedef Option(Window_Handle) Window_Handle_Option;
typedef Option(Window_Data *) Window_Ptr_Option;

typedef struct Window_Manager {
  Allocator allocator;

  Window_Data windows[WINDOW_MANAGER_CAP];
  Window_Slot table[WINDOW_MANAGER_CAP];
  usize count;
  usize cap;

  Window_Handle_Option focused_window;
  Window_Handle_Option dragged_window;
} Window_Manager;

static Window_Error
init_window_manager(Window_Manager *manager, Allocator allocator);
static void destroy_window_manager(Window_Manager *manager);
static Window_Open_Result
window_manager_open_window(Window_Manager *manager, Window_Open_Info *info);
static Window_Error
window_manager_close_window(Window_Manager *manager, Window_Handle handle);
static void window_manager_close_all_windows(Window_Manager *manager);
static Window_Handle_Option
window_manager_get_window_handle(Window_Manager *manager, Window_Data *window);
static Window_Ptr_Option
window_manager_get_window_ptr(Window_Manager *manager, Window_Handle handle);
static void window_manager_view(Window_Manager *manager);

static void init_window(Window_Data *window, Allocator allocator);
static void destroy_window(Window_Data *window);
static Window_Events window_view(Window_Data *window);

typedef struct View_Model {
  Allocator global_allocator;
  Allocator frame_allocator;
  Arena_Data frame_arena;

  Renderer2D *renderer; // Borrowed from the game state
  Element_Context ctx;
  Window_Manager window_manager;

  String_Builder builder;
  char builder_buf[512];

  View_Inbound_Event mailbox[VIEW_MODEL_EVENT_CAP];
  usize mailbox_len;
} View_Model;

static View_Model g_model = {0};

static void process_element_render_commands(
    Renderer2D *renderer, Element_Render_Command_Buffer cmds
);

// NOTE(nico): I want to completely isolate the UI from the simulation. The only
// two-way door in through the event pump. It will be like a mailbox system
// where sender and receiver can response. Inbound and Outbound are separated so
// that there is never any confusion when draining and it can be sequenced
// cleanly (no need to thread that part)

static Element_Dimensions
measure_texture_wrapper(Element_Font el_font, String text) {
  Database_Font_Query font_query =
      database_get_font_atlas_entry((Font_ID)el_font.user_index, el_font.size);
  if (!font_query.ok) {
    return (Element_Dimensions){0};
  }

  Vec2 dim = font_atlas_entry_measure_text(font_query.value, text);
  return (Element_Dimensions){.width = dim.x, .height = dim.y};
}

void init_view(Renderer2D *renderer) {
  g_model.global_allocator = heap_allocator();

  Allocation_Result frame_alloc = g_model.global_allocator.alloc(
      g_model.global_allocator, VIEW_MODEL_FRAME_ARENA_SIZE
  );
  assert(frame_alloc.err == Allocation_Error_None);

  init_arena(
      &g_model.frame_arena, frame_alloc.allocation, VIEW_MODEL_FRAME_ARENA_SIZE
  );
  g_model.frame_allocator = arena_allocator(&g_model.frame_arena);

  g_model.renderer = renderer;
  init_element_context(
      &g_model.ctx,
      &(Element_Context_Create_Info){
        .measure_text_proc = measure_texture_wrapper
      },
      g_model.global_allocator
  );

  g_model.builder = make_builder_from_buf(g_model.builder_buf, 512);
  init_window_manager(&g_model.window_manager, g_model.global_allocator);
}

void destroy_view(void) {
  destroy_window_manager(&g_model.window_manager);
  destroy_element_context(&g_model.ctx);
  g_model.global_allocator.free(
      g_model.global_allocator, g_model.frame_arena.buf
  );
}

void update_view(void) {
  g_model.frame_allocator.free_all(g_model.frame_allocator);

  // Drain the mailbox
  for (usize i = 0; i < g_model.mailbox_len; i += 1) {
    View_Inbound_Event *event = &g_model.mailbox[i];

    switch (event->kind) {
    case View_Inbound_Event_Open_Window:
      unwrap(window_manager_open_window(
          &g_model.window_manager, &event->open_window
      ));
      break;
    case View_Inbound_Event_Close_Window:
      window_manager_close_window(
          &g_model.window_manager, event->close_window.handle
      );
      break;
    case View_Inbound_Event_Close_All_Windows:
      window_manager_close_all_windows(&g_model.window_manager);
      break;
    }
  }
  g_model.mailbox_len = 0;
}

void render_view(f32 render_w, f32 render_h) {
  set_screen_state(
      &g_model.ctx,
      (Element_Dimensions){
        render_w,
        render_h,
      }
  );
  set_pointer_state(
      &g_model.ctx,
      app_mouse_position(),
      (bool32)app_mouse_pressed(Mouse_Button_Left),
      (bool32)app_mouse_pressed(Mouse_Button_Right)
  );
  set_delta_time(&g_model.ctx, app_get_elapsed_time());

  begin_render_2d(g_model.renderer, render_w, render_h);
  begin_ui(&g_model.ctx);

  window_manager_view(&g_model.window_manager);

  Element_Render_Command_Buffer cmds = end_ui(&g_model.ctx);
  process_element_render_commands(g_model.renderer, cmds);
  end_render_2d(g_model.renderer);
}

bool32 push_view_inbound_event(View_Inbound_Event event) {
  if (g_model.mailbox_len >= VIEW_MODEL_EVENT_CAP) {
    return false;
  }

  g_model.mailbox[g_model.mailbox_len++] = event;
  return true;
}

//////////////////////////////
// Text screen
//////////////////////////////
static Text_Screen_Error init_text_screen(
    Text_Screen *screen,
    f32 physical_width,
    f32 physical_height,
    f32 cell_width,
    f32 cell_height,
    f32 font_size,
    Allocator allocator
) {
  screen->physical_width = physical_width;
  screen->physical_height = physical_height;
  screen->cell_width = cell_width;
  screen->cell_height = cell_height;
  screen->font_size = font_size;
  screen->width = (usize)(physical_width / cell_width);
  screen->height = (usize)(physical_height / cell_height);
  screen->cells =
      make_array(screen->cells, screen->width * screen->height, allocator);

  for (usize i = 0; i < screen->cells.len; i += 1) {
    array_set(screen->cells, i, ((Text_Cell){0}));
  }

  if (screen->cells.items == nullptr) {
    return Text_Screen_Error_Failed_To_Initialize;
  }

  return Text_Screen_Error_None;
}

static void destroy_text_screen(Text_Screen *screen) {
  delete_array(screen->cells);
}

static usize text_screen_coord_to_index(Text_Screen *screen, Vec2Int coord) {
  return (usize)coord.y * screen->width + (usize)coord.x;
}

static void text_screen_clear(Text_Screen *screen) {
  memset(
      screen->cells.items, 0, screen->width * screen->height * sizeof(Text_Cell)
  );
}

static void text_screen_move_cursor_down(Text_Screen *screen) {
  screen->cursor.x = 0;
  if (screen->cursor.y + 1 >= (i32)screen->height) {
    return;
  }
  screen->cursor.y += 1;
}

static void text_screen_move_cursor_up(Text_Screen *screen) {
  screen->cursor =
      vec2int((i32)screen->width - 1, max_i32(screen->cursor.y - 1, 0));
}

static void text_screen_move_cursor_forward(Text_Screen *screen) {
  screen->cursor.x += 1;
  if (screen->cursor.x >= (i32)screen->width) {
    text_screen_move_cursor_down(screen);
  }
}

static void text_screen_move_cursor_backward(Text_Screen *screen) {
  if (screen->cursor.x == 0) {
    if (screen->cursor.y > 0) {
      text_screen_move_cursor_up(screen);
    }
    return;
  }
  screen->cursor.x -= 1;
}

static bool32 text_screen_set_cell_background_color(
    Text_Screen *screen, Vec2Int coord, Theme_Color color
) {
  if (coord.x < 0 || coord.x >= (i32)screen->width || coord.y < 0 ||
      coord.y >= (i32)screen->height) {
    return false;
  }

  usize index = text_screen_coord_to_index(screen, coord);
  screen->cells.items[index].bg = color;

  return true;
}

static void
text_screen_write_ascii_char(Text_Screen *screen, char c, Theme_Color fg) {
  if (c == '\n') {
    text_screen_move_cursor_down(screen);
  }

  usize index = text_screen_coord_to_index(screen, screen->cursor);
  array_set(
      screen->cells,
      index,
      ((Text_Cell){
        .present = true,
        .content = (utf8_char)c,
        .fg = fg,
      })
  );
  text_screen_move_cursor_forward(screen);
}

static void text_screen_delete_at_cursor(Text_Screen *screen, usize len) {
  usize end = text_screen_coord_to_index(screen, screen->cursor);
  usize start = end >= len ? end - len : 0;

  memset(screen->cells.items + start, 0, (end - start) * sizeof(Text_Cell));
  for (usize i = 0; i < len; i += 1) {
    text_screen_move_cursor_backward(screen);
  }
}

static void text_screen_render(
    Text_Screen *screen, Renderer2D *renderer, Vec2 origin, Theme theme
) {
  f32 total_w = (f32)screen->width * screen->cell_width;
  f32 total_h = (f32)screen->height * screen->cell_height;

  f32 off_x = roundf((screen->physical_width - total_w) * 0.5f);
  f32 off_y = roundf((screen->physical_height - total_h) * 0.5f);

  for (usize y = 0; y < screen->height; y += 1) {
    for (usize x = 0; x < screen->width; x += 1) {
      Vec2Int coord = vec2int((i32)x, (i32)y);
      usize index = text_screen_coord_to_index(screen, coord);

      f32 physical_x = (f32)x * screen->cell_width + off_x + origin.x;
      f32 physical_y = (f32)y * screen->cell_height + off_y + origin.y;

      Text_Cell *cell = array_get_ptr(screen->cells, index);
      if (cell->bg != Theme_Color_Transparent) {
        draw_rect(
            renderer,
            (Rectangle){
              .x = physical_x,
              .y = physical_y,
              .width = screen->cell_width,
              .height = screen->cell_height
            },
            theme.colors[cell->bg]
        );
      }

      if (!cell->present) {
        continue;
      }

      draw_char(
          renderer,
          (char)cell->content,
          vec2(physical_x, physical_y),
          screen->font_size,
          theme.colors[cell->fg]
      );
    }
  }
}

//////////////////////////////////////
// Window_Data manager implementation
//////////////////////////////////////
static Window_Error
init_window_manager(Window_Manager *manager, Allocator allocator) {
  manager->allocator = allocator;
  manager->cap = WINDOW_MANAGER_CAP;

  for (usize i = 0; i < manager->cap; i += 1) {
    manager->table[i] = (Window_Slot){
      .generation = 1,
      .packed = WINDOW_SLOT_FREE_BIT_MASK,
    };
  }

  return Window_Error_None;
}

static void destroy_window_manager(Window_Manager *manager) {
  for (usize i = 0; i < manager->count; i += 1) {
    destroy_window(&manager->windows[i]);
    delete_string(manager->windows[i].title, manager->allocator);
  }
}

static Window_Open_Result
window_manager_open_window(Window_Manager *manager, Window_Open_Info *info) {
  // NOTE(nico): Linear search is fine for this little items
  if (manager->count >= manager->cap) {
    return err(Window_Open_Result, Window_Error_Capacity_Reached);
  }

  u32 slot_index = 0;
  bool32 slot_found = false;
  for (usize i = 0; i < manager->cap; i += 1) {
    if (manager->table[i].packed & WINDOW_SLOT_FREE_BIT_MASK) {
      slot_index = (u32)i;
      slot_found = true;
      break;
    }
  }

  if (!slot_found) {
    return err(Window_Open_Result, Window_Error_Capacity_Reached);
  }

  u32 backing_index = (u32)manager->count;
  u32 generation = manager->table[slot_index].generation;
  String title_copy = or_return(
      string_clone(info->title, manager->allocator),
      err(Window_Open_Result, Window_Error_Failed_To_Open)
  );

  manager->table[slot_index] = (Window_Slot){
    .generation = generation,
    .packed = backing_index & WINDOW_SLOT_BACKING_INDEX_MASK,
  };

  manager->windows[backing_index] = (Window_Data){
    .backing_index = backing_index,
    .slot_index = slot_index,
    .title = title_copy,
    .kind = info->kind,
    .position = info->position,
    .width = info->width,
    .height = info->height,
  };
  manager->count += 1;

  init_window(&manager->windows[backing_index], manager->allocator);

  return ok(
      Window_Open_Result,
      ((Window_Handle){.generation = generation, .id = slot_index})
  );
}

static Window_Error
window_manager_close_window(Window_Manager *manager, Window_Handle handle) {
  if (handle.id >= manager->cap ||
      manager->table[handle.id].generation != handle.generation) {
    return Window_Error_Invalid_Handle;
  }

  usize last_window_index = manager->count - 1;
  usize last_slot_index = (usize)manager->windows[last_window_index].slot_index;
  usize removed_window_index = (usize)(manager->table[handle.id].packed &
                                       WINDOW_SLOT_BACKING_INDEX_MASK);

  if (manager->windows[removed_window_index].slot_index != handle.id) {
    return Window_Error_Invalid_Handle;
  }

  manager->table[handle.id] = (Window_Slot){
    .generation = manager->table[handle.id].generation + 1,
    .packed = WINDOW_SLOT_FREE_BIT_MASK,
  };
  delete_string(
      manager->windows[removed_window_index].title, manager->allocator
  );
  destroy_window(&manager->windows[removed_window_index]);

  if (removed_window_index != last_window_index) {
    manager->windows[removed_window_index] =
        manager->windows[last_window_index];
    manager->windows[removed_window_index].backing_index =
        (u32)removed_window_index;
    manager->table[last_slot_index].packed =
        (u32)removed_window_index & WINDOW_SLOT_BACKING_INDEX_MASK;
  }

  manager->count -= 1;

  return Window_Error_None;
}

static void window_manager_close_all_windows(Window_Manager *manager) {
  for (usize i = 0; i < manager->count; i += 1) {
    delete_string(manager->windows[i].title, manager->allocator);
    destroy_window(&manager->windows[i]);
  }

  for (usize i = 0; i < manager->cap; i += 1) {

    manager->table[i] = (Window_Slot){
      .generation = 1,
      .packed = WINDOW_SLOT_FREE_BIT_MASK,
    };
  }

  manager->count = 0;
}

static Window_Handle_Option
window_manager_get_window_handle(Window_Manager *manager, Window_Data *window) {
  return some(
      Window_Handle_Option,
      ((Window_Handle){
        .generation = manager->table[window->slot_index].generation,
        .id = window->slot_index
      })
  );
}

static Window_Ptr_Option
window_manager_get_window_ptr(Window_Manager *manager, Window_Handle handle) {
  if (handle.id >= manager->cap ||
      manager->table[handle.id].generation != handle.generation) {
    return none(Window_Ptr_Option);
  }

  u32 backing_index =
      manager->table[handle.id].packed & WINDOW_SLOT_BACKING_INDEX_MASK;
  return some(Window_Ptr_Option, &manager->windows[backing_index]);
}

static void window_manager_view(Window_Manager *manager) {
  if (manager->dragged_window.some) {
    Window_Ptr_Option window_opt =
        window_manager_get_window_ptr(manager, manager->dragged_window.value);
    assert(window_opt.some);

    Window_Data *window = window_opt.value;
    if (app_mouse_just_released(Mouse_Button_Left)) {
      window->flags &= ~Window_Flag_Dragged;
      manager->dragged_window = none(Window_Handle_Option);
    } else {
      window->position = vec2_sub(window->position, app_mouse_delta());
    }
  }

  // TODO(nico): Might need some compositing shit
  for (usize i = 0; i < manager->count; i += 1) {
    Window_Data *window = &manager->windows[i];
    Window_Slot slot = manager->table[window->slot_index];

    push_element_id_seed(
        ((u64)window->slot_index << 32) | (u64)slot.generation
    );
    Window_Events events = window_view(&manager->windows[i]);
    pop_element_id_seed();

    assert(!(
        events & Window_Event_Gained_Focus && events & Window_Event_Lost_Focus
    ));

    if (events & Window_Event_Gained_Focus) {
      window->flags |= Window_Flag_Focused;
      manager->focused_window =
          window_manager_get_window_handle(manager, window);
    }

    if (events & Window_Event_Lost_Focus) {
      window->flags &= ~Window_Flag_Focused;
      manager->focused_window = none(Window_Handle_Option);
    }

    if (events & Window_Event_Began_Drag) {
      window->flags |= Window_Event_Began_Drag;
      manager->dragged_window =
          window_manager_get_window_handle(manager, window);
    }
  }
}

//////////////////////////////////////
// Window & Programs implementation
//////////////////////////////////////
static void init_window(Window_Data *window, Allocator allocator) {
  switch (window->kind) {
  case Window_Kind_Code_Editor: {
    Code_Editor *editor = &window->editor;

    f32 font_size = 18.f;

    Database_Font_Query font_query =
        database_get_font_atlas_entry(Font_ID_IBMPlex_Mono, font_size);

    f32 cell_width = font_size;
    f32 cell_height = font_size;
    if (font_query.ok) {
      cell_width = roundf(font_query.value->max_advance);
      cell_height = font_query.value->line_height;
    }

    editor->document = unwrap(
        make_document(&(Document_Create_Info){.initial_cap = 512}, allocator)
    );

    init_text_screen(
        &editor->screen,
        window->width,
        window->height,
        cell_width,
        cell_height,
        font_size,
        allocator
    );

    // NOTE(nico): just to make the compiler shut up about unused functions for
    // now
    text_screen_write_ascii_char(&editor->screen, ' ', Theme_Color_Foreground);
    text_screen_delete_at_cursor(&editor->screen, 1);
  } break;
  }
}

static void destroy_window(Window_Data *window) {
  switch (window->kind) {
  case Window_Kind_Code_Editor:
    destroy_document(window->editor.document);
    destroy_text_screen(&window->editor.screen);
    break;
  }
}

static void code_editor_cells_view(Rectangle rect, rawptr data) {
  Window_Data *window = (Window_Data *)data;
  Code_Editor *editor = &window->editor;
  text_screen_clear(&editor->screen);

  editor->cursor = unwrap(document_query_position_from_logical_offset(
      &editor->document, editor->document.gap_start
  ));

  if (editor->cursor.line < editor->vertical_scroll) {
    editor->vertical_scroll = editor->cursor.line;
  } else if (
      editor->cursor.line >= editor->vertical_scroll + editor->screen.height
  ) {
    editor->vertical_scroll = editor->cursor.line - editor->screen.height + 1;
  }

  if (editor->cursor.col < editor->horizontal_scroll) {
    editor->horizontal_scroll = editor->cursor.col;
  } else if (
      editor->cursor.col >= editor->horizontal_scroll + editor->screen.width
  ) {
    editor->horizontal_scroll = editor->cursor.col - editor->screen.width + 1;
  }

  Vec2Int caret = vec2int(
      (i32)(editor->cursor.col - editor->horizontal_scroll),
      (i32)(editor->cursor.line - editor->vertical_scroll)
  );

  static f32 caret_blink_duration = 1.f;

  editor->caret_blink_time += app_get_elapsed_time();
  if (editor->caret_blink_time >= caret_blink_duration) {
    editor->caret_blink_time -= caret_blink_duration;
  }

  text_screen_set_cell_background_color(
      &editor->screen,
      caret,
      editor->caret_blink_time < caret_blink_duration * 0.5f
          ? Theme_Color_Transparent
          : Theme_Color_Foreground
  );

  usize start = editor->vertical_scroll;
  usize end =
      min_usize(start + editor->screen.height, editor->document.lines.len);

  for (usize i = start; i < end; i += 1) {
    Document_Line_Content content =
        unwrap(document_query_line_content(&editor->document, i));

    i32 screen_y = (i32)(i - start);
    i32 screen_x = 0;

    usize rem_scroll = editor->horizontal_scroll;
    for (usize j = rem_scroll; j < content.head.len; j += 1) {
      if ((usize)screen_x >= editor->screen.width) {
        break;
      }

      Vec2Int coord = vec2int(screen_x, screen_y);
      usize index = text_screen_coord_to_index(&editor->screen, coord);

      Text_Cell *cell = &editor->screen.cells.items[index];
      cell->present = true;
      cell->content = (utf8_char)content.head.data[j];
      cell->fg = Theme_Color_Foreground;

      screen_x += 1;
    }

    rem_scroll = editor->horizontal_scroll > content.head.len
                     ? editor->horizontal_scroll - content.head.len
                     : 0;
    for (usize j = rem_scroll; j < content.tail.len; j += 1) {
      if ((usize)screen_x >= editor->screen.width) {
        break;
      }

      Vec2Int coord = vec2int(screen_x, screen_y);
      usize index = text_screen_coord_to_index(&editor->screen, coord);

      Text_Cell *cell = &editor->screen.cells.items[index];
      cell->present = true;
      cell->content = (utf8_char)content.tail.data[j];
      cell->fg = Theme_Color_Foreground;

      screen_x += 1;
    }
  }

  text_screen_render(
      &window->editor.screen,
      g_model.renderer,
      vec2(rect.x, rect.y),
      (Theme){
        .colors = {
          [Theme_Color_Background] = ISW_BG1,
          [Theme_Color_Foreground] = ISW_CREAM_LIGHT0,
          [Theme_Color_Muted] = ISW_CREAM_SHADOW,
          [Theme_Color_Accent] = ISW_RED,
        }
      }
  );
}

static void code_editor_view(Window_Data *window) {
  Code_Editor *editor = &window->editor;

  if (editor->capture_input) {
    Text_Array chars = app_chars_pressed();
    for (usize i = 0; i < chars.len; i += 1) {
      document_write_char(&editor->document, (char)chars.items[i]);
    }

    if (app_key_pressed(Keyboard_Key_Backspace)) {
      document_delete_chars(
          &editor->document, app_key_press_count(Keyboard_Key_Backspace)
      );
    }

    if (app_key_pressed(Keyboard_Key_Enter)) {
      usize count = app_key_press_count(Keyboard_Key_Enter);
      for (usize i = 0; i < count; i += 1) {
        document_write_char(&editor->document, '\n');
      }
    }

    if (app_key_pressed(Keyboard_Key_Left)) {
      usize count = app_key_press_count(Keyboard_Key_Left);
      for (usize i = 0; i < count; i += 1) {
        if (editor->document.gap_start == 0) {
          break;
        }

        assert(
            document_move_gap(
                &editor->document,
                editor->document.gap_start > 0 ? editor->document.gap_start - 1
                                               : 0
            ) == Document_Error_None
        );
      }
    }

    if (app_key_pressed(Keyboard_Key_Right)) {
      usize count = app_key_press_count(Keyboard_Key_Right);
      for (usize i = 0; i < count; i += 1) {
        if (editor->document.gap_start >=
            document_text_len(&editor->document)) {
          break;
        }

        assert(
            document_move_gap(
                &editor->document, editor->document.gap_start + 1
            ) == Document_Error_None
        );
      }
    }
  }

  element_container((&(Element_Create_Info){
    .layout = Element_Layout_Kind_Column,
    .sizing = {.width = element_sizing_fit(), .height = element_sizing_fit()},
    .style = {
      .base.linears.border = 1.f,
      .base.colors.background = ISW_BG1,
      .base.constraints.padding = element_constraint(4, 4, 4, 4),
      .base.variable_colors.border = {
        .is_cardinal = true,
        .cardinal = {
          [Cardinality_Top] = ISW_BG0,
          [Cardinality_Left] = ISW_BG0,
          [Cardinality_Bottom] = ISW_CREAM_LIGHT0,
          [Cardinality_Right] = ISW_CREAM_LIGHT0,
        },
      }
    },
  })) {
    Element_Client_Info element = get_current_element();
    Vec2 m_pos = app_mouse_position();

    if (app_mouse_just_pressed(Mouse_Button_Left)) {
      window->editor.capture_input =
          rect_point_in(element.computed_rect, m_pos.x, m_pos.y);
    }

    element_custom((&(Element_Create_Info){
      .sizing =
          {
            .width = element_sizing_fixed(window->width),
            .height = element_sizing_fixed(window->height),
          },
      .content_proc = code_editor_cells_view,
      .content_data = window,
    }));
  }

  element_container((&(Element_Create_Info){
    .layout = Element_Layout_Kind_Row,
    .sizing = {.width = element_sizing_grow(), .height = element_sizing_fit()},
    .alignment = {.horizontal = Element_Alignment_Space_Between},
    .style = {
      .base.linears.border = 1.f,
      .base.linears.child_gap = 4.f,
      .base.colors.background = ISW_CREAM_DARK,
      .base.constraints.padding = element_constraint(3, 3, 3, 3),
      .base.variable_colors.border = {
        .is_cardinal = true,
        .cardinal = {
          [Cardinality_Top] = ISW_BG0,
          [Cardinality_Left] = ISW_BG0,
          [Cardinality_Bottom] = ISW_CREAM_LIGHT0,
          [Cardinality_Right] = ISW_CREAM_LIGHT0,
        },
      }
    },
  })) {
    element_label((&(Element_Create_Info){
      .text = from_c_str("compilation successful.."),
      .style = {
        .base.linears.font_size = 18.f,
        .base.colors.text = ISW_BG0,
        .font_index = Font_ID_IBMPlex_Mono,
      },
    }));

    builder_reset(&g_model.builder);
    builder_write(
        &g_model.builder,
        "ln %d, col %d",
        (i32)editor->cursor.line,
        (i32)editor->cursor.col
    );
    String pos =
        builder_clone_string(&g_model.builder, g_model.frame_allocator);

    element_label((&(Element_Create_Info){
      .text = pos,
      .style = {
        .base.linears.font_size = 18.f,
        .base.colors.text = ISW_BG0,
        .font_index = Font_ID_IBMPlex_Mono,
      },
    }));
  }
}

static Window_Events window_view(Window_Data *window) {
  Window_Events events = 0;

  element_container((&(Element_Create_Info){
    .layout = Element_Layout_Kind_Column,
    .sizing = {.width = element_sizing_fit(), .height = element_sizing_fit()},
    .position =
        {.kind = Element_Position_Absolute, .pixel_offset = window->position},
    .style = {
      .base.linears.border = 1.f,
      .base.linears.child_gap = 4.f,
      .base.colors.background = ISW_CREAM,
      .base.constraints.padding = element_constraint(3, 3, 3, 3),
      .base.variable_colors.border = {
        .is_cardinal = true,
        .cardinal = {
          [Cardinality_Top] = ISW_CREAM_LIGHT0,
          [Cardinality_Left] = ISW_CREAM_LIGHT0,
          [Cardinality_Bottom] = ISW_BG0,
          [Cardinality_Right] = ISW_BG0,
        },
      }
    },
  })) {
    Element_Client_Info element = get_current_element();
    Vec2 m_pos = app_mouse_position();

    if (app_mouse_just_pressed(Mouse_Button_Left)) {
      if (rect_point_in(element.computed_rect, m_pos.x, m_pos.y)) {
        events |= Window_Event_Gained_Focus;
      } else if (window->flags & Window_Flag_Focused) {
        events |= Window_Event_Lost_Focus;
      }
    }

    element_container((&(Element_Create_Info){
      .id = {.some = true, .value = WINDOW_TITLE_BAR_ID},
      .layout = Element_Layout_Kind_Row,
      .sizing =
          {
            .width = element_sizing_grow(),
            .height = element_sizing_fit(),
          },
      .alignment =
          {
            .horizontal = Element_Alignment_Space_Between,
            .vertical = Element_Alignment_Center,
          },
      .style = {
        .base.colors.background =
            window->flags & Window_Flag_Focused ? ISW_ORANGE : ISW_CREAM_SHADOW,
        .base.constraints.padding = element_constraint(4, 4, 4, 4),
      },
    })) {
      Element_Client_Info title_element = get_current_element();

      if (app_mouse_just_pressed(Mouse_Button_Left)) {
        if (rect_point_in(title_element.computed_rect, m_pos.x, m_pos.y)) {
          events |= Window_Event_Began_Drag;
        }
      }

      element_label((&(Element_Create_Info){
        .text = window->title,
        .style = {
          .base.linears.font_size = 18.f,
          .base.colors.text = ISW_CREAM_LIGHT0,
          .font_index = Font_ID_IBMPlex_Mono,
        },
      }));
    }

    switch (window->kind) {
    case Window_Kind_Code_Editor:
      code_editor_view(window);
    }
  }

  return events;
}

static void draw_rect_bevel_outline(
    Renderer2D *renderer,
    Rectangle rect,
    f32 thickness,
    Element_Variable_Color color
) {
  if (!color.is_cardinal) {
    return;
  }

  f32 t = min_f32(thickness, min_f32(rect.width, rect.height) * 0.5f);
  if (t <= 0.f) {
    return;
  }

  draw_rect(
      renderer,
      (Rectangle){
        .x = rect.x,
        .y = rect.y,
        .width = rect.width - t,
        .height = t,
      },
      color.cardinal[Cardinality_Top]
  );
  draw_rect(
      renderer,
      (Rectangle){
        .x = rect.x + rect.width - t,
        .y = rect.y,
        .width = t,
        .height = rect.height - t,
      },
      color.cardinal[Cardinality_Right]
  );
  draw_rect(
      renderer,
      (Rectangle){
        .x = rect.x,
        .y = rect.y + rect.height - t,
        .width = rect.width,
        .height = t,
      },
      color.cardinal[Cardinality_Bottom]
  );
  draw_rect(
      renderer,
      (Rectangle){
        .x = rect.x,
        .y = rect.y + t,
        .width = t,
        .height = rect.height - t * 2.f,
      },
      color.cardinal[Cardinality_Left]
  );
}

static void process_element_render_commands(
    Renderer2D *renderer, Element_Render_Command_Buffer cmds
) {
  for (usize i = 0; i < cmds.len; i += 1) {
    Element_Render_Command cmd = cmds.items[i];

    switch (cmd.kind) {
    case Element_Render_Command_Rectangle: {
      // NOTE(nico): hard crash for now until the feature is implemented in the
      // 2d renderer
      assert(cmd.rectangle.radius == 0.f);
      draw_rect(renderer, cmd.rectangle.rect, cmd.rectangle.color);

      if (cmd.rectangle.border > 0.f) {
        if (cmd.rectangle.border_color.is_cardinal) {
          draw_rect_bevel_outline(
              renderer,
              cmd.rectangle.rect,
              cmd.rectangle.border,
              cmd.rectangle.border_color
          );
        } else {
          draw_rect_outline(
              renderer,
              cmd.rectangle.rect,
              cmd.rectangle.border,
              cmd.rectangle.border_color.uniform
          );
        }
      }
    } break;
    case Element_Render_Command_Line: {
      assert(false);
    } break;
    case Element_Render_Command_Text: {
      // NOTE(nico): hard crash for now. Still no multi-font supported in a
      // single pass
      assert(cmd.text.font.user_index == Font_ID_IBMPlex_Mono);

      draw_text(
          renderer,
          cmd.text.chars,
          cmd.text.origin,
          cmd.text.font.size,
          cmd.text.color
      );
    } break;
    case Element_Render_Command_Image: {
      // NOTE(nico): The whole render situation is super annoying. It is too
      // rigid for now. I need to solve the issues in it to draw from arbitrary
      // texture
      assert(false);
    } break;
    case Element_Render_Command_Custom: {
      cmd.custom.callback(cmd.custom.rect, cmd.custom.data);
    } break;
    }
  }
}
