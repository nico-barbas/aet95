#ifndef CORE_UI_H
#define CORE_UI_H

#include "core/allocator.h"
#include "core/map.h"
#include "core/math.h"
#include "core/strings.h"
#include "core/types.h"

#include <assert.h>
#include <stddef.h>

typedef enum Element_Error {
  Element_Error_None = 0,
  Element_Error_Failed_To_Initialize_Context,
} Element_Error;

// typedef u64 Element_Handle;

typedef enum Element_State {
  Element_State_Enter,
  Element_State_Normal,
  Element_State_Focus,
  Element_State_Exit,
  Element_State_MAX,
} Element_State;

typedef u32 Element_Flags;
typedef enum Element_Flag {
  // Behavior flags
  Element_Flag_Visible = 1 << 0,
  Element_Flag_Ignore_Events = 1 << 1,
  Element_Flag_Interactive = 1 << 2,

  // Rendering flags
  Element_Flag_Render_Background = 1 << 3,
  Element_Flag_Render_Text = 1 << 4,
  Element_Flag_Render_Image = 1 << 5,
  Element_Flag_Render_Custom = 1 << 6,
} Element_Flag;

typedef u32 Element_Events;
typedef enum Element_Event {
  Element_Event_Entered = 1 << 0,
  Element_Event_Hovered = 1 << 1,
  Element_Event_Left_Clicked = 1 << 2,
  Element_Event_Right_Clicked = 1 << 3,
} Element_Event;

typedef enum Element_Layout_Kind {
  Element_Layout_Kind_None,
  Element_Layout_Kind_Row,
  Element_Layout_Kind_Column,
} Element_Layout_Kind;

typedef struct Element_Sizing {
  enum {
    Element_Sizing_Fit,
    Element_Sizing_Grow,
    Element_Sizing_Shrink,
    Element_Sizing_Fixed,
  } kind;
  f32 value;
} Element_Sizing;

typedef enum Element_Alignment {
  Element_Alignment_Start,
  Element_Alignment_Center,
  Element_Alignment_End,
  Element_Alignment_Space_Evenly,
  Element_Alignment_Space_Between,
} Element_Alignment;

typedef struct Element_Dimensions {
  f32 width;
  f32 height;
} Element_Dimensions;

// This is semantically fucked, but idc
typedef struct Element_Variable_Color {
  union {
    Color uniform;
    Color cardinal[Cardinality_4D_MAX];
  };
  bool8 is_cardinal;
} Element_Variable_Color;

typedef struct Element_Constraint {
  f32 left;
  f32 right;
  f32 top;
  f32 bottom;
} Element_Constraint;

typedef struct Element_Font {
  f32 size;
  i32 user_index;
  void *data;
} Element_Font;

typedef struct Element_Image {
  f32 width;
  f32 height;
  void *data;
} Element_Image;

typedef struct Element_Image_Option {
  Rectangle source_rect;
  bool32 horizontal_flip;
  bool32 vertical_flip;
} Element_Image_Option;

typedef struct Element_Position {
  enum {
    Element_Position_Relative,
    Element_Position_Absolute,
  } kind;
  enum {
    Element_Position_Anchor_Top_Left,
    Element_Position_Anchor_Top_Right,
    Element_Position_Anchor_Bottom_Left,
    Element_Position_Anchor_Bottom_Right,
  } absolute_anchor;
  Vec2 pixel_offset;
  Vec2 percent_offset;
} Element_Position;

typedef enum Element_Ease_Fn : byte {
  Element_Ease_Fn_Linear,
  Element_Ease_Fn_In,
  Element_Ease_Fn_Out,
  Element_Ease_Fn_In_Out,
  Element_Ease_Fn_In_Cubic,
  Element_Ease_Fn_Out_Cubic,
  Element_Ease_Fn_In_Out_Cubic,
  Element_Ease_Fn_Back_In,
  Element_Ease_Fn_Back_Out,
} Element_Ease_Fn;

typedef enum Element_Property {
  // f32 properties
  Element_Property_linear_start_,
  Element_Property_Border,
  Element_Property_Radius,
  Element_Property_Child_Gap,
  Element_Property_Font_Size,
  Element_Property_linear_end_,
  // color properties
  Element_Property_color_start_,
  Element_Property_Background_Color,
  Element_Property_Text_Color,
  Element_Property_Image_Color,
  Element_Property_color_end_,
  // cardinal color properties
  Element_Property_variable_color_start_,
  Element_Property_Border_Color,
  Element_Property_variable_color_end_,
  // constraint properties
  Element_Property_constraint_start_,
  Element_Property_Padding,
  Element_Property_constraint_end_,

  Element_Property_MAX,
} Element_Property;

#define LINEAR_PROPERTIES_CAP                                                  \
  (Element_Property_linear_end_ - Element_Property_linear_start_ - 1)
#define COLOR_PROPERTIES_CAP                                                   \
  (Element_Property_color_end_ - Element_Property_color_start_ - 1)
#define VARIABLE_COLOR_PROPERTIES_CAP                                          \
  (Element_Property_variable_color_end_ -                                      \
   Element_Property_variable_color_start_ - 1)
#define CONSTRAINT_PROPERTIES_CAP                                              \
  (Element_Property_constraint_end_ - Element_Property_constraint_start_ - 1)

typedef u32 Element_Property_Mask;

typedef struct Element_Style_Properties {
  struct {
    f32 border;
    f32 radius;
    f32 child_gap;
    f32 font_size;
  } linears;
  struct {
    Color background;
    Color text;
    Color image;
  } colors;
  struct {
    Element_Variable_Color border;
  } variable_colors;
  struct {
    Element_Constraint padding;
  } constraints;
} Element_Style_Properties;

typedef enum Element_Style_Variant {
  Element_Style_Variant_Enter,
  Element_Style_Variant_Focus,
  Element_Style_Variant_Exit,
  Element_Style_Variant_MAX,
} Element_Style_Variant;

typedef u32 Element_Transition_Mask;

typedef struct Element_Transition {
  f32 duration;
  Element_Ease_Fn ease;
} Element_Transition;

// NOTE(nico): fuck having differente transition time per property. It has very
// limited use anyway and I hate myself for having to unpack it into the
// internal style
typedef struct Element_Style {
  Element_Style_Properties base;
  Element_Style_Properties variants[Element_Style_Variant_MAX];
  Element_Property_Mask variant_masks[Element_Style_Variant_MAX];
  Element_Transition_Mask transition_set;
  Element_Transition transitions[Element_State_MAX];

  // Non-animated
  void *font_data;
  i32 font_index;
  Element_Image_Option image_options;
} Element_Style;

typedef struct Element_Internal_Style {
  f32 linear_properties[Element_State_MAX][LINEAR_PROPERTIES_CAP];
  Color color_properties[Element_State_MAX][COLOR_PROPERTIES_CAP];
  Element_Variable_Color
      variable_color_properties[Element_State_MAX]
                               [VARIABLE_COLOR_PROPERTIES_CAP];
  Element_Constraint constraint_properties[Element_State_MAX]
                                          [CONSTRAINT_PROPERTIES_CAP];
  Element_Transition_Mask transition_set;
  Element_Transition transitions[Element_State_MAX];

  void *font_data;
  Element_Image_Option image_options;
} Element_Internal_Style;

typedef struct Element_Create_Info {
  Option(u32) id;
  Element_Flags override_flags;
  Element_Style style;
  Element_Layout_Kind layout;
  struct {
    Element_Sizing width;
    Element_Sizing height;
  } sizing;
  struct {
    Element_Alignment horizontal;
    Element_Alignment vertical;
  } alignment;
  Element_Position position;
  String text;
  Element_Image image;
  void (*content_proc)(Rectangle rect, rawptr data);
  rawptr content_data;
} Element_Create_Info;

typedef struct Element Element;
struct Element {
  u32 id;
  Element_Flags flags;
  Element_Internal_Style style;

  // Internal graph
  Element *parent;
  Element *first_child;
  Element *last_child;
  Element *next;
  Element *previous;
  u32 child_count;
  u32 relative_child_count;

  // Layout fields
  Element_Layout_Kind layout;
  struct {
    Element_Sizing width;
    Element_Sizing height;
  } sizing;
  struct {
    Element_Alignment horizontal;
    Element_Alignment vertical;
  } alignment;
  Element_Position position;

  // Content fields
  Rectangle computed_rect;
  String text;
  Element_Image image;

  // This is the escape latch for using the layout engine but having access to
  // custom rendering
  rawptr content_data;
  void (*content_proc)(Rectangle rect, rawptr data);
};

typedef struct Element_Cached_Info {
  Rectangle computed_rect;
  Element_Events previous_events;
  Element_Events events;
  usize last_touched;

  Element_State target_state;
  f32 transition_time;

  f32 linear_properties[LINEAR_PROPERTIES_CAP];
  Color color_properties[COLOR_PROPERTIES_CAP];
  Element_Variable_Color
      variable_color_properties[VARIABLE_COLOR_PROPERTIES_CAP];
  Element_Constraint constraint_properties[CONSTRAINT_PROPERTIES_CAP];
} Element_Cached_Info;

typedef struct Element_Client_Info {
  Rectangle computed_rect;
  Element_Events events;
} Element_Client_Info;

typedef struct Element_Input_Info {
  bool8 just_pressed;
  bool8 previously_just_pressed;
  bool8 just_released;
  bool8 previously_just_released;
  bool8 pressed;
  bool8 previously_pressed;
} Element_Input_Info;

typedef u32 Element_Context_Flags;
typedef enum Element_Context_Flag {
  Element_Context_Flag_Validation_Layer = 1 << 0,
} Element_Context_Flag;

// Rendering Commands
typedef struct Element_Render_Command {
  enum {
    Element_Render_Command_Rectangle,
    Element_Render_Command_Line,
    Element_Render_Command_Text,
    Element_Render_Command_Image,
    Element_Render_Command_Custom,
  } kind;
  union {
    struct {
      Rectangle rect;
      f32 radius;
      f32 border;
      Color color;
      Element_Variable_Color border_color;
    } rectangle;
    struct {
      Vec2 start;
      Vec2 end;
      f32 thickness;
      Color color;
    } line;
    struct {
      Vec2 origin;
      String chars;
      Element_Font font;
      Color color;
    } text;
    struct {
      Rectangle dst_rect;
      Rectangle src_rect;
      Element_Image source;
      Color color;
      bool32 horizontal_flip;
      bool32 vertical_flip;
    } image;
    struct {
      Rectangle rect;
      rawptr data;
      void (*callback)(Rectangle rect, rawptr data);
    } custom;
  };
} Element_Render_Command;

typedef struct Element_Render_Command_Buffer {
  Element_Render_Command *items;
  usize len;
} Element_Render_Command_Buffer;

#define LIST_TYPE Element
#define LIST_TYPE_NAME Element_List
#define LIST_FUNCTION_PREFIX element_list
#include "core/list.h"

#define LIST_TYPE Element *
#define LIST_TYPE_NAME Element_Ptr_List
#define LIST_FUNCTION_PREFIX element_ptr_list
#include "core/list.h"

#define LIST_TYPE Element_Render_Command
#define LIST_TYPE_NAME Element_Render_List
#define LIST_FUNCTION_PREFIX element_render_list
#include "core/list.h"

typedef struct Element_Context {
  Allocator allocator;
  Element_Context_Flags flags;
  Element_List elements;
  Element_Ptr_List element_roots;
  Element_Ptr_List element_stack;
  Open_Map element_cache;
  Element *current_element;
  Element *active_element;

  // Input states
  u64 frame_counter;
  f32 dt;
  Element_Dimensions screen_dimensions;
  Vec2 m_pos;
  Vec2 m_previous_pos;
  Vec2 m_delta;
  bool32 m_over_ui;
  bool32 m_previously_over_ui;
  Element_Input_Info m_left;
  Element_Input_Info m_right;

  // Cached list
  Element_Render_List commands;

  // Callbacks
  Element_Dimensions (*measure_text)(Element_Font font, String text);
} Element_Context;

typedef struct Element_Context_Create_Info {
  usize init_cap;
  Element_Context_Flags flags;
  Element_Dimensions (*measure_text_proc)(Element_Font font, String text);
} Element_Context_Create_Info;

///////////////////////
// Public API
///////////////////////
Element_Error init_element_context(
    Element_Context *ctx, Element_Context_Create_Info *info, Allocator allocator
);
Element_Error destroy_element_context(Element_Context *ctx);

void set_context_current(Element_Context *ctx);
void set_screen_state(Element_Context *ctx, Element_Dimensions dimensions);
void set_pointer_state(
    Element_Context *ctx, Vec2 m_pos, bool32 m_left, bool32 m_right
);
void set_delta_time(Element_Context *ctx, f32 dt);

void begin_ui(Element_Context *ctx);
Element_Render_Command_Buffer end_ui(Element_Context *ctx);

void begin_element_impl(
    Element_Create_Info *info, Element_Flags flags, u32 callsite
);
void end_element();

Element_Client_Info get_current_element();

#define ELEMENT_CALLSITE                                                       \
  ((u32)((u32)(uintptr)__FILE__ * 0x9E3779B97F4A7C15u ^ (u32)__LINE__))
#define begin_element(info, flags)                                             \
  begin_element_impl(info, flags, ELEMENT_CALLSITE)

#define UI_CONCAT_RAW(a, b) a##b
#define UI_CONCAT(a, b) UI_CONCAT_RAW(a, b)
#define UI_ONCE UI_CONCAT(_once_, __LINE__)

#define element_container(info)                                                \
  for (u8 UI_ONCE =                                                            \
           (begin_element(                                                     \
                info,                                                          \
                ((info)->override_flags | Element_Flag_Visible |               \
                 Element_Flag_Render_Background)                               \
            ),                                                                 \
            0);                                                                \
       !UI_ONCE;                                                               \
       UI_ONCE = (end_element(), 1))

#define element_label(info)                                                    \
  for (u8 UI_ONCE =                                                            \
           (begin_element(                                                     \
                info,                                                          \
                ((info)->override_flags | Element_Flag_Visible |               \
                 Element_Flag_Render_Background | Element_Flag_Render_Text)    \
            ),                                                                 \
            0);                                                                \
       !UI_ONCE;                                                               \
       UI_ONCE = (end_element(), 1))

#define element_button(info)                                                   \
  for (u8 UI_ONCE =                                                            \
           (begin_element(                                                     \
                info,                                                          \
                ((info)->override_flags | Element_Flag_Visible |               \
                 Element_Flag_Interactive | Element_Flag_Render_Background)    \
            ),                                                                 \
            0);                                                                \
       !UI_ONCE;                                                               \
       UI_ONCE = (end_element(), 1))

#define element_image(info)                                                    \
  for (u8 UI_ONCE =                                                            \
           (begin_element(                                                     \
                info,                                                          \
                ((info)->override_flags | Element_Flag_Visible |               \
                 Element_Flag_Render_Image)                                    \
            ),                                                                 \
            0);                                                                \
       !UI_ONCE;                                                               \
       UI_ONCE = (end_element(), 1))

#define element_custom(info)                                                   \
  for (u8 UI_ONCE =                                                            \
           (begin_element(                                                     \
                info,                                                          \
                ((info)->override_flags | Element_Flag_Visible |               \
                 Element_Flag_Render_Custom)                                   \
            ),                                                                 \
            0);                                                                \
       !UI_ONCE;                                                               \
       UI_ONCE = (end_element(), 1))

static inline Element_Sizing element_sizing_fixed(f32 value) {
  return (Element_Sizing){.kind = Element_Sizing_Fixed, .value = value};
}

static inline Element_Sizing element_sizing_grow() {
  return (Element_Sizing){.kind = Element_Sizing_Grow};
}

static inline Element_Sizing element_sizing_fit() {
  return (Element_Sizing){.kind = Element_Sizing_Fit};
}

static inline Element_Constraint
element_constraint(f32 l, f32 r, f32 t, f32 b) {
  return (Element_Constraint){.left = l, .right = r, .top = t, .bottom = b};
}

//////////////////////
// Predefined colors
//////////////////////

#define BASIC_CLR_TRANSPARENT ((Color){.raw = {0, 0, 0, 0}})
#define BASIC_CLR_WHITE ((Color){.raw = {1, 1, 1, 1}})
#define BASIC_CLR_BLACK ((Color){.raw = {0, 0, 0, 1}})
#define BASIC_CLR_RED ((Color){.raw = {1, 0, 0, 1}})

// Gruvbox
// NOTE(nico): stored linear, the surface is *UnormSrgb so the hardware
// re-encodes on write. Trailing comment is the source sRGB hex.
#define GRUVBOX_CLR_BG0_HARD                                                   \
  ((Color){.raw = {0.012286f, 0.014444f, 0.015209f, 1}}) // #1d2021
#define GRUVBOX_CLR_BG0                                                        \
  ((Color){.raw = {0.021219f, 0.021219f, 0.021219f, 1}}) // #282828
#define GRUVBOX_CLR_BG0_SOFT                                                   \
  ((Color){.raw = {0.031896f, 0.029557f, 0.028426f, 1}}) // #32302f
#define GRUVBOX_CLR_BG1                                                        \
  ((Color){.raw = {0.045186f, 0.039546f, 0.036889f, 1}}) // #3c3836
#define GRUVBOX_CLR_BG2                                                        \
  ((Color){.raw = {0.080220f, 0.066626f, 0.059511f, 1}}) // #504945
#define GRUVBOX_CLR_BG3                                                        \
  ((Color){.raw = {0.132868f, 0.107023f, 0.088656f, 1}}) // #665c54
#define GRUVBOX_CLR_BG4                                                        \
  ((Color){.raw = {0.201556f, 0.158961f, 0.127438f, 1}}) // #7c6f64
#define GRUVBOX_CLR_FG0                                                        \
  ((Color){.raw = {0.964686f, 0.879622f, 0.571125f, 1}}) // #fbf1c7
#define GRUVBOX_CLR_FG1                                                        \
  ((Color){.raw = {0.830770f, 0.708376f, 0.445201f, 1}}) // #ebdbb2
#define GRUVBOX_CLR_FG2                                                        \
  ((Color){.raw = {0.665387f, 0.552011f, 0.356400f, 1}}) // #d5c4a1
#define GRUVBOX_CLR_FG3                                                        \
  ((Color){.raw = {0.508881f, 0.423268f, 0.291771f, 1}}) // #bdae93
#define GRUVBOX_CLR_FG4                                                        \
  ((Color){.raw = {0.391572f, 0.318547f, 0.230740f, 1}}) // #a89984
#define GRUVBOX_CLR_GRAY                                                       \
  ((Color){.raw = {0.287441f, 0.226966f, 0.174647f, 1}}) // #928374
#define GRUVBOX_CLR_RED                                                        \
  ((Color){.raw = {0.603827f, 0.017642f, 0.012286f, 1}}) // #cc241d
#define GRUVBOX_CLR_GREEN                                                      \
  ((Color){.raw = {0.313989f, 0.309469f, 0.010330f, 1}}) // #98971a
#define GRUVBOX_CLR_YELLOW                                                     \
  ((Color){.raw = {0.679542f, 0.318547f, 0.015209f, 1}}) // #d79921
#define GRUVBOX_CLR_BLUE                                                       \
  ((Color){.raw = {0.059511f, 0.234551f, 0.246201f, 1}}) // #458588
#define GRUVBOX_CLR_PURPLE                                                     \
  ((Color){.raw = {0.439657f, 0.122139f, 0.238398f, 1}}) // #b16286
#define GRUVBOX_CLR_AQUA                                                       \
  ((Color){.raw = {0.138432f, 0.337164f, 0.144128f, 1}}) // #689d6a
#define GRUVBOX_CLR_ORANGE                                                     \
  ((Color){.raw = {0.672443f, 0.109462f, 0.004391f, 1}}) // #d65d0e
#define GRUVBOX_CLR_BRIGHT_RED                                                 \
  ((Color){.raw = {0.964686f, 0.066626f, 0.034340f, 1}}) // #fb4934
#define GRUVBOX_CLR_BRIGHT_GREEN                                               \
  ((Color){.raw = {0.479320f, 0.496933f, 0.019382f, 1}}) // #b8bb26
#define GRUVBOX_CLR_BRIGHT_YELLOW                                              \
  ((Color){.raw = {0.955973f, 0.508881f, 0.028426f, 1}}) // #fabd2f
#define GRUVBOX_CLR_BRIGHT_BLUE                                                \
  ((Color){.raw = {0.226966f, 0.376262f, 0.313989f, 1}}) // #83a598
#define GRUVBOX_CLR_BRIGHT_PURPLE                                              \
  ((Color){.raw = {0.651406f, 0.238398f, 0.327778f, 1}}) // #d3869b
#define GRUVBOX_CLR_BRIGHT_AQUA                                                \
  ((Color){.raw = {0.270498f, 0.527115f, 0.201556f, 1}}) // #8ec07c
#define GRUVBOX_CLR_BRIGHT_ORANGE                                              \
  ((Color){.raw = {0.991102f, 0.215861f, 0.009721f, 1}}) // #fe8019
#define GRUVBOX_CLR_FADED_RED                                                  \
  ((Color){.raw = {0.337164f, 0.000000f, 0.001821f, 1}}) // #9d0006
#define GRUVBOX_CLR_FADED_GREEN                                                \
  ((Color){.raw = {0.191202f, 0.174647f, 0.004391f, 1}}) // #79740e
#define GRUVBOX_CLR_FADED_YELLOW                                               \
  ((Color){.raw = {0.462077f, 0.181164f, 0.006995f, 1}}) // #b57614
#define GRUVBOX_CLR_FADED_BLUE                                                 \
  ((Color){.raw = {0.002125f, 0.132868f, 0.187821f, 1}}) // #076678
#define GRUVBOX_CLR_FADED_PURPLE                                               \
  ((Color){.raw = {0.274677f, 0.049707f, 0.165132f, 1}}) // #8f3f71
#define GRUVBOX_CLR_FADED_AQUA                                                 \
  ((Color){.raw = {0.054480f, 0.198069f, 0.097587f, 1}}) // #427b58
#define GRUVBOX_CLR_FADED_ORANGE                                               \
  ((Color){.raw = {0.428690f, 0.042311f, 0.000911f, 1}}) // #af3a03

////////////////////
// Static asserts
////////////////////

#endif