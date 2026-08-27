#ifndef VIEW_H
#define VIEW_H

#include "core/allocator.h"
#include "core/math.h"
#include "core/types.h"
#include "render2d.h"

//////////////////////////////
// Text screen
//////////////////////////////
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

//////////////////////////////
// Windows
//////////////////////////////

typedef struct Window_Handle {
  u32 generation;
  u32 id;
} Window_Handle;

typedef enum Window_Kind {
  Window_Kind_Code_Editor,
} Window_Kind;

typedef struct Window_Open_Info {
  String title;
  Window_Kind kind;
  Vec2 position;
  f32 width;
  f32 height;
} Window_Open_Info;

//////////////////////////////
// Exposed API
//////////////////////////////
typedef struct View_Inbound_Event {
  enum {
    View_Inbound_Event_Open_Window,
    View_Inbound_Event_Close_Window,
    View_Inbound_Event_Close_All_Windows,
  } kind;
  union {
    Window_Open_Info open_window;
    struct {
      Window_Handle handle;
    } close_window;
  };
} View_Inbound_Event;

typedef struct View_Outbound_Event {
  enum {
    View_Outbound_Event_Capture_Input,
    View_Outbound_Event_Close_Game,
  } kind;
} View_Outbound_Event;

void init_view(Renderer2D *renderer);
void destroy_view(void);
void update_view(void);
void render_view(f32 render_w, f32 render_h);

bool32 push_view_inbound_event(View_Inbound_Event event);

////////////////////////////////
// Palette
////////////////////////////////

/* DARK STRUCTURE */
#define ISW_BG0_HARD                                                           \
  ((Color){.raw = {0.005605f, 0.006512f, 0.005182f, 1}}) /* #111310 */

#define ISW_BG0                                                                \
  ((Color){.raw = {0.010960f, 0.010960f, 0.009134f, 1}}) /* #1B1B18 */

#define ISW_BG1                                                                \
  ((Color){.raw = {0.019382f, 0.016807f, 0.012983f, 1}}) /* #26231E */

#define ISW_BG2                                                                \
  ((Color){.raw = {0.038204f, 0.030713f, 0.022174f, 1}}) /* #373129 */

#define ISW_BG3                                                                \
  ((Color){.raw = {0.082283f, 0.064803f, 0.042311f, 1}}) /* #51483A */

/* CREAM / PLASTIC */
#define ISW_CREAM_SHADOW                                                       \
  ((Color){.raw = {0.341914f, 0.262251f, 0.141263f, 1}}) /* #9E8C69 */

#define ISW_CREAM_DARK                                                         \
  ((Color){.raw = {0.502886f, 0.401978f, 0.223228f, 1}}) /* #BCAA82 */

#define ISW_CREAM                                                              \
  ((Color){.raw = {0.693872f, 0.571125f, 0.337164f, 1}}) /* #D9C79D */

#define ISW_CREAM_LIGHT0                                                       \
  ((Color){.raw = {0.814847f, 0.686685f, 0.445201f, 1}}) /* #E9D8B2 */

#define ISW_CREAM_LIGHT0_HARD                                                  \
  ((Color){.raw = {0.913099f, 0.806952f, 0.603827f, 1}}) /* #F5E8CC */

/* TEAL */
#define ISW_TEAL_DARK                                                          \
  ((Color){.raw = {0.000000f, 0.114435f, 0.135633f, 1}}) /* #005F67 */

#define ISW_TEAL                                                               \
  ((Color){.raw = {0.003035f, 0.234551f, 0.242281f, 1}}) /* #0A8587 */

#define ISW_TEAL_LIGHT                                                         \
  ((Color){.raw = {0.124772f, 0.473531f, 0.391572f, 1}}) /* #63B7A8 */

/* WARM ACCENTS */
#define ISW_YELLOW                                                             \
  ((Color){.raw = {0.768151f, 0.351533f, 0.009134f, 1}}) /* #E3A018 */

#define ISW_ORANGE                                                             \
  ((Color){.raw = {0.665387f, 0.144128f, 0.003347f, 1}}) /* #D56A0B */

#define ISW_RED                                                                \
  ((Color){.raw = {0.479320f, 0.042311f, 0.017642f, 1}}) /* #B83A24 */

#define ISW_RED_DARK                                                           \
  ((Color){.raw = {0.274677f, 0.020289f, 0.013702f, 1}}) /* #8F271F */

/* OPTIONAL COOL ACCENT */
#define ISW_BLUE                                                               \
  ((Color){.raw = {0.008568f, 0.152926f, 0.194618f, 1}}) /* #176D7A */

#endif