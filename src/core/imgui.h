// #ifndef CORE_UI_H
// #define CORE_UI_H

// #include "core/allocator.h"
// #include "core/map.h"
// #include "core/strings.h"
// #include "core/types.h"

// typedef enum Element_Error {
//   Element_Error_None = 0,
//   Element_Error_Failed_To_Initialize_Context,
// } Element_Error;

// typedef u32 Element_Flags;
// typedef enum Element_Flag {
//   // Behavior flags
//   Element_Flag_Visible = 1 << 0,
//   Element_Flag_Ignore_Events = 1 << 1,
//   Element_Flag_Interactive = 1 << 2,

//   // Rendering flags
//   Element_Flag_Render_Background = 1 << 3,
//   Element_Flag_Render_Text = 1 << 4,
//   Element_Flag_Render_Image = 1 << 5,
// } Element_Flag;

// typedef u32 Element_Events;
// typedef enum Element_Event {
//   Element_Event_Hovered = 1 << 0,
//   Element_Event_Left_Clicked = 1 << 1,
//   Element_Event_Right_Clicked = 1 << 2,
// } Element_Event;

// typedef enum Element_State {
//   Element_State_Clear,
//   Element_State_Hovered,
//   Element_State_Clicked,
//   Element_State_MAX,
// } Element_State;

// typedef struct Element_Id {
//   u32 hash;
//   u32 index;
// } Element_Id;

// typedef struct Element_Rectangle {
//   f32 x;
//   f32 y;
//   f32 width;
//   f32 height;
// } Element_Rectangle;

// typedef struct Element_Point {
//   f32 x;
//   f32 y;
// } Element_Point;

// typedef struct Element_Dimensions {
//   f32 width;
//   f32 height;
// } Element_Dimensions;

// typedef enum Element_Axis {
//   Element_Axis_Horizontal,
//   Element_Axis_Vertical,
// } Element_Axis;

// typedef enum Element_Direction {
//   Element_Direction_Top,
//   Element_Direction_Right,
//   Element_Direction_Bottom,
//   Element_Direction_Left,
//   Element_Direction_MAX,
// } Element_Direction;

// typedef enum Element_Layout_Kind {
//   Element_Layout_Kind_None,
//   Element_Layout_Kind_Row,
//   Element_Layout_Kind_Column,
// } Element_Layout_Kind;

// typedef struct Element_Sizing {
//   enum {
//     Element_Sizing_Fit,
//     Element_Sizing_Grow,
//     Element_Sizing_Shrink,
//     Element_Sizing_Fixed,
//   } kind;
//   f32 value;
// } Element_Sizing;

// typedef enum Element_Alignment {
//   Element_Alignment_Start,
//   Element_Alignment_Center,
//   Element_Alignment_End,
//   Element_Alignment_Space_Evenly,
//   Element_Alignment_Space_Between,
// } Element_Alignment;

// typedef struct Element_Color {
//   f32 r;
//   f32 g;
//   f32 b;
//   f32 a;
// } Element_Color;

// typedef struct Element_Border_Color {
//   enum {
//     Element_Border_Color_Uniform,
//     Element_Border_Color_Directional,
//   } kind;
//   union {
//     Element_Color uniform;
//     Element_Color directional[Element_Direction_MAX];
//   } variant;
// } Element_Border_Color;

// typedef struct Element_Padding {
//   f32 left;
//   f32 right;
//   f32 top;
//   f32 bottom;
// } Element_Padding;

// typedef struct Element_Font {
//   f32 size;
//   void *data;
// } Element_Font;

// typedef struct Element_Image {
//   f32 width;
//   f32 height;
//   void *data;
// } Element_Image;

// typedef struct Element_Position {
//   Element_Point raw_offset;
//   f32 percent_offset[2];
//   enum {
//     Element_Position_Relative,
//     Element_Position_Absolute,
//   } kind;
//   enum {
//     Element_Position_Anchor_Top_Left,
//     Element_Position_Anchor_Top_Right,
//     Element_Position_Anchor_Bottom_Left,
//     Element_Position_Anchor_Bottom_Right,
//   } anchor;
// } Element_Position;

// typedef enum Element_Ease_Fn {
//   Element_Ease_Fn_Linear,
//   Element_Ease_Fn_In,
//   Element_Ease_Fn_Out,
//   Element_Ease_Fn_In_Out,
//   Element_Ease_Fn_In_Cubic,
//   Element_Ease_Fn_Out_Cubic,
//   Element_Ease_Fn_In_Out_Cubic,
//   Element_Ease_Fn_Back_In,
//   Element_Ease_Fn_Back_Out,
// } Element_Ease_Fn;

// typedef enum Element_Property {
//   // f32 properties
//   Element_Property_Border,
//   Element_Property_Radius,
//   Element_Property_Child_Gap,
//   Element_Property_Font_Size,
//   // color properties
//   Element_Property_Background_Color,
//   Element_Property_Border_Color,
//   Element_Property_Text_Color,
//   Element_Property_Image_Color,

//   Element_Property_MAX,
// } Element_Property;

// typedef struct Element_Style_Property_Values {
//   union {
//     f32 f32[Element_State_MAX];
//     Element_Color color[Element_State_MAX];
//   };
// } Element_Style_Property_Values;

// typedef struct Element_Style_Property {
//   f32 duration; // 0 = instant snap (default)
//   Element_Ease_Fn ease;
//   Element_Style_Property_Values values;
// } Element_Style_Property;

// typedef struct Element_Style {
//   Element_Style_Property properties[Element_Property_MAX];

//   // Non-animated (for now)
//   Element_Padding padding;
//   void *font_data;
//   struct {
//     Element_Rectangle source_rect;
//     bool32 horizontal_flip;
//     bool32 vertical_flip;
//   } image_options;
// } Element_Style;

// typedef struct Element_Create_Info {
//   Element_Id id;
//   Element_Flags override_flags;
//   Element_Style style;
//   Element_Layout_Kind layout;
//   struct {
//     Element_Sizing width;
//     Element_Sizing height;
//   } sizing;
//   struct {
//     Element_Alignment horizontal;
//     Element_Alignment vertical;
//   } alignment;
//   Element_Position position;
//   String text;
//   Element_Image image;
// } Element_Create_Info;

// typedef struct Element_Info {
//   Element_Id id;
//   Element_Layout_Kind layout;
//   struct {
//     Element_Sizing width;
//     Element_Sizing height;
//   } sizing;
//   struct {
//     Element_Alignment horizontal;
//     Element_Alignment vertical;
//   } alignment;
//   Element_Position position;
//   String text;
//   Element_Image image;
// } Element_Info;

// typedef struct Element Element;
// struct Element {
//   Element_Info info;
//   Element_Style style;
//   Element_Flags flags;
//   Element_Rectangle computed_rect;

//   // Internal graph
//   Element *parent;
//   Element *first_child;
//   Element *last_child;
//   Element *next;
//   Element *previous;
//   u32 child_count;
//   u32 relative_child_count;
// };

// typedef struct Element_Property_Animation {
//   union {
//     f32 f32;
//     Element_Color color;
//   } from;
//   f32 time;
// } Element_Property_Animation;

// typedef struct Element_Cached_Info {
//   Element_Rectangle computed_rect;
//   Element_Events previous_events;
//   Element_Events events;
//   usize last_touched;

//   Element_State target_state;
//   Element_Property_Animation animations[Element_Property_MAX];
// } Element_Cached_Info;

// typedef struct Element_Client_Info {
//   Element_Rectangle computed_rect;
//   Element_Events events;
// } Element_Client_Info;

// typedef struct Element_Iterator {
//   Element *element;
//   usize iteration;
// } Element_Iterator;

// typedef struct Element_Input_Info {
//   bool8 just_pressed;
//   bool8 previously_just_pressed;
//   bool8 just_released;
//   bool8 previously_just_released;
//   bool8 pressed;
//   bool8 previously_pressed;
// } Element_Input_Info;

// typedef u32 Element_Context_Flags;
// typedef enum Element_Context_Flag {
//   Element_Context_Flag_Validation_Layer = 1 << 0,
// } Element_Context_Flag;

// typedef struct Element_Context_Create_Info {
//   usize init_cap;
//   Element_Context_Flags flags;
//   Element_Dimensions (*measure_text_proc)(Element_Font font, String text);
// } Element_Context_Create_Info;

// // Rendering Commands
// typedef struct Element_Render_Command {
//   enum {
//     Element_Render_Command_Rectangle,
//     Element_Render_Command_Line,
//     Element_Render_Command_Text,
//     Element_Render_Command_Image,
//   } kind;
//   union {
//     struct {
//       Element_Rectangle rect;
//       f32 radius;
//       f32 border;
//       Element_Color color;
//       Element_Border_Color border_color;
//     } rectangle;
//     struct {
//       Element_Point start;
//       Element_Point end;
//       f32 thickness;
//       Element_Color color;
//     } line;
//     struct {
//       Element_Point origin;
//       String chars;
//       Element_Font font;
//       Element_Color color;
//     } text;
//     struct {
//       Element_Rectangle dst_rect;
//       Element_Rectangle src_rect;
//       Element_Image source;
//       Element_Color color;
//       bool32 horizontal_flip;
//       bool32 vertical_flip;
//     } image;
//   } variant;
// } Element_Render_Command;

// typedef struct Element_Render_Command_Buffer {
//   Element_Render_Command *items;
//   usize len;
// } Element_Render_Command_Buffer;

// #define LIST_TYPE Element
// #define LIST_TYPE_NAME Element_List
// #define LIST_FUNCTION_PREFIX element_list
// #include "core/list.h"

// #define LIST_TYPE usize
// #define LIST_TYPE_NAME Element_Index_List
// #define LIST_FUNCTION_PREFIX element_index_list
// #include "core/list.h"

// #define LIST_TYPE Element_Render_Command
// #define LIST_TYPE_NAME Element_Render_List
// #define LIST_FUNCTION_PREFIX element_render_list
// #include "core/list.h"

// typedef struct Element_Context {
//   Allocator allocator;
//   Element_Context_Flags flags;
//   Element_List elements;
//   Element_Index_List element_roots;
//   Element_Index_List element_stack;
//   Open_Map element_cache;
//   Element *current_element;
//   Element *active_element;

//   // Input states
//   u64 frame_counter;
//   u32 id_counter;
//   f32 dt;
//   Element_Dimensions screen_dimensions;
//   Element_Point m_pos;
//   Element_Point m_previous_pos;
//   Element_Point m_delta;
//   bool32 m_over_ui;
//   bool32 m_previously_over_ui;
//   Element_Input_Info m_left;
//   Element_Input_Info m_right;

//   // Cached list
//   Element_Render_List commands;

//   // Callbacks
//   Element_Dimensions (*measure_text)(Element_Font font, String text);
// } Element_Context;

// ///////////////////////
// // Public API
// ///////////////////////
// Element_Error init_element_context(
//     Element_Context *ctx, Element_Context_Create_Info *info, Allocator
//     allocator
// );
// Element_Error close_element_context(Element_Context *ctx);

// void set_context_current(Element_Context *ctx);
// void set_screen_state(Element_Context *ctx, Element_Dimensions dimensions);
// void set_pointer_state(
//     Element_Context *ctx, Element_Point m_pos, bool32 m_left, bool32 m_right
// );
// void set_delta_time(Element_Context *ctx, f32 dt);

// Element_Point get_mouse_delta();
// Element_Point get_mouse_position();

// void begin_ui(Element_Context *ctx);
// Element_Render_Command_Buffer end_ui(Element_Context *ctx);

// void begin_element(Element_Create_Info *info, Element_Flags flags);
// void end_element();

// Element_Client_Info get_current_element();

// #define UI_CONCAT_RAW(a, b) a##b
// #define UI_CONCAT(a, b) UI_CONCAT_RAW(a, b)
// #define UI_ONCE UI_CONCAT(_once_, __LINE__)

// #define container(info) \
//   for (u8 UI_ONCE = \
//            (begin_element( \
//                 info, \
//                 ((info)->override_flags | Element_Flag_Visible | \
//                  Element_Flag_Render_Background) \
//             ), \
//             0); \
//        !UI_ONCE; \ UI_ONCE = (end_element(), 1))

// #define label(info) \
//   for (u8 UI_ONCE = \
//            (begin_element( \
//                 info, \
//                 ((info)->override_flags | Element_Flag_Visible | \
//                  Element_Flag_Render_Background | Element_Flag_Render_Text) \
//             ), \
//             0); \
//        !UI_ONCE; \ UI_ONCE = (end_element(), 1))

// #define button(info) \
//   for (u8 UI_ONCE = \
//            (begin_element( \
//                 info, \
//                 ((info)->override_flags | Element_Flag_Visible | \
//                  Element_Flag_Interactive | Element_Flag_Render_Background) \
//             ), \
//             0); \
//        !UI_ONCE; \ UI_ONCE = (end_element(), 1))

// #define image(info) \
//   for (u8 UI_ONCE = \
//            (begin_element( \
//                 info, \
//                 ((info)->override_flags | Element_Flag_Visible | \
//                  Element_Flag_Render_Image) \
//             ), \
//             0); \
//        !UI_ONCE; \ UI_ONCE = (end_element(), 1))

// #define image_interactive(info) \
//   for (u8 UI_ONCE = \
//            (begin_element( \
//                 info, \
//                 ((info)->override_flags | Element_Flag_Visible | \
//                  Element_Flag_Interactive | Element_Flag_Render_Image) \
//             ), \
//             0); \
//        !UI_ONCE; \ UI_ONCE = (end_element(), 1))

// #undef UI_ONCE
// #undef UI_CONCAT
// #undef UI_CONCAT_RAW

// ///////////////////////
// // Builder helpers
// ///////////////////////

// static inline Element_Sizing sizing_fixed(f32 value) {
//   return (Element_Sizing){.kind = Element_Sizing_Fixed, .value = value};
// }

// static inline Element_Sizing sizing_grow() {
//   return (Element_Sizing){.kind = Element_Sizing_Grow};
// }

// static inline Element_Sizing sizing_fit() {
//   return (Element_Sizing){.kind = Element_Sizing_Fit};
// }

// static inline bool32 element_property_is_color(Element_Property prop) {
//   return prop >= Element_Property_Background_Color;
// }

// static inline Element_Style_Property f32_property_uniform(f32 value) {
//   return (Element_Style_Property){.values = {.f32 = {value, value, value}}};
// }

// static inline Element_Style_Property
// f32_property(f32 clear, f32 hovered, f32 clicked) {
//   return (Element_Style_Property){.values = {.f32 = {clear, hovered,
//   clicked}}};
// }

// static inline Element_Style_Property f32_property_anim(
//     f32 clear, f32 hovered, f32 clicked, f32 duration, Element_Ease_Fn ease
// ) {
//   return (Element_Style_Property){
//     .duration = duration,
//     .ease = ease,
//     .values = {.f32 = {clear, hovered, clicked}},
//   };
// }

// static inline Element_Style_Property
// color_property_uniform(Element_Color value) {
//   return (Element_Style_Property){.values = {.color = {value, value,
//   value}}};
// }

// static inline Element_Style_Property color_property(
//     Element_Color clear, Element_Color hovered, Element_Color clicked
// ) {
//   return (Element_Style_Property){
//     .values = {.color = {clear, hovered, clicked}},
//   };
// }

// static inline Element_Style_Property color_property_anim(
//     Element_Color clear,
//     Element_Color hovered,
//     Element_Color clicked,
//     f32 duration,
//     Element_Ease_Fn ease
// ) {
//   return (Element_Style_Property){
//     .duration = duration,
//     .ease = ease,
//     .values = {.color = {clear, hovered, clicked}},
//   };
// }

// #define CLR_TRANSPARENT ((Element_Color){0, 0, 0, 0})
// #define CLR_WHITE ((Element_Color){1, 1, 1, 1})
// #define CLR_BLACK ((Element_Color){0, 0, 0, 1})
// #define CLR_RED ((Element_Color){1, 0, 0, 1})

// #endif