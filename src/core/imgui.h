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
} Element_Flag;

typedef u32 Element_Events;
typedef enum Element_Event {
  Element_Event_Hovered = 1 << 0,
  Element_Event_Left_Clicked = 1 << 1,
  Element_Event_Right_Clicked = 1 << 2,
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

#define LINEAR_PROPERTY_ENUM_RAW_INDEX(e)                                      \
  ((e) - Element_Property_linear_start_ - 1)
#define COLOR_PROPERTY_ENUM_RAW_INDEX(e)                                       \
  ((e) - Element_Property_color_start_ - 1)
#define VARIABLE_COLOR_PROPERTY_ENUM_RAW_INDEX(e)                              \
  ((e) - Element_Property_variable_color_start_ - 1)
#define CONSTRAINT_PROPERTY_ENUM_RAW_INDEX(e)                                  \
  ((e) - Element_Property_constraint_start_ - 1)

// NOTE(nico): fuck having differente transition time per property. It has very
// limited use anyway and I hate myself for having to unpack it into the
// internal style
typedef struct Element_Style {
  struct {
    struct {
      f32 border[Element_State_MAX];
      f32 radius[Element_State_MAX];
      f32 child_gap[Element_State_MAX];
      f32 font_size[Element_State_MAX];
    } linears;
    struct {
      Color background[Element_State_MAX];
      Color text[Element_State_MAX];
      Color image[Element_State_MAX];
    } colors;
    struct {
      Element_Variable_Color border[Element_State_MAX];
    } variable_colors;
    struct {
      Element_Constraint padding[Element_State_MAX];
    } constraints;
  } properties;
  struct {
    f32 duration;
    Element_Ease_Fn ease;
  } transitions[Element_State_MAX];

  // Non-animated
  void *font_data;
  Element_Image_Option image_options;
} Element_Style;

typedef struct Element_Internal_Style {
  f32 linear_properties[LINEAR_PROPERTIES_CAP][Element_State_MAX];
  Color color_properties[COLOR_PROPERTIES_CAP][Element_State_MAX];
  Element_Variable_Color
      variable_color_properties[VARIABLE_COLOR_PROPERTIES_CAP]
                               [Element_State_MAX];
  Element_Constraint constraint_properties[CONSTRAINT_PROPERTIES_CAP]
                                          [Element_State_MAX];
  struct {
    f32 duration;
    Element_Ease_Fn ease;
  } transitions[Element_State_MAX];

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

// Vec2 get_mouse_delta();
// Vec2 get_mouse_position();

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

#define container(info)                                                        \
  for (u8 UI_ONCE =                                                            \
           (begin_element(                                                     \
                info,                                                          \
                ((info)->override_flags | Element_Flag_Visible |               \
                 Element_Flag_Render_Background)                               \
            ),                                                                 \
            0);                                                                \
       !UI_ONCE;                                                               \
       UI_ONCE = (end_element(), 1))

#define label(info)                                                            \
  for (u8 UI_ONCE =                                                            \
           (begin_element(                                                     \
                info,                                                          \
                ((info)->override_flags | Element_Flag_Visible |               \
                 Element_Flag_Render_Background | Element_Flag_Render_Text)    \
            ),                                                                 \
            0);                                                                \
       !UI_ONCE;                                                               \
       UI_ONCE = (end_element(), 1))

#define button(info)                                                           \
  for (u8 UI_ONCE =                                                            \
           (begin_element(                                                     \
                info,                                                          \
                ((info)->override_flags | Element_Flag_Visible |               \
                 Element_Flag_Interactive | Element_Flag_Render_Background)    \
            ),                                                                 \
            0);                                                                \
       !UI_ONCE;                                                               \
       UI_ONCE = (end_element(), 1))

#define image(info)                                                            \
  for (u8 UI_ONCE =                                                            \
           (begin_element(                                                     \
                info,                                                          \
                ((info)->override_flags | Element_Flag_Visible |               \
                 Element_Flag_Render_Image)                                    \
            ),                                                                 \
            0);                                                                \
       !UI_ONCE;                                                               \
       UI_ONCE = (end_element(), 1))

#define image_interactive(info)                                                \
  for (u8 UI_ONCE =                                                            \
           (begin_element(                                                     \
                info,                                                          \
                ((info)->override_flags | Element_Flag_Visible |               \
                 Element_Flag_Interactive | Element_Flag_Render_Image)         \
            ),                                                                 \
            0);                                                                \
       !UI_ONCE;                                                               \
       UI_ONCE = (end_element(), 1))

// #undef UI_ONCE
// #undef UI_CONCAT
// #undef UI_CONCAT_RAW

// #define CLR_TRANSPARENT ((Element_Color){0, 0, 0, 0})
// #define CLR_WHITE ((Element_Color){1, 1, 1, 1})
// #define CLR_BLACK ((Element_Color){0, 0, 0, 1})
// #define CLR_RED ((Element_Color){1, 0, 0, 1})

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

////////////////////
// Static asserts
////////////////////
// NOTE(nico): Element_Style is memcpy'd straight onto Element_Internal_Style.
// That is only valid while every authored member sits at the exact offset of
// the dense slot its property enum resolves to. Size equality alone does not
// catch a reorder, so each slot is pinned individually: swapping two members,
// reordering the enum or changing an index macro breaks the build instead of
// silently transposing properties at runtime.
#define ELEMENT_STYLE_SLOT_MATCHES(authored, dense_slot)                       \
  static_assert(                                                               \
      offsetof(Element_Style, properties.authored) ==                          \
          offsetof(Element_Internal_Style, dense_slot),                        \
      "Element style layout drift: " #authored " vs " #dense_slot              \
  )

// Whole-group sizes. Catches a property added to one side only, which would
// otherwise just shift every following group in lockstep.
#define ELEMENT_STYLE_GROUP_MATCHES(authored, dense)                           \
  static_assert(                                                               \
      sizeof(((Element_Style *)nullptr)->properties.authored) ==               \
          sizeof(((Element_Internal_Style *)nullptr)->dense),                  \
      "Element style group size drift: " #authored " vs " #dense               \
  )

// Members past the property block, shared verbatim by both layouts.
#define ELEMENT_STYLE_TAIL_MATCHES(member)                                     \
  static_assert(                                                               \
      offsetof(Element_Style, member) ==                                       \
          offsetof(Element_Internal_Style, member),                            \
      "Element style layout drift: " #member                                   \
  )

ELEMENT_STYLE_GROUP_MATCHES(linears, linear_properties);
ELEMENT_STYLE_SLOT_MATCHES(
    linears.border,
    linear_properties[LINEAR_PROPERTY_ENUM_RAW_INDEX(Element_Property_Border)]
);
ELEMENT_STYLE_SLOT_MATCHES(
    linears.radius,
    linear_properties[LINEAR_PROPERTY_ENUM_RAW_INDEX(Element_Property_Radius)]
);
ELEMENT_STYLE_SLOT_MATCHES(
    linears.child_gap,
    linear_properties
        [LINEAR_PROPERTY_ENUM_RAW_INDEX(Element_Property_Child_Gap)]
);
ELEMENT_STYLE_SLOT_MATCHES(
    linears.font_size,
    linear_properties
        [LINEAR_PROPERTY_ENUM_RAW_INDEX(Element_Property_Font_Size)]
);

ELEMENT_STYLE_GROUP_MATCHES(colors, color_properties);
ELEMENT_STYLE_SLOT_MATCHES(
    colors.background,
    color_properties
        [COLOR_PROPERTY_ENUM_RAW_INDEX(Element_Property_Background_Color)]
);
ELEMENT_STYLE_SLOT_MATCHES(
    colors.text,
    color_properties[COLOR_PROPERTY_ENUM_RAW_INDEX(Element_Property_Text_Color)]
);
ELEMENT_STYLE_SLOT_MATCHES(
    colors.image,
    color_properties
        [COLOR_PROPERTY_ENUM_RAW_INDEX(Element_Property_Image_Color)]
);

ELEMENT_STYLE_GROUP_MATCHES(variable_colors, variable_color_properties);
ELEMENT_STYLE_SLOT_MATCHES(
    variable_colors.border,
    variable_color_properties
        [VARIABLE_COLOR_PROPERTY_ENUM_RAW_INDEX(Element_Property_Border_Color)]
);

ELEMENT_STYLE_GROUP_MATCHES(constraints, constraint_properties);
ELEMENT_STYLE_SLOT_MATCHES(
    constraints.padding,
    constraint_properties
        [CONSTRAINT_PROPERTY_ENUM_RAW_INDEX(Element_Property_Padding)]
);

ELEMENT_STYLE_TAIL_MATCHES(transitions);
ELEMENT_STYLE_TAIL_MATCHES(font_data);
ELEMENT_STYLE_TAIL_MATCHES(image_options);

static_assert(
    sizeof(Element_Style) == sizeof(Element_Internal_Style),
    "Element style size drift"
);
static_assert(
    alignof(Element_Style) == alignof(Element_Internal_Style),
    "Element style alignment drift"
);

#undef ELEMENT_STYLE_SLOT_MATCHES
#undef ELEMENT_STYLE_GROUP_MATCHES
#undef ELEMENT_STYLE_TAIL_MATCHES

#endif