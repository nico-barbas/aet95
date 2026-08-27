#include "core/imgui.h"

#include "core/allocator.h"
#include "core/map.h"
#include "core/math.h"
#include "core/types.h"

#include <assert.h>
#include <math.h>
#include <stddef.h>
#include <string.h>

#define RESERVED_ELEMENT_HASH 1

static Element_Context *_ctx;

typedef enum Element_Axis {
  Element_Axis_Horizontal,
  Element_Axis_Vertical,
} Element_Axis;

typedef struct Element_Iterator {
  Element *element;
  usize iteration;
} Element_Iterator;

typedef struct Element_Computed_Properties {
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
} Element_Computed_Properties;

#define LINEAR_PROPERTY_ENUM_RAW_INDEX(e)                                      \
  ((e) - Element_Property_linear_start_ - 1)
#define COLOR_PROPERTY_ENUM_RAW_INDEX(e)                                       \
  ((e) - Element_Property_color_start_ - 1)
#define VARIABLE_COLOR_PROPERTY_ENUM_RAW_INDEX(e)                              \
  ((e) - Element_Property_variable_color_start_ - 1)
#define CONSTRAINT_PROPERTY_ENUM_RAW_INDEX(e)                                  \
  ((e) - Element_Property_constraint_start_ - 1)

#define LINEAR_PROPERTY_INDEX_ENUM(i)                                          \
  ((Element_Property)(Element_Property_linear_start_ + 1 + (i)))
#define COLOR_PROPERTY_INDEX_ENUM(i)                                           \
  ((Element_Property)(Element_Property_color_start_ + 1 + (i)))
#define VARIABLE_COLOR_PROPERTY_INDEX_ENUM(i)                                  \
  ((Element_Property)(Element_Property_variable_color_start_ + 1 + (i)))
#define CONSTRAINT_PROPERTY_INDEX_ENUM(i)                                      \
  ((Element_Property)(Element_Property_constraint_start_ + 1 + (i)))

///////////////////////
// Helpers
///////////////////////
static u64 element_id_mix(u64 acc, u64 value) {
  const u64 element_id_mix_c0 = 0x9E3779B97F4A7C15ull;
  const u64 element_id_mix_c1 = 0xBF58476D1CE4E5B9ull;
  const u64 element_id_mix_c2 = 0x94D049BB133111EBull;

  u64 h = acc ^ (value * element_id_mix_c0);
  h ^= h >> 30;
  h *= element_id_mix_c1;
  h ^= h >> 27;
  h *= element_id_mix_c2;
  h ^= h >> 31;
  return h;
}

static bool32 element_id_eq(void *h1, void *h2) {
  u64 *_h1 = (u64 *)h1;
  u64 *_h2 = (u64 *)h2;

  return *_h1 == *_h2;
}

// static u64 element_get_handle(Element *element) {
//   return element->id;
// }

static Element_Iterator element_iterator(Element *element) {
  return (Element_Iterator){
    .element = element,
    .iteration = 0,
  };
}

static bool32 element_has_next(Element_Iterator *it) {
  return it->element != nullptr;
}

static Element *element_next(Element_Iterator *it) {
  Element *item = it->element;
  it->iteration += 1;
  it->element = it->element->next;
  return item;
}

static Element_State element_state_derive_from_events(Element_Events e) {
  if (e & Element_Event_Hovered || e & Element_Event_Left_Clicked ||
      e & Element_Event_Right_Clicked) {
    return Element_State_Focus;
  }

  if (e & Element_Event_Entered) {
    return Element_State_Normal;
  }

  return Element_State_Normal;
}

static Element_Variable_Color
variable_color_lerp(Element_Variable_Color a, Element_Variable_Color b, f32 t) {
  if (a.is_cardinal && b.is_cardinal) {
    Element_Variable_Color result = {
      .is_cardinal = true,
    };

    for (usize i = 0; i < Cardinality_4D_MAX; i += 1) {
      result.cardinal[i] = color_lerp(a.cardinal[i], b.cardinal[i], t);
    }

    return result;
  } else if (!a.is_cardinal && !b.is_cardinal) {
    return (Element_Variable_Color){
      .uniform = color_lerp(a.uniform, b.uniform, t),
    };
  } else {
    Element_Variable_Color avg_target = a.is_cardinal ? a : b;
    Color avg = {0};
    for (usize i = 0; i < Cardinality_4D_MAX; i += 1) {
      avg = color_add(avg, avg_target.cardinal[i]);
    }

    return (Element_Variable_Color){
      .is_cardinal = false,
      .uniform = color_add(
          a.is_cardinal ? b.uniform : a.uniform, color_scale(avg, 1.f / 4.f)
      ),
    };
  }
}

static Element_Constraint
element_constraint_lerp(Element_Constraint a, Element_Constraint b, f32 t) {
  return (Element_Constraint){
    .top = lerp_f32(a.top, b.top, t),
    .right = lerp_f32(a.right, b.right, t),
    .bottom = lerp_f32(a.bottom, b.bottom, t),
    .left = lerp_f32(a.left, b.left, t),
  };
}

// NOTE(nico): the state-major internal layout makes every authored group
// bit-identical to one row of its dense array, so each group is a straight
// copy. These pin the sizes; field order inside a group still has to match the
// Element_Property order by hand
static_assert(
    sizeof(((Element_Style_Properties *)nullptr)->linears) ==
        LINEAR_PROPERTIES_CAP * sizeof(f32),
    "linear property count drift between the style and Element_Property"
);
static_assert(
    sizeof(((Element_Style_Properties *)nullptr)->colors) ==
        COLOR_PROPERTIES_CAP * sizeof(Color),
    "color property count drift between the style and Element_Property"
);
static_assert(
    sizeof(((Element_Style_Properties *)nullptr)->variable_colors) ==
        VARIABLE_COLOR_PROPERTIES_CAP * sizeof(Element_Variable_Color),
    "variable color property count drift between the style and Element_Property"
);
static_assert(
    sizeof(((Element_Style_Properties *)nullptr)->constraints) ==
        CONSTRAINT_PROPERTIES_CAP * sizeof(Element_Constraint),
    "constraint property count drift between the style and Element_Property"
);

// Element_Style_Variant_MAX means the state has no variant and rests on base
static const Element_Style_Variant ELEMENT_STATE_VARIANT[Element_State_MAX] = {
  [Element_State_Enter] = Element_Style_Variant_Enter,
  [Element_State_Normal] = Element_Style_Variant_MAX,
  [Element_State_Focus] = Element_Style_Variant_Focus,
  [Element_State_Exit] = Element_Style_Variant_Exit,
};

static Element_Internal_Style element_style_to_internal(Element_Style *style) {
  Element_Internal_Style result = {
    .transition_set = style->transition_set,
    .font_data = style->font_data,
    .image_options = style->image_options,
  };
  memcpy(result.transitions, style->transitions, sizeof(result.transitions));

  for (usize i = 0; i < Element_State_MAX; i += 1) {
    // Every state starts as the base look
    memcpy(
        result.linear_properties[i],
        &style->base.linears,
        sizeof(style->base.linears)
    );
    memcpy(
        result.color_properties[i],
        &style->base.colors,
        sizeof(style->base.colors)
    );
    memcpy(
        result.variable_color_properties[i],
        &style->base.variable_colors,
        sizeof(style->base.variable_colors)
    );
    memcpy(
        result.constraint_properties[i],
        &style->base.constraints,
        sizeof(style->base.constraints)
    );

    Element_Style_Variant variant = ELEMENT_STATE_VARIANT[i];
    if (variant == Element_Style_Variant_MAX ||
        !(style->transition_set & bitmask((u32)i))) {
      continue;
    }

    // The variant only overrides the properties its mask names. Anything else
    // keeps the base value it was just given
    Element_Property_Mask mask = style->variant_masks[variant];
    Element_Style_Properties *src = &style->variants[variant];

    f32 linears[LINEAR_PROPERTIES_CAP];
    memcpy(linears, &src->linears, sizeof(linears));
    for (usize p = 0; p < LINEAR_PROPERTIES_CAP; p += 1) {
      if (mask & bitmask(LINEAR_PROPERTY_INDEX_ENUM(p))) {
        result.linear_properties[i][p] = linears[p];
      }
    }

    Color colors[COLOR_PROPERTIES_CAP];
    memcpy(colors, &src->colors, sizeof(colors));
    for (usize p = 0; p < COLOR_PROPERTIES_CAP; p += 1) {
      if (mask & bitmask(COLOR_PROPERTY_INDEX_ENUM(p))) {
        result.color_properties[i][p] = colors[p];
      }
    }

    Element_Variable_Color variable_colors[VARIABLE_COLOR_PROPERTIES_CAP];
    memcpy(variable_colors, &src->variable_colors, sizeof(variable_colors));
    for (usize p = 0; p < VARIABLE_COLOR_PROPERTIES_CAP; p += 1) {
      if (mask & bitmask(VARIABLE_COLOR_PROPERTY_INDEX_ENUM(p))) {
        result.variable_color_properties[i][p] = variable_colors[p];
      }
    }

    Element_Constraint constraints[CONSTRAINT_PROPERTIES_CAP];
    memcpy(constraints, &src->constraints, sizeof(constraints));
    for (usize p = 0; p < CONSTRAINT_PROPERTIES_CAP; p += 1) {
      if (mask & bitmask(CONSTRAINT_PROPERTY_INDEX_ENUM(p))) {
        result.constraint_properties[i][p] = constraints[p];
      }
    }
  }

  return result;
}

// NOTE(nico): This is honestly cursed
static Element_Computed_Properties process_element_computed_properties(
    Element_Internal_Style *style, Element_Cached_Info *cache
) {
  if (cache == nullptr) {
    Element_State s = Element_State_Enter;

    return (Element_Computed_Properties){
      .linears =
          {
            .border =
                style->linear_properties[s][LINEAR_PROPERTY_ENUM_RAW_INDEX(
                    Element_Property_Border
                )],
            .radius =
                style->linear_properties[s][LINEAR_PROPERTY_ENUM_RAW_INDEX(
                    Element_Property_Radius
                )],
            .child_gap =
                style->linear_properties[s][LINEAR_PROPERTY_ENUM_RAW_INDEX(
                    Element_Property_Child_Gap
                )],
            .font_size =
                style->linear_properties[s][LINEAR_PROPERTY_ENUM_RAW_INDEX(
                    Element_Property_Font_Size
                )],
          },
      .colors =
          {
            .background =
                style->color_properties[s][COLOR_PROPERTY_ENUM_RAW_INDEX(
                    Element_Property_Background_Color
                )],
            .text = style->color_properties[s][COLOR_PROPERTY_ENUM_RAW_INDEX(
                Element_Property_Text_Color
            )],
            .image = style->color_properties[s][COLOR_PROPERTY_ENUM_RAW_INDEX(
                Element_Property_Image_Color
            )],
          },
      .variable_colors =
          {
            .border = style->variable_color_properties
                          [s][VARIABLE_COLOR_PROPERTY_ENUM_RAW_INDEX(
                              Element_Property_Border_Color
                          )],
          },
      .constraints = {
        .padding =
            style->constraint_properties[s][CONSTRAINT_PROPERTY_ENUM_RAW_INDEX(
                Element_Property_Padding
            )],
      }
    };
  }

  Element_State s = cache->target_state;
  f32 t = style->transitions[s].duration > 0.f
              ? min_f32(
                    cache->transition_time / style->transitions[s].duration, 1.f
                )
              : 1.f;

  Element_Computed_Properties result = {
    .linears =
        {
          .border = lerp_f32(
              cache->linear_properties
                  [LINEAR_PROPERTY_ENUM_RAW_INDEX(Element_Property_Border)],
              style->linear_properties
                  [s][LINEAR_PROPERTY_ENUM_RAW_INDEX(Element_Property_Border)],
              t
          ),
          .radius = lerp_f32(
              cache->linear_properties
                  [LINEAR_PROPERTY_ENUM_RAW_INDEX(Element_Property_Radius)],
              style->linear_properties
                  [s][LINEAR_PROPERTY_ENUM_RAW_INDEX(Element_Property_Radius)],
              t
          ),
          .child_gap = lerp_f32(
              cache->linear_properties
                  [LINEAR_PROPERTY_ENUM_RAW_INDEX(Element_Property_Child_Gap)],
              style->linear_properties[s][LINEAR_PROPERTY_ENUM_RAW_INDEX(
                  Element_Property_Child_Gap
              )],
              t
          ),
          .font_size = lerp_f32(
              cache->linear_properties
                  [LINEAR_PROPERTY_ENUM_RAW_INDEX(Element_Property_Font_Size)],
              style->linear_properties[s][LINEAR_PROPERTY_ENUM_RAW_INDEX(
                  Element_Property_Font_Size
              )],
              t
          ),
        },
    .colors =
        {
          .background = color_lerp(
              cache->color_properties[COLOR_PROPERTY_ENUM_RAW_INDEX(
                  Element_Property_Background_Color
              )],
              style->color_properties[s][COLOR_PROPERTY_ENUM_RAW_INDEX(
                  Element_Property_Background_Color
              )],
              t
          ),
          .text = color_lerp(
              cache->color_properties
                  [COLOR_PROPERTY_ENUM_RAW_INDEX(Element_Property_Text_Color)],
              style->color_properties[s][COLOR_PROPERTY_ENUM_RAW_INDEX(
                  Element_Property_Text_Color
              )],
              t
          ),
          .image = color_lerp(
              cache->color_properties
                  [COLOR_PROPERTY_ENUM_RAW_INDEX(Element_Property_Image_Color)],
              style->color_properties[s][COLOR_PROPERTY_ENUM_RAW_INDEX(
                  Element_Property_Image_Color
              )],
              t
          ),
        },
    .variable_colors =
        {
          .border = variable_color_lerp(
              cache->variable_color_properties
                  [VARIABLE_COLOR_PROPERTY_ENUM_RAW_INDEX(
                      Element_Property_Border_Color
                  )],
              style->variable_color_properties
                  [s][VARIABLE_COLOR_PROPERTY_ENUM_RAW_INDEX(
                      Element_Property_Border_Color
                  )],
              t
          ),
        },
    .constraints = {
      .padding = element_constraint_lerp(
          cache->constraint_properties
              [CONSTRAINT_PROPERTY_ENUM_RAW_INDEX(Element_Property_Padding)],
          style->constraint_properties
              [s][CONSTRAINT_PROPERTY_ENUM_RAW_INDEX(Element_Property_Padding)],
          t
      ),
    },
  };

  return result;
}

///////////////////////
// Element Processing
///////////////////////
static Element_Events
process_element_events(Element *element, Element_Cached_Info *cached_info);
static void
process_element_commands(Element *element, Element_Cached_Info *cached_info);

static void process_element(Element *element) {
  if (!(element->flags & Element_Flag_Visible)) {
    return;
  }

  Element_Cached_Info *cache = open_map_get(_ctx->element_cache, element->id);

  // TODO(nico): generate stable ids for new elements

  if (cache == nullptr) {
    Element_State s = Element_State_Enter;
    Element_Cached_Info new_cache = {
      .computed_rect = element->computed_rect,
      .last_touched = _ctx->frame_counter,
      .target_state = Element_State_Normal,
    };

    memcpy(
        new_cache.linear_properties,
        element->style.linear_properties[s],
        sizeof(new_cache.linear_properties)
    );
    memcpy(
        new_cache.color_properties,
        element->style.color_properties[s],
        sizeof(new_cache.color_properties)
    );
    memcpy(
        new_cache.variable_color_properties,
        element->style.variable_color_properties[s],
        sizeof(new_cache.variable_color_properties)
    );
    memcpy(
        new_cache.constraint_properties,
        element->style.constraint_properties[s],
        sizeof(new_cache.constraint_properties)
    );

    open_map_set(_ctx->element_cache, element->id, new_cache);
    cache = open_map_get(_ctx->element_cache, element->id);
  }

  process_element_events(element, cache);
  process_element_commands(element, cache);

  Element_Iterator it = element_iterator(element->first_child);
  while (element_has_next(&it)) {
    Element *child = element_next(&it);
    process_element(child);
  }
}

static Element_Events
process_element_events(Element *element, Element_Cached_Info *cache) {
  cache->last_touched = _ctx->frame_counter;
  cache->transition_time = min_f32(
      cache->transition_time + _ctx->dt,
      element->style.transitions[cache->target_state].duration
  );
  cache->computed_rect = element->computed_rect;

  if (element->flags & Element_Flag_Ignore_Events) {
    return 0;
  }

  cache->previous_events = cache->events;
  cache->events = 0;

  bool32 hovered =
      rect_point_in(element->computed_rect, _ctx->m_pos.x, _ctx->m_pos.y);
  _ctx->m_over_ui |= hovered;

  if (hovered) {
    cache->events |= Element_Event_Hovered;
  }

  if (element->flags & Element_Flag_Interactive && hovered) {
    // NOTE(nico): This is weird, only left click make an element active?
    if (_ctx->m_left.just_pressed) {
      _ctx->active_element = element;
      cache->events |= Element_Event_Left_Clicked;
    }

    if (_ctx->m_right.just_pressed) {
      cache->events |= Element_Event_Right_Clicked;
    }
  }

  if (cache->target_state == Element_State_Enter &&
      cache->transition_time >=
          element->style.transitions[Element_State_Enter].duration) {
    cache->events |= Element_Event_Entered;
  }

  // TODO(nico): Handle Drag and Input
  Element_State previous_state =
      element_state_derive_from_events(cache->previous_events);
  Element_State state = element_state_derive_from_events(cache->events);

  if (state != previous_state) {
    cache->target_state = state;
    cache->transition_time = 0.f;
  }

  return cache->events;
}

static void
process_element_commands(Element *element, Element_Cached_Info *cached_info) {
  if (!(element->flags & Element_Flag_Visible)) {
    return;
  }

  Element_Computed_Properties style =
      process_element_computed_properties(&element->style, cached_info);

  if (element->flags & Element_Flag_Render_Background) {
    element_render_list_push(
        &_ctx->commands,
        &(Element_Render_Command){
          .kind = Element_Render_Command_Rectangle,
          .rectangle = {
            .rect = element->computed_rect,
            .radius = style.linears.radius,
            .border = style.linears.border,
            .color = style.colors.background,
            .border_color = style.variable_colors.border,
          },
        }
    );
  }

  if (element->flags & Element_Flag_Render_Text) {
    element_render_list_push(
        &_ctx->commands,
        &(Element_Render_Command){
          .kind = Element_Render_Command_Text,
          .text = {
            .origin = vec2(element->computed_rect.x, element->computed_rect.y),
            .chars = element->text,
            .font =
                {
                  .data = element->style.font_data,
                  .size = style.linears.font_size,
                },
            .color = style.colors.text
          },
        }
    );
  }

  if (element->flags & Element_Flag_Render_Image) {
    element_render_list_push(
        &_ctx->commands,
        &(Element_Render_Command){
          .kind = Element_Render_Command_Image,
          .image = {
            .dst_rect = element->computed_rect,
            .src_rect = element->style.image_options.source_rect,
            .source = element->image,
            .color = style.colors.image,
            .horizontal_flip = element->style.image_options.horizontal_flip,
            .vertical_flip = element->style.image_options.vertical_flip,
          }
        }
    );
  }

  if (element->flags & Element_Flag_Render_Custom) {
    element_render_list_push(
        &_ctx->commands,
        &(Element_Render_Command){
          .kind = Element_Render_Command_Custom,
          .custom = {
            .rect = element->computed_rect,
            .callback = element->content_proc,
            .data = element->content_data,
          },
        }
    );
  }
}

///////////////////////
// Layouting algorithm
///////////////////////
static void calculate_element_size(Element *element) {
  Element_Cached_Info *cache = open_map_get(_ctx->element_cache, element->id);
  // NOTE(nico): how the fuck is this not crashing on the first frame

  Element_Computed_Properties style =
      process_element_computed_properties(&element->style, cache);
  Element_Constraint padding = style.constraints.padding;

  // NOTE(nico): All the calculation based on animations is absolutely cursed
  if (element->flags & Element_Flag_Render_Text) {
    f32 font_size = style.linears.font_size;
    Element_Dimensions text_dimensions = _ctx->measure_text(
        (Element_Font){.data = element->style.font_data, .size = font_size},
        element->text
    );
    element->computed_rect.width = text_dimensions.width;
    element->computed_rect.height = text_dimensions.height;
  }

  if (element->flags & Element_Flag_Render_Image) {
    element->computed_rect.width =
        element->style.image_options.source_rect.width;
    element->computed_rect.height =
        element->style.image_options.source_rect.height;
  }

  if (element->sizing.width.kind == Element_Sizing_Fixed) {
    element->computed_rect.width = element->sizing.width.value;
  } else {
    element->computed_rect.width =
        element->computed_rect.width + padding.left + padding.right;
  }

  if (element->sizing.height.kind == Element_Sizing_Fixed) {
    element->computed_rect.height = element->sizing.height.value;
  } else {
    element->computed_rect.height =
        element->computed_rect.height + padding.top + padding.bottom;
  }

  f32 computed_child_gap = style.linears.child_gap;
  f32 total_child_gap =
      (f32)(element->relative_child_count - 1) * computed_child_gap;
  switch (element->layout) {
  case Element_Layout_Kind_None:
    break;
  case Element_Layout_Kind_Row:
    if (element->sizing.width.kind != Element_Sizing_Fixed) {
      element->computed_rect.width += total_child_gap;
    }
    break;
  case Element_Layout_Kind_Column:
    if (element->sizing.height.kind != Element_Sizing_Fixed) {
      element->computed_rect.height += total_child_gap;
    }
    break;
  }
}

static void position_absolute_element(Element *element) {
  Element_Position position = element->position;

  f32 parent_x = 0.f, parent_y = 0.f;
  if (element->parent != nullptr) {
    Element *parent = element->parent;

    switch (position.absolute_anchor) {
    case Element_Position_Anchor_Top_Left:
      parent_x = parent->computed_rect.x;
      parent_y = parent->computed_rect.y;
      break;
    case Element_Position_Anchor_Top_Right:
      // FIXME(nico): that seems wrong? Depending on the layouting order, if
      // this is before size calculation, it will bleed out of the parent
      // container
      parent_x = parent->computed_rect.x + parent->computed_rect.width;
      parent_y = parent->computed_rect.y;
      break;
    case Element_Position_Anchor_Bottom_Left:
      // FIXME(nico): Same here, that seems wrong
      parent_x = parent->computed_rect.x;
      parent_y = parent->computed_rect.y + parent->computed_rect.height;
      break;
    case Element_Position_Anchor_Bottom_Right:
      parent_x = parent->computed_rect.x + parent->computed_rect.width;
      parent_y = parent->computed_rect.y + parent->computed_rect.height;
      break;
    }
  }

  f32 offset_x =
      position.pixel_offset.x +
      (element->computed_rect.width * (position.percent_offset.x / 100.f));
  f32 offset_y =
      position.pixel_offset.y +
      (element->computed_rect.height * (position.percent_offset.y / 100.f));

  element->computed_rect.x = parent_x + offset_x;
  element->computed_rect.y = parent_y + offset_y;

  // TODO(nico): make the element fit the screen if the flag is present (the
  // flag doesn't exist yet)
}

static f32
grow_children_along_axis(Element *element, Element_Axis axis, f32 total_size) {
  f32 remaining_size = total_size;
  usize grow_count = 0;

  Element_Iterator it = element_iterator(element->first_child);
  while (element_has_next(&it)) {
    Element *child = element_next(&it);
    Element_Sizing sizing = axis == Element_Axis_Horizontal
                                ? child->sizing.width
                                : child->sizing.height;
    if (sizing.kind == Element_Sizing_Grow) {
      grow_count += 1;
    }
  }

  if (grow_count == 0) {
    return remaining_size;
  }

  while (remaining_size > 0) {
    f32 smallest = INFINITY;
    f32 second_smallest = INFINITY;
    f32 grow_value = remaining_size;

    it = element_iterator(element->first_child);
    while (element_has_next(&it)) {
      Element *child = element_next(&it);
      Element_Sizing sizing = axis == Element_Axis_Horizontal
                                  ? child->sizing.width
                                  : child->sizing.height;
      if (sizing.kind != Element_Sizing_Grow)
        continue;
      f32 size = axis == Element_Axis_Horizontal ? child->computed_rect.width
                                                 : child->computed_rect.height;
      if (size < smallest) {
        second_smallest = smallest;
        smallest = size;
      }
      if (size > smallest) {
        second_smallest = fminf(second_smallest, size);
        grow_value = second_smallest - smallest;
      }
    }

    grow_value = fminf(grow_value, remaining_size / (f32)grow_count);

    it = element_iterator(element->first_child);
    while (element_has_next(&it)) {
      Element *child = element_next(&it);
      Element_Sizing sizing = axis == Element_Axis_Horizontal
                                  ? child->sizing.width
                                  : child->sizing.height;
      if (sizing.kind != Element_Sizing_Grow)
        continue;
      if (axis == Element_Axis_Horizontal) {
        if (child->computed_rect.width == smallest) {
          child->computed_rect.width += grow_value;
          remaining_size -= grow_value;
        }
      } else {
        if (child->computed_rect.height == smallest) {
          child->computed_rect.height += grow_value;
          remaining_size -= grow_value;
        }
      }
    }
  }

  return remaining_size;
}

/**
 * @brief
 * @param element
 * @param axis
 * @param total_size
 */
static void
grow_children_across_axis(Element *element, Element_Axis axis, f32 total_size) {
  Element_Iterator it = element_iterator(element->first_child);
  while (element_has_next(&it)) {
    Element *child = element_next(&it);

    switch (axis) {
    case Element_Axis_Horizontal:
      if (child->sizing.width.kind == Element_Sizing_Grow) {
        child->computed_rect.width = total_size;
      }
      break;
    case Element_Axis_Vertical:
      if (child->sizing.height.kind == Element_Sizing_Grow) {
        child->computed_rect.height = total_size;
      }
      break;
    }
  }
}

/**
 * @brief
 * @param
 */
static void layout_children(Element *element) {
  if (element->child_count == 0) {
    return;
  }

  Element_Cached_Info *cache = open_map_get(_ctx->element_cache, element->id);

  Element_Computed_Properties style =
      process_element_computed_properties(&element->style, cache);
  Element_Constraint padding = style.constraints.padding;

  f32 start_x = element->computed_rect.x + padding.left;
  f32 start_y = element->computed_rect.y + padding.top;

  f32 content_width =
      element->computed_rect.width - (padding.left + padding.right);
  f32 content_height =
      element->computed_rect.height - (padding.top + padding.bottom);

  switch (element->layout) {
  case Element_Layout_Kind_None:
    break;
  case Element_Layout_Kind_Row: {
    content_width -=
        (f32)(element->relative_child_count - 1) * style.linears.child_gap;
    f32 remaining_width = content_width;

    Element_Iterator it = element_iterator(element->first_child);
    while (element_has_next(&it)) {
      Element *child = element_next(&it);
      if (child->position.kind == Element_Position_Relative) {
        remaining_width -= child->computed_rect.width;
      }
    }

    remaining_width = grow_children_along_axis(
        element, Element_Axis_Horizontal, remaining_width
    );
    grow_children_across_axis(element, Element_Axis_Vertical, content_height);

    f32 current_x = start_x;
    if (element->alignment.horizontal == Element_Alignment_Center) {
      current_x += remaining_width * 0.5f;
    } else if (element->alignment.horizontal == Element_Alignment_End) {
      current_x += remaining_width;
    } else if (
        element->alignment.horizontal == Element_Alignment_Space_Evenly
    ) {
      current_x += remaining_width / (f32)(element->relative_child_count + 1);
    }

    it = element_iterator(element->first_child);
    while (element_has_next(&it)) {
      Element *child = element_next(&it);

      switch (child->position.kind) {
      case Element_Position_Relative: {
        child->computed_rect.x = current_x;
        child->computed_rect.y = start_y;

        if (element->alignment.vertical == Element_Alignment_Center) {
          child->computed_rect.y +=
              (content_height - child->computed_rect.height) * 0.5f;
        } else if (element->alignment.vertical == Element_Alignment_End) {
          child->computed_rect.y +=
              content_height - child->computed_rect.height;
        }

        current_x += child->computed_rect.width;
        switch (element->alignment.horizontal) {
        case Element_Alignment_Space_Evenly:
          current_x +=
              remaining_width / (f32)(element->relative_child_count + 1);
          break;
        case Element_Alignment_Space_Between:
          if (element->relative_child_count > 1) {
            current_x +=
                remaining_width / (f32)(element->relative_child_count - 1);
          }
          break;
        case Element_Alignment_Start:
        case Element_Alignment_Center:
        case Element_Alignment_End:
        default:
          if (it.iteration < element->child_count) {
            current_x += style.linears.child_gap;
          }
          break;
        }

      } break;
      case Element_Position_Absolute:
        position_absolute_element(child);
        break;
      }

      if (child->child_count > 0) {
        layout_children(child);
      }
    }
  } break;
  case Element_Layout_Kind_Column: {
    content_height -=
        (f32)(element->relative_child_count - 1) * style.linears.child_gap;

    f32 remaining_height = content_height;

    Element_Iterator it = element_iterator(element->first_child);
    while (element_has_next(&it)) {
      Element *child = element_next(&it);

      if (child->position.kind == Element_Position_Relative) {
        remaining_height -= child->computed_rect.height;
      }
    }

    remaining_height = grow_children_along_axis(
        element, Element_Axis_Vertical, remaining_height
    );
    grow_children_across_axis(element, Element_Axis_Horizontal, content_width);

    f32 current_y = start_y;
    if (element->alignment.vertical == Element_Alignment_Center) {
      current_y += remaining_height * 0.5f;
    } else if (element->alignment.vertical == Element_Alignment_End) {
      current_y += remaining_height;
    } else if (element->alignment.vertical == Element_Alignment_Space_Evenly) {
      current_y += remaining_height / (f32)(element->relative_child_count + 1);
    }

    it = element_iterator(element->first_child);
    while (element_has_next(&it)) {
      Element *child = element_next(&it);

      switch (child->position.kind) {
      case Element_Position_Relative: {
        child->computed_rect.x = start_x;
        child->computed_rect.y = current_y;

        if (element->alignment.horizontal == Element_Alignment_Center) {
          child->computed_rect.x +=
              (content_width - child->computed_rect.width) * 0.5f;
        } else if (element->alignment.horizontal == Element_Alignment_End) {
          child->computed_rect.x += content_width - child->computed_rect.width;
        }

        current_y += child->computed_rect.height;
        switch (element->alignment.vertical) {
        case Element_Alignment_Space_Evenly:
          current_y +=
              remaining_height / (f32)(element->relative_child_count + 1);
          break;
        case Element_Alignment_Space_Between:
          if (element->relative_child_count > 1) {
            current_y +=
                remaining_height / (f32)(element->relative_child_count - 1);
          }
          break;
        case Element_Alignment_Start:
        case Element_Alignment_Center:
        case Element_Alignment_End:
        default:
          if (it.iteration < element->child_count) {
            current_y += style.linears.child_gap;
          }
          break;
        }

      } break;
      case Element_Position_Absolute:
        position_absolute_element(child);
        break;
      }

      if (child->child_count > 0) {
        layout_children(child);
      }
    }
  } break;
  }
}

///////////////////////
// Public API
///////////////////////
// FIXME(nico): Provide a way to query the minimum size the context will need
// FIXME(nico): [19-08-26] No cleanup on error path
Element_Error init_element_context(
    Element_Context *ctx, Element_Context_Create_Info *info, Allocator allocator
) {
  usize init_cap = info->init_cap ? info->init_cap : 256;

  ctx->allocator = allocator;
  ctx->flags = info->flags;
  ctx->elements = or_return(
      make_element_list(init_cap, allocator),
      Element_Error_Failed_To_Initialize_Context
  );
  ctx->element_roots = or_return(
      make_element_ptr_list(init_cap, allocator),
      Element_Error_Failed_To_Initialize_Context
  );
  ctx->element_stack = or_return(
      make_element_ptr_list(init_cap, allocator),
      Element_Error_Failed_To_Initialize_Context
  );
  ctx->element_cache = make_open_map(
      u64, Element_Cached_Info, init_cap, hash_fnv1a, element_id_eq, allocator
  );

  usize cmd_cap = init_cap * 2;
  ctx->commands = or_return(
      make_element_render_list(cmd_cap, allocator),
      Element_Error_Failed_To_Initialize_Context
  );
  ;

  // Callbacks
  ctx->measure_text = info->measure_text_proc;

  return Element_Error_None;
}

Element_Error destroy_element_context(Element_Context *ctx) {
  delete_element_list(&ctx->elements);
  delete_element_ptr_list(&ctx->element_roots);
  delete_element_ptr_list(&ctx->element_stack);
  delete_open_map(ctx->element_cache);

  delete_element_render_list(&ctx->commands);

  return Element_Error_None;
}

void set_context_current(Element_Context *ctx) {
  _ctx = ctx;
}

void set_screen_state(Element_Context *ctx, Element_Dimensions dimensions) {
  ctx->screen_dimensions = dimensions;
}

void set_pointer_state(
    Element_Context *ctx, Vec2 m_pos, bool32 m_left, bool32 m_right
) {
  ctx->m_previous_pos = ctx->m_pos;
  ctx->m_pos = m_pos;
  ctx->m_delta = (Vec2){
    .x = m_pos.x - ctx->m_previous_pos.x,
    .y = m_pos.y - ctx->m_previous_pos.y,
  };

  ctx->m_left = (Element_Input_Info){
    .previously_pressed = ctx->m_left.pressed,
    .previously_just_pressed = ctx->m_left.just_pressed,
    .previously_just_released = ctx->m_left.just_released,
    .pressed = (bool8)m_left,
    .just_pressed = m_left && !ctx->m_left.pressed,
    .just_released = !m_left && ctx->m_left.pressed,
  };
  ctx->m_right = (Element_Input_Info){
    .previously_pressed = ctx->m_right.pressed,
    .previously_just_pressed = ctx->m_right.just_pressed,
    .previously_just_released = ctx->m_right.just_released,
    .pressed = (bool8)m_right,
    .just_pressed = m_right && !ctx->m_right.pressed,
    .just_released = !m_right && ctx->m_right.pressed,
  };

  ctx->m_previously_over_ui = ctx->m_over_ui;
  ctx->m_over_ui = false;
}

void set_delta_time(Element_Context *ctx, f32 dt) {
  ctx->dt = dt;
}

void push_element_id_seed(u64 seed) {
  assert(_ctx->id_seed_stack_len < ELEMENT_ID_STACK_CAP);

  u64 top = _ctx->id_seed_stack_len > 0
                ? _ctx->id_seed_stack[_ctx->id_seed_stack_len - 1]
                : 0;
  _ctx->id_seed_stack[_ctx->id_seed_stack_len++] = element_id_mix(top, seed);
}

void pop_element_id_seed() {
  // NOTE(nico): this probably overflows anyway
  if (_ctx->id_seed_stack_len == 0) {
    return;
  }
  _ctx->id_seed_stack_len -= 1;
}

// /**
//  * @brief ONLY use during a begin_ui/end_ui scope or after manually setting
//  * the context
//  * @return return the pointer position stored by the context
//  */
// static Vec2 get_mouse_delta() {
//   return _ctx->m_delta;
// }

// /**
//  * @brief ONLY use during a begin_ui/end_ui scope or after manually setting
//  * the context
//  * @return return the pointer delta stored by the context
//  */
// static Vec2 get_mouse_position() {
//   return _ctx->m_pos;
// }

void begin_ui(Element_Context *ctx) {
  ctx->frame_counter += 1;
  ctx->elements.len = 0;
  ctx->element_roots.len = 0;
  ctx->element_stack.len = 0;
  ctx->commands.len = 0;
  ctx->id_seed_stack_len = 0;

  if (ctx->m_left.just_released) {
    ctx->active_element = nullptr;
  }

  _ctx = ctx;
}

Element_Render_Command_Buffer end_ui(Element_Context *ctx) {
  for (usize i = 0; i < ctx->element_roots.len; i += 1) {
    Element *root = ctx->element_roots.items[i];

    switch (root->position.kind) {
    case Element_Position_Relative:
      root->computed_rect.x = 0;
      root->computed_rect.y = 0;
      break;
    case Element_Position_Absolute:
      position_absolute_element(root);
      break;
    }

    if (root->layout != Element_Layout_Kind_None) {
      layout_children(root);
    }

    process_element(root);
  }

  Open_Map_Iterator it = open_map_iterator(ctx->element_cache);
  while (open_map_has_next(&it)) {
    open_map_next(&it);
    Element_Cached_Info *cached_info = it.value;

    // NOTE(nico): pretty arbitrary choice of frames. Could expose this to the
    // context options
    if (ctx->frame_counter - cached_info->last_touched >= 30) {
      assert(open_map_remove_raw(ctx->element_cache, it.key));
    }
  }

  _ctx = nullptr;

  return (Element_Render_Command_Buffer){
    .items = ctx->commands.items,
    .len = ctx->commands.len,
  };
}

void begin_element_impl(
    Element_Create_Info *info, Element_Flags flags, u32 callsite
) {
  if (_ctx->flags & Element_Context_Flag_Validation_Layer) {
    if (flags & Element_Flag_Render_Text && !info->style.font_data) {
      assert(false);
      // TODO(nico): Log to the define output
    }
  }

  Element *parent = _ctx->current_element;
  u64 parent_id = parent != nullptr ? parent->id : 0;
  u64 user_id = info->id.some ? info->id.value : (u64)callsite;
  u64 seed = _ctx->id_seed_stack_len > 0
                 ? _ctx->id_seed_stack[_ctx->id_seed_stack_len - 1]
                 : 0;

  u64 id = element_id_mix(element_id_mix(parent_id, seed), user_id);

  // FIXME(nico): [19-08-26] Need to handle this cleanly. But too lazy to do it
  // in this refactor
  usize element_index = unwrap(element_list_push(
      &_ctx->elements,
      &(Element){
        .id = id,
        .flags = flags,
        .layout = info->layout,
        .sizing = {.width = info->sizing.width, .height = info->sizing.height},
        .alignment =
            {
              .horizontal = info->alignment.horizontal,
              .vertical = info->alignment.vertical,
            },
        .style = element_style_to_internal(&info->style),
        .position = info->position,
        .text = info->text,
        .image = info->image,
        .content_proc = info->content_proc,
        .content_data = info->content_data,
      }
  ));
  Element *element = &_ctx->elements.items[element_index];

  if (parent != nullptr) {
    element->parent = parent;

    parent->child_count += 1;
    if (info->position.kind == Element_Position_Relative) {
      parent->relative_child_count += 1;
    }

    if (parent->first_child == nullptr) {
      parent->first_child = element;
    } else {
      parent->last_child->next = element;
    }
    parent->last_child = element;
  } else {
    unwrap(element_ptr_list_push(&_ctx->element_roots, &element));
  }

  unwrap(element_ptr_list_push(&_ctx->element_stack, &element));
  _ctx->current_element = element;
}

void end_element() {
  if (_ctx->current_element == nullptr) {
    assert(false);
  }

  Element_Ptr_List_Pop_Option element_opt =
      element_ptr_list_pop(&_ctx->element_stack);
  assert(element_opt.some);
  Element *element = element_opt.value;

  calculate_element_size(element);

  if (element->parent != nullptr &&
      element->position.kind == Element_Position_Relative) {
    Element *parent = element->parent;

    switch (parent->layout) {
    case Element_Layout_Kind_None:
      break;
    case Element_Layout_Kind_Row:
      parent->computed_rect.width += element->computed_rect.width;
      parent->computed_rect.height =
          fmaxf(parent->computed_rect.height, element->computed_rect.height);
      break;
    case Element_Layout_Kind_Column:
      parent->computed_rect.width =
          fmaxf(parent->computed_rect.width, element->computed_rect.width);
      parent->computed_rect.height += element->computed_rect.height;
      break;
    }
  }

  usize stack_len = _ctx->element_stack.len;
  if (stack_len > 0) {
    _ctx->current_element = _ctx->element_stack.items[stack_len - 1];
  } else {
    _ctx->current_element = nullptr;
  }
}

// FIXME(nico): no null pointer guard
Element_Client_Info get_current_element() {
  u64 handle = _ctx->current_element->id;
  Element_Cached_Info *cached_info = open_map_get(_ctx->element_cache, handle);

  if (cached_info == nullptr) {
    return (Element_Client_Info){0};
  }

  return (Element_Client_Info){
    .computed_rect = cached_info->computed_rect,
    .events = cached_info->events,
  };
}