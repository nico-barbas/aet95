#include "ui.h"

#include "core/allocator.h"
#include "core/array.h"
#include "core/imgui.h"
#include "core/math.h"
#include "core/platform.h"
#include "core/strings.h"
#include "core/types.h"
#include "db.h"
#include "font.h"
#include "game.h"
#include "render.h"

#include <assert.h>
#include <math.h>
#include <string.h>

static Text_Editor tmp_text_editor = {0};

Text_Screen_Error init_text_screen(
    Text_Screen *screen,
    f32 physical_width,
    f32 physical_height,
    f32 cell_width,
    f32 cell_height,
    Allocator allocator
) {
  screen->physical_width = physical_width;
  screen->physical_height = physical_height;
  screen->cell_width = cell_width;
  screen->cell_height = cell_height;
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

void destroy_text_screen(Text_Screen *screen) {
  delete_array(screen->cells);
}

static usize text_screen_coord_to_index(Text_Screen *screen, Vec2Int coord) {
  return (usize)coord.y * screen->width + (usize)coord.x;
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

void text_screen_write_ascii_char(Text_Screen *screen, char c, Theme_Color fg) {
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

void text_screen_write_ascii_string(
    Text_Screen *screen, String text, Theme_Color fg
) {
  for (usize i = 0; i < text.len; i += 1) {
    text_screen_write_ascii_char(screen, text.data[i], fg);
  }
}

void text_screen_delete_at_cursor(Text_Screen *screen, usize len) {
  usize end = text_screen_coord_to_index(screen, screen->cursor);
  usize start = end >= len ? end - len : 0;

  memset(screen->cells.items + start, 0, (end - start) * sizeof(Text_Cell));
  for (usize i = 0; i < len; i += 1) {
    text_screen_move_cursor_backward(screen);
  }
}

void text_screen_render(
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
      if (vec2int_eq(screen->cursor, coord)) {
        draw_text(
            renderer,
            from_c_str("@"),
            vec2(physical_x, physical_y),
            BASIC_CLR_WHITE
        );
      }

      Text_Cell *cell = array_get_ptr(screen->cells, index);
      if (!cell->present) {
        continue;
      }

      draw_char(
          renderer,
          (char)cell->content,
          vec2(physical_x, physical_y),
          theme.colors[cell->fg]
      );
    }
  }
}

static void text_editor_cells_view(Rectangle rect, rawptr data) {
  Text_Editor *ed = (Text_Editor *)data;

  text_screen_render(
      &ed->screen,
      ed->tmp_renderer,
      vec2(rect.x, rect.y),
      (Theme){
        .colors = {
          [Theme_Color_Background] = GRUVBOX_CLR_BG0_HARD,
          [Theme_Color_Foreground] = GRUVBOX_CLR_FG0,
          [Theme_Color_Muted] = GRUVBOX_CLR_GRAY,
          [Theme_Color_Accent] = GRUVBOX_CLR_ORANGE
        }
      }
  );
}

static void text_editor_view(Text_Editor *ed) {
  // Font_Atlas *font = &_db.font_table[Font_ID_IBM_Default];

  if (ed->focused) {
    Text_Array chars = app_chars_pressed();
    for (usize i = 0; i < chars.len; i += 1) {
      // ed->line_buffer[ed->line_len++] = (char)chars.items[i];
      text_screen_write_ascii_char(
          &ed->screen, (char)chars.items[i], Theme_Color_Foreground
      );
    }

    if (app_key_pressed(Keyboard_Key_Backspace)) {
      text_screen_delete_at_cursor(
          &ed->screen, app_key_press_count(Keyboard_Key_Backspace)
      );
    }
  }

  element_container((&(Element_Create_Info){
    .layout = Element_Layout_Kind_Column,
    .sizing = {.width = element_sizing_fit(), .height = element_sizing_fit()},
    .style = {
      .base.colors.background = GRUVBOX_CLR_BG0_HARD,
      .base.constraints.padding = element_constraint(8, 8, 8, 8),
    },
  })) {
    Element_Client_Info element = get_current_element();
    Vec2 m_pos = app_mouse_position();

    if (app_mouse_just_pressed(Mouse_Button_Left)) {
      ed->focused = rect_point_in(element.computed_rect, m_pos.x, m_pos.y);
    }

    element_custom((&(Element_Create_Info){
      .sizing =
          {.width = element_sizing_fixed(400),
           .height = element_sizing_fixed(600)},
      .content_proc = text_editor_cells_view,
      .content_data = ed,
    }));
  }
}

void tmp_init_game_view(Renderer2D *renderer, Allocator allocator) {
  Font_Atlas *font = &_db.font_table[Font_ID_IBM_Default];

  tmp_text_editor.tmp_renderer = renderer;
  init_text_screen(
      &tmp_text_editor.screen,
      400,
      600,
      roundf(font->max_advance),
      font->line_height,
      allocator
  );
}

void game_view(Game_State *model) {
  (void)model;

  set_screen_state(
      &model->el_ctx,
      (Element_Dimensions){STARTUP_WINDOW_WIDTH, STARTUP_WINDOW_HEIGHT}
  );
  set_pointer_state(
      &model->el_ctx,
      app_mouse_position(),
      (bool32)app_mouse_pressed(Mouse_Button_Left),
      (bool32)app_mouse_pressed(Mouse_Button_Right)
  );
  set_delta_time(&model->el_ctx, app_get_elapsed_time());

  text_editor_view(&tmp_text_editor);

  // container((&(Element_Create_Info){
  //   .override_flags = Element_Flag_Ignore_Events,
  //   .layout = Element_Layout_Kind_Column,
  //   .sizing = {.width = element_sizing_fit(), .height =
  //   element_sizing_fit()}, .style = {
  //     .base =
  //         {
  //           .constraints.padding = element_constraint(40, 40, 40, 40),
  //           .colors.background = color(1, 0, 1, 1),
  //         },
  //     .variants =
  //         {
  //           [Element_Style_Variant_Enter] =
  //               {
  //                 .constraints.padding = element_constraint(0, 0, 0, 0),
  //               },
  //         },
  //     .variant_masks =
  //         {
  //           [Element_Style_Variant_Enter] =
  //           bitmask(Element_Property_Padding),
  //         },
  //     .transition_set = bitmask(Element_State_Enter, Element_State_Normal),
  //     .transitions = {
  //       [Element_State_Normal] = {
  //         .duration = 10.f,
  //       },
  //     },
  //   }
  // })) {
  //   label((&(Element_Create_Info){
  //     .text = from_c_str("hello world"),
  //     .style = {
  //       .base =
  //           {
  //             .linears.font_size = 18.f,
  //             .colors.text = color(1, 1, 1, 1),
  //           },
  //       .font_data = font,
  //     },
  //   }));
  // }
}

void render_game_view(
    Renderer2D *renderer, Element_Render_Command_Buffer cmds
) {
  for (usize i = 0; i < cmds.len; i += 1) {
    Element_Render_Command cmd = cmds.items[i];

    switch (cmd.kind) {
    case Element_Render_Command_Rectangle: {
      // NOTE(nico): hard crash for now until the feature is implemented in the
      // 2d renderer
      assert(cmd.rectangle.border == 0.f && cmd.rectangle.border == 0.f);

      draw_rect(renderer, cmd.rectangle.rect, cmd.rectangle.color);
    } break;
    case Element_Render_Command_Line: {
      assert(false);
    } break;
    case Element_Render_Command_Text: {
      draw_text(renderer, cmd.text.chars, cmd.text.origin, cmd.text.color);
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