#ifndef UI_ELEMENT_H
#define UI_ELEMENT_H

#include "core/math.h"
#include "core/strings.h"
#include "core/types.h"

typedef u32 Element_Flags;
typedef enum Element_Flag {
  Element_Flag_Content_Background = 1 << 0,
  Element_Flag_Content_Text = 1 << 1,
  Element_Flag_Content_Image = 1 << 2,
} Element_Flag;

typedef enum Element_State {
  Element_State_Normal,
  Element_State_Hovered,
  Element_State_Pressed,
  Element_State_Disabled,
} Element_State;

typedef enum Element_Axis {
  Element_Axis_Horizontal,
  Element_Axis_Vertical,
} Element_Axis;

typedef enum Element_Sizing {
  Element_Sizing_Fit,
  Element_Sizing_Grow,
  Element_Sizing_Shrink,
  Element_Sizing_Fixed,
} Element_Sizing;

typedef enum Element_Layout_Kind {
  Element_Layout_Kind_None,
  Element_Layout_Kind_Row,
  Element_Layout_Kind_Column,
} Element_Layout_Kind;

typedef enum Element_Alignment {
  Element_Alignment_Start,
  Element_Alignment_Center,
  Element_Alignment_End,
  Element_Alignment_Space_Evenly,
  Element_Alignment_Space_Between,
} Element_Alignment;

typedef struct Element_Padding {
  f32 left;
  f32 right;
  f32 top;
  f32 bottom;
} Element_Padding;

typedef struct Element_Border_Color {
  enum {
    Element_Border_Color_Uniform,
    Element_Border_Color_Directional,
  } kind;
  union {
    Color uniform;
    Color directional[Cardinality_4D_MAX];
  };
} Element_Border_Color;

typedef struct Element_Font {
  f32 size;
  rawptr data;
} Element_Font;

typedef struct Element_Image {
  f32 width;
  f32 height;
  rawptr data;
} Element_Image;

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

typedef struct Element_Style {
  bool32 ok;
} Element_Style;

typedef struct Element {
  String id;
  u64 hashed_id;
  Element_Flags flags;

  Option(usize) parent;
  Option(usize) first_child;
  Option(usize) last_child;
  Option(usize) next;
  Option(usize) previous;
  usize child_count;

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
  union {
    String text;
    Element_Image image;
  } content;
} Element;

typedef struct Element_Create_Info {
  String id;

} Element_Create_Info;

#endif