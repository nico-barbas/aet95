#ifndef UI_H
#define UI_H

#include "core/allocator.h"
#include "core/imgui.h"
#include "core/math.h"
#include "core/types.h"
#include "game.h"
#include "render2d.h"

typedef enum Text_Screen_Error {
  Text_Screen_Error_None,
  Text_Screen_Error_Failed_To_Initialize,
} Text_Screen_Error;

typedef enum Theme_Color : byte {
  Theme_Color_Background,
  Theme_Color_Foreground,
  Theme_Color_Muted,
  Theme_Color_Accent,
  Theme_Color_MAX,
} Theme_Color;

typedef struct Theme {
  Color colors[Theme_Color_MAX];
} Theme;

typedef struct Text_Cell {
  bool8 present;
  utf8_char content;
  Theme_Color fg;
  Theme_Color bg;
} Text_Cell;

// NOTE(nico): doesn't handle resize. Which might be important later on
typedef struct Text_Screen {
  Allocator allocator;

  f32 physical_width;
  f32 physical_height;
  f32 cell_width;
  f32 cell_height;
  f32 font_size;
  usize width;
  usize height;
  Array(Text_Cell) cells;

  // Runtime
  Vec2Int cursor;
} Text_Screen;

Text_Screen_Error init_text_screen(
    Text_Screen *screen,
    f32 physical_width,
    f32 physical_height,
    f32 cell_width,
    f32 cell_height,
    f32 font_size,
    Allocator allocator
);
void destroy_text_screen(Text_Screen *screen);

void text_screen_write_ascii_char(Text_Screen *screen, char c, Theme_Color fg);
void text_screen_write_ascii_string(
    Text_Screen *screen, String text, Theme_Color fg
);
void text_screen_delete_at_cursor(Text_Screen *screen, usize len);
void text_screen_render(
    Text_Screen *screen, Renderer2D *renderer, Vec2 origin, Theme theme
);

typedef struct Text_Editor {
  Renderer2D *tmp_renderer;
  Text_Screen screen;

  bool32 focused;

  char line_buffer[512];
  usize line_len;
} Text_Editor;

void tmp_init_game_view(Renderer2D *renderer, Allocator allocator);
void game_view(Game_State *model);

void render_game_view(Renderer2D *renderer, Element_Render_Command_Buffer cmds);

#endif