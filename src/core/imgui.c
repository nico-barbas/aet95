// #include "core/imgui.h"

// #include "core/allocator.h"
// #include "core/map.h"
// #include "core/types.h"

// #include <assert.h>
// #include <math.h>

// #define RESERVED_ELEMENT_HASH 1

// Element_Context *_ctx;

// ///////////////////////
// // Helpers
// ///////////////////////
// static f32 ease_f32(f32 start, f32 end, f32 t, Element_Ease_Fn fn) {
//   f32 eased_t;
//   switch (fn) {
//   case Element_Ease_Fn_Linear:
//     eased_t = t;
//     break;
//   case Element_Ease_Fn_In:
//     eased_t = t * t;
//     break;
//   case Element_Ease_Fn_Out: {
//     f32 inv = 1.f - t;
//     eased_t = 1.f - inv * inv;
//   } break;
//   case Element_Ease_Fn_In_Out: {
//     f32 inv = -2.f * t + 2.f;
//     eased_t = t < 0.5f ? 2.f * t * t : 1.f - inv * inv / 2.f;
//   } break;
//   case Element_Ease_Fn_In_Cubic:
//     eased_t = t * t * t;
//     break;
//   case Element_Ease_Fn_Out_Cubic: {
//     f32 inv = 1.f - t;
//     eased_t = 1.f - inv * inv * inv;
//   } break;
//   case Element_Ease_Fn_In_Out_Cubic: {
//     f32 inv = -2.f * t + 2.f;
//     eased_t = t < 0.5f ? 4.f * t * t * t : 1.f - inv * inv * inv / 2.f;
//   } break;
//   case Element_Ease_Fn_Back_In: {
//     // c1 = 1.70158, c3 = c1 + 1
//     eased_t = 2.70158f * t * t * t - 1.70158f * t * t;
//   } break;
//   case Element_Ease_Fn_Back_Out: {
//     f32 inv = t - 1.f;
//     eased_t = 1.f + 2.70158f * inv * inv * inv + 1.70158f * inv * inv;
//   } break;
//   default:
//     eased_t = t;
//     break;
//   }
//   return start + (end - start) * eased_t;
// }

// static Element_Color blend_element_color(
//     Element_Color a, Element_Color b, f32 t, Element_Ease_Fn fn
// ) {
//   return (Element_Color){
//     .r = ease_f32(a.r, b.r, t, fn),
//     .g = ease_f32(a.g, b.g, t, fn),
//     .b = ease_f32(a.b, b.b, t, fn),
//     .a = ease_f32(a.a, b.a, t, fn),
//   };
// }

// static f32 resolve_f32(
//     Element_Property_Animation *animation,
//     Element_Style_Property *property,
//     Element_Property which,
//     Element_State target
// ) {
//   if (_ctx->flags & Element_Context_Flag_Validation_Layer) {
//     assert(!element_property_is_color(which));
//   }

//   f32 to = property->values.f32[target];
//   if (property->duration == 0.f || animation->time >= property->duration) {
//     return to;
//   }

//   f32 t = animation->time / property->duration;
//   return ease_f32(animation->from.f32, to, t, property->ease);
// }

// static Element_Color resolve_color(
//     Element_Property_Animation *animation,
//     Element_Style_Property *property,
//     Element_Property which,
//     Element_State target
// ) {
//   if (_ctx->flags & Element_Context_Flag_Validation_Layer) {
//     assert(element_property_is_color(which));
//   }

//   Element_Color to = property->values.color[target];
//   if (property->duration == 0.f || animation->time >= property->duration) {
//     return to;
//   }

//   f32 t = animation->time / property->duration;
//   return blend_element_color(animation->from.color, to, t, property->ease);
// }

// static Element_State element_state_from_events(Element_Events e) {
//   if (e & (Element_Event_Left_Clicked | Element_Event_Right_Clicked))
//     return Element_State_Clicked;

//   if (e & Element_Event_Hovered)
//     return Element_State_Hovered;

//   return Element_State_Clear;
// }

// /**
//  * @brief Multiply each field by Knuth's golden-ratio constant and XOR them.
//  * Should provide good distribution
//  * @param h Pointer to the raw key to hash
//  * @param size Size of the key. Not used here
//  * @return Hashed integer to index into an Open_Map
//  */
// static usize element_id_hash(void *h, usize size) {
//   (void)size;
//   Element_Id *_h = (Element_Id *)h;
//   return (usize)_h->index * 2654435761u ^ (usize)_h->hash * 2246822519u;
// }

// static bool32 element_id_eq(void *h1, void *h2) {
//   Element_Id *_h1 = (Element_Id *)h1;
//   Element_Id *_h2 = (Element_Id *)h2;

//   return _h1->index == _h2->index && _h1->hash == _h2->hash;
// }

// static bool32 point_in_rect(Element_Rectangle r, Element_Point p) {
//   return p.x > r.x && p.x < r.x + r.width && p.y > r.y && p.y < r.y +
//   r.height;
// }

// static Element_Iterator element_iterator(Element *element) {
//   return (Element_Iterator){
//     .element = element,
//     .iteration = 0,
//   };
// }

// static bool32 element_has_next(Element_Iterator *it) {
//   return it->element != nullptr;
// }

// static Element *element_next(Element_Iterator *it) {
//   Element *item = it->element;
//   it->iteration += 1;
//   it->element = it->element->next;
//   return item;
// }

// ///////////////////////
// // Element Processing
// ///////////////////////
// static Element_Events
// process_element_events(Element *element, Element_Cached_Info *cached_info);
// static void
// process_element_commands(Element *element, Element_Cached_Info *cached_info);

// static void process_element(Element *element) {
//   if (!(element->flags & Element_Flag_Visible)) {
//     return;
//   }

//   Element_Cached_Info *cached_info =
//       open_map_get(_ctx->element_cache, element->info.id);

//   // NOTE(nico): What?
//   Element_Id id = {0};
//   id = element->info.id;
//   // if (element->info.id.hash != RESERVED_ELEMENT_HASH && !cached_info) {
//   // } else {
//   //   id = element->info.id;
//   // }

//   element->info.id = id;

//   if (!cached_info) {
//     Element_Cached_Info new_info = {
//       .computed_rect = element->computed_rect,
//       .last_touched = _ctx->frame_counter,
//       .target_state = Element_State_Clear,
//     };

//     for (int i = 0; i < Element_Property_MAX; i++) {
//       Element_Style_Property *property = &element->style.properties[i];

//       new_info.animations[i].time = property->duration;

//       if (element_property_is_color((Element_Property)i)) {
//         new_info.animations[i].from.color =
//             property->values.color[Element_State_Clear];
//       } else {
//         new_info.animations[i].from.f32 =
//             property->values.f32[Element_State_Clear];
//       }
//     }

//     open_map_set(_ctx->element_cache, id, new_info);
//     cached_info = open_map_get(_ctx->element_cache, id);
//   }

//   process_element_events(element, cached_info);
//   process_element_commands(element, cached_info);

//   Element_Iterator it = element_iterator(element->first_child);
//   while (element_has_next(&it)) {
//     Element *child = element_next(&it);
//     process_element(child);
//   }
// }

// static Element_Events
// process_element_events(Element *element, Element_Cached_Info *cached_info) {
//   if (element->flags & Element_Flag_Ignore_Events) {
//     return 0;
//   }

//   cached_info->last_touched = _ctx->frame_counter;

//   bool32 hovered = point_in_rect(element->computed_rect, _ctx->m_pos);
//   _ctx->m_over_ui |= hovered;

//   cached_info->previous_events = cached_info->events;
//   cached_info->events = 0;

//   if (hovered) {
//     cached_info->events |= Element_Event_Hovered;
//   }

//   if (element->flags & Element_Flag_Interactive && hovered) {
//     // NOTE(nico): This is weird, only left click make an element active?
//     if (_ctx->m_left.just_pressed) {
//       _ctx->active_element = element;
//       cached_info->events |= Element_Event_Left_Clicked;
//     }

//     if (_ctx->m_right.just_pressed) {
//       cached_info->events |= Element_Event_Right_Clicked;
//     }
//   }

//   // NOTE(nico): Advance all property timers. No need to change internal
//   state.
//   // Setting the time to max duration makes t = 1 and we get the current
//   state
//   // value anyway
//   for (int i = 0; i < Element_Property_MAX; i++) {
//     Element_Property_Animation *animation = &cached_info->animations[i];
//     f32 duration = element->style.properties[i].duration;
//     animation->time += _ctx->dt;
//     if (animation->time > duration)
//       animation->time = duration;
//   }

//   // TODO(nico): Handle Drag and Input
//   Element_State previous_state =
//       element_state_from_events(cached_info->previous_events);
//   Element_State state = element_state_from_events(cached_info->events);

//   // On state change: snapshot current interpolated values as from, reset
//   timers if (state != previous_state) {

//     for (int i = 0; i < Element_Property_MAX; i++) {
//       Element_Property_Animation *animation = &cached_info->animations[i];
//       Element_Style_Property *property = &element->style.properties[i];

//       Element_Property prop = (Element_Property)i;
//       if (element_property_is_color(prop)) {
//         animation->from.color =
//             resolve_color(animation, property, prop,
//             cached_info->target_state);
//       } else {
//         animation->from.f32 =
//             resolve_f32(animation, property, prop,
//             cached_info->target_state);
//       }

//       animation->time = 0.f;
//     }
//     cached_info->target_state = state;
//   }

//   return cached_info->events;
// }

// static void
// process_element_commands(Element *element, Element_Cached_Info *cached_info)
// {
//   if (!(element->flags & Element_Flag_Visible)) {
//     return;
//   }

//   Element_Style *style = &element->style;
//   Element_State target_state = cached_info->target_state;

//   if (element->flags & Element_Flag_Render_Background) {
//     list_push(
//         _ctx->commands,
//         ((Element_Render_Command){
//           .kind = Element_Render_Command_Rectangle,
//           .variant.rectangle = {
//             .rect = element->computed_rect,
//             .radius = resolve_f32(
//                 &cached_info->animations[Element_Property_Radius],
//                 &style->properties[Element_Property_Radius],
//                 Element_Property_Radius,
//                 target_state
//             ),
//             .border = resolve_f32(
//                 &cached_info->animations[Element_Property_Border],
//                 &style->properties[Element_Property_Border],
//                 Element_Property_Border,
//                 target_state
//             ),
//             .color = resolve_color(
//                 &cached_info->animations[Element_Property_Background_Color],
//                 &style->properties[Element_Property_Background_Color],
//                 Element_Property_Background_Color,
//                 target_state
//             ),
//             .border_color = (Element_Border_Color){
//               .kind = Element_Border_Color_Uniform,
//               .variant.uniform = resolve_color(
//                   &cached_info->animations[Element_Property_Border_Color],
//                   &style->properties[Element_Property_Border_Color],
//                   Element_Property_Border_Color,
//                   target_state
//               ),
//             },
//           },
//         })
//     );
//   }

//   if (element->flags & Element_Flag_Render_Text) {
//     list_push(
//         _ctx->commands,
//         ((Element_Render_Command){
//           .kind = Element_Render_Command_Text,
//           .variant.text = {
//             .origin =
//                 {.x = element->computed_rect.x, .y =
//                 element->computed_rect.y},
//             .chars = element->info.text,
//             .font =
//                 {
//                   .data = style->font_data,
//                   .size = resolve_f32(
//                       &cached_info->animations[Element_Property_Font_Size],
//                       &style->properties[Element_Property_Font_Size],
//                       Element_Property_Font_Size,
//                       target_state
//                   ),
//                 },
//             .color = resolve_color(
//                 &cached_info->animations[Element_Property_Text_Color],
//                 &style->properties[Element_Property_Text_Color],
//                 Element_Property_Text_Color,
//                 target_state
//             ),
//           },
//         })
//     );
//   }

//   if (element->flags & Element_Flag_Render_Image) {
//     list_push(
//         _ctx->commands,
//         ((Element_Render_Command){
//           .kind = Element_Render_Command_Image,
//           .variant.image = {
//             .dst_rect = element->computed_rect,
//             .src_rect = style->image_options.source_rect,
//             .source = element->info.image,
//             .color = resolve_color(
//                 &cached_info->animations[Element_Property_Image_Color],
//                 &style->properties[Element_Property_Image_Color],
//                 Element_Property_Image_Color,
//                 target_state
//             ),
//             .horizontal_flip = style->image_options.horizontal_flip,
//             .vertical_flip = style->image_options.vertical_flip,
//           },
//         })
//     );
//   }
// }

// ///////////////////////
// // Layouting algorithm
// ///////////////////////
// static void calculate_element_size(Element *element) {
//   Element_Style *style = &element->style;
//   Element_Info *info = &element->info;
//   Element_Cached_Info *cached_info =
//       open_map_get(_ctx->element_cache, element->info.id);

//   if (element->flags & Element_Flag_Render_Text) {
//     f32 font_size = style->properties[Element_Property_Font_Size]
//                         .values.f32[Element_State_Clear];

//     if (cached_info != nullptr) {
//       font_size = resolve_f32(
//           &cached_info->animations[Element_Property_Font_Size],
//           &style->properties[Element_Property_Font_Size],
//           Element_Property_Font_Size,
//           cached_info->target_state
//       );
//     }

//     Element_Dimensions text_dimensions = _ctx->measure_text(
//         (Element_Font){.data = style->font_data, .size = font_size},
//         element->info.text
//     );
//     element->computed_rect.width = text_dimensions.width;
//     element->computed_rect.height = text_dimensions.height;
//   }

//   if (element->flags & Element_Flag_Render_Image) {
//     element->computed_rect.width = style->image_options.source_rect.width;
//     element->computed_rect.height = style->image_options.source_rect.height;
//   }

//   if (info->sizing.width.kind == Element_Sizing_Fixed) {
//     element->computed_rect.width = info->sizing.width.value;
//   } else {
//     element->computed_rect.width = element->computed_rect.width +
//                                    style->padding.left +
//                                    style->padding.right;
//   }

//   if (info->sizing.height.kind == Element_Sizing_Fixed) {
//     element->computed_rect.height = info->sizing.height.value;
//   } else {
//     element->computed_rect.height = element->computed_rect.height +
//                                     style->padding.top +
//                                     style->padding.bottom;
//   }

//   f32 computed_child_gap =
//       cached_info ? resolve_f32(
//                         &cached_info->animations[Element_Property_Child_Gap],
//                         &style->properties[Element_Property_Child_Gap],
//                         Element_Property_Child_Gap,
//                         cached_info->target_state
//                     )
//                   : style->properties[Element_Property_Child_Gap]
//                         .values.f32[Element_State_Clear];
//   f32 total_child_gap =
//       (f32)(element->relative_child_count - 1) * computed_child_gap;
//   switch (element->info.layout) {
//   case Element_Layout_Kind_None:
//     break;
//   case Element_Layout_Kind_Row:
//     if (info->sizing.width.kind != Element_Sizing_Fixed) {
//       element->computed_rect.width += total_child_gap;
//     }
//     break;
//   case Element_Layout_Kind_Column:
//     if (info->sizing.height.kind != Element_Sizing_Fixed) {
//       element->computed_rect.height += total_child_gap;
//     }
//     break;
//   }
// }

// /**
//  * @brief Position an element that uses Absolute positioning based on the
//  * position of its parent.
//  *
//  * FIXME(nico): This is a bit counter-intuitive to the way Web Devs are used
//  to.
//  * We do not retrace the entire graph to find the nearest "relative" parent
//  * element
//  * @param element The Element on which to operate
//  */
// static void position_absolute_element(Element *element) {
//   Element_Position position = element->info.position;

//   f32 parent_x = 0.f, parent_y = 0.f;
//   if (element->parent != nullptr) {
//     Element *parent = element->parent;

//     switch (position.anchor) {
//     case Element_Position_Anchor_Top_Left:
//       parent_x = parent->computed_rect.x;
//       parent_y = parent->computed_rect.y;
//       break;
//     case Element_Position_Anchor_Top_Right:
//       // FIXME(nico): that seems wrong? Depending on the layouting order, if
//       // this is before size calculation, it will bleed out of the parent
//       // container
//       parent_x = parent->computed_rect.x + parent->computed_rect.width;
//       parent_y = parent->computed_rect.y;
//       break;
//     case Element_Position_Anchor_Bottom_Left:
//       // FIXME(nico): Same here, that seems wrong
//       parent_x = parent->computed_rect.x;
//       parent_y = parent->computed_rect.y + parent->computed_rect.height;
//       break;
//     case Element_Position_Anchor_Bottom_Right:
//       parent_x = parent->computed_rect.x + parent->computed_rect.width;
//       parent_y = parent->computed_rect.y + parent->computed_rect.height;
//       break;
//     }
//   }

//   f32 offset_x = position.raw_offset.x + (element->computed_rect.width *
//                                           (position.percent_offset[0] /
//                                           100.f));
//   f32 offset_y = position.raw_offset.y + (element->computed_rect.height *
//                                           (position.percent_offset[1] /
//                                           100.f));

//   element->computed_rect.x = parent_x + offset_x;
//   element->computed_rect.y = parent_y + offset_y;

//   // TODO(nico): make the element fit the screen if the flag is present (the
//   // flag doesn't exist yet)
// }

// /**
//  * @brief Grow all the children of a given element to fit their constraints
//  *
//  * @param element The Element on which to operate
//  * @param axis The Axis on which to grow the children (usually dictated by
//  the
//  * element itself)
//  * @param total_size The total size available to grow the children
//  * @return The remaining size after growing. Most of the time 0
//  */
// static f32
// grow_children_along_axis(Element *element, Element_Axis axis, f32 total_size)
// {
//   f32 remaining_size = total_size;
//   usize grow_count = 0;

//   Element_Iterator it = element_iterator(element->first_child);
//   while (element_has_next(&it)) {
//     Element *child = element_next(&it);
//     Element_Sizing sizing = axis == Element_Axis_Horizontal
//                                 ? child->info.sizing.width
//                                 : child->info.sizing.height;
//     if (sizing.kind == Element_Sizing_Grow) {
//       grow_count += 1;
//     }
//   }

//   if (grow_count == 0) {
//     return remaining_size;
//   }

//   while (remaining_size > 0) {
//     f32 smallest = INFINITY;
//     f32 second_smallest = INFINITY;
//     f32 grow_value = remaining_size;

//     it = element_iterator(element->first_child);
//     while (element_has_next(&it)) {
//       Element *child = element_next(&it);
//       Element_Sizing sizing = axis == Element_Axis_Horizontal
//                                   ? child->info.sizing.width
//                                   : child->info.sizing.height;
//       if (sizing.kind != Element_Sizing_Grow)
//         continue;
//       f32 size = axis == Element_Axis_Horizontal ? child->computed_rect.width
//                                                  :
//                                                  child->computed_rect.height;
//       if (size < smallest) {
//         second_smallest = smallest;
//         smallest = size;
//       }
//       if (size > smallest) {
//         second_smallest = fminf(second_smallest, size);
//         grow_value = second_smallest - smallest;
//       }
//     }

//     grow_value = fminf(grow_value, remaining_size / (f32)grow_count);

//     it = element_iterator(element->first_child);
//     while (element_has_next(&it)) {
//       Element *child = element_next(&it);
//       Element_Sizing sizing = axis == Element_Axis_Horizontal
//                                   ? child->info.sizing.width
//                                   : child->info.sizing.height;
//       if (sizing.kind != Element_Sizing_Grow)
//         continue;
//       if (axis == Element_Axis_Horizontal) {
//         if (child->computed_rect.width == smallest) {
//           child->computed_rect.width += grow_value;
//           remaining_size -= grow_value;
//         }
//       } else {
//         if (child->computed_rect.height == smallest) {
//           child->computed_rect.height += grow_value;
//           remaining_size -= grow_value;
//         }
//       }
//     }
//   }

//   return remaining_size;
// }

// /**
//  * @brief
//  * @param element
//  * @param axis
//  * @param total_size
//  */
// static void
// grow_children_across_axis(Element *element, Element_Axis axis, f32
// total_size) {
//   Element_Iterator it = element_iterator(element->first_child);
//   while (element_has_next(&it)) {
//     Element *child = element_next(&it);

//     switch (axis) {
//     case Element_Axis_Horizontal:
//       if (child->info.sizing.width.kind == Element_Sizing_Grow) {
//         child->computed_rect.width = total_size;
//       }
//       break;
//     case Element_Axis_Vertical:
//       if (child->info.sizing.height.kind == Element_Sizing_Grow) {
//         child->computed_rect.height = total_size;
//       }
//       break;
//     }
//   }
// }

// /**
//  * @brief
//  * @param
//  */
// static void layout_children(Element *element) {
//   if (element->child_count == 0) {
//     return;
//   }

//   Element_Cached_Info *cached_info =
//       open_map_get(_ctx->element_cache, element->info.id);

//   Element_Info *info = &element->info;
//   Element_Style *style = &element->style;
//   Element_Padding padding = style->padding;

//   f32 start_x = element->computed_rect.x + padding.left;
//   f32 start_y = element->computed_rect.y + padding.top;

//   f32 content_width =
//       element->computed_rect.width - (padding.left + padding.right);
//   f32 content_height =
//       element->computed_rect.height - (padding.top + padding.bottom);

//   f32 computed_child_gap =
//       cached_info ? resolve_f32(
//                         &cached_info->animations[Element_Property_Child_Gap],
//                         &style->properties[Element_Property_Child_Gap],
//                         Element_Property_Child_Gap,
//                         cached_info->target_state
//                     )
//                   : style->properties[Element_Property_Child_Gap]
//                         .values.f32[Element_State_Clear];

//   switch (info->layout) {
//   case Element_Layout_Kind_None:
//     break;
//   case Element_Layout_Kind_Row: {
//     content_width -=
//         (f32)(element->relative_child_count - 1) * computed_child_gap;
//     f32 remaining_width = content_width;

//     Element_Iterator it = element_iterator(element->first_child);
//     while (element_has_next(&it)) {
//       Element *child = element_next(&it);
//       if (child->info.position.kind == Element_Position_Relative) {
//         remaining_width -= child->computed_rect.width;
//       }
//     }

//     remaining_width = grow_children_along_axis(
//         element, Element_Axis_Horizontal, remaining_width
//     );
//     grow_children_across_axis(element, Element_Axis_Vertical,
//     content_height);

//     f32 current_x = start_x;
//     if (info->alignment.horizontal == Element_Alignment_Center) {
//       current_x += remaining_width * 0.5f;
//     } else if (info->alignment.horizontal == Element_Alignment_End) {
//       current_x += remaining_width;
//     } else if (info->alignment.horizontal == Element_Alignment_Space_Evenly)
//     {
//       current_x += remaining_width / (f32)(element->relative_child_count +
//       1);
//     }

//     it = element_iterator(element->first_child);
//     while (element_has_next(&it)) {
//       Element *child = element_next(&it);

//       switch (child->info.position.kind) {
//       case Element_Position_Relative: {
//         child->computed_rect.x = current_x;
//         child->computed_rect.y = start_y;

//         if (info->alignment.vertical == Element_Alignment_Center) {
//           child->computed_rect.y +=
//               (content_height - child->computed_rect.height) * 0.5f;
//         } else if (info->alignment.vertical == Element_Alignment_End) {
//           child->computed_rect.y +=
//               content_height - child->computed_rect.height;
//         }

//         current_x += child->computed_rect.width;
//         switch (info->alignment.horizontal) {
//         case Element_Alignment_Space_Evenly:
//           current_x +=
//               remaining_width / (f32)(element->relative_child_count + 1);
//           break;
//         case Element_Alignment_Space_Between:
//           if (element->relative_child_count > 1) {
//             current_x +=
//                 remaining_width / (f32)(element->relative_child_count - 1);
//           }
//           break;
//         case Element_Alignment_Start:
//         case Element_Alignment_Center:
//         case Element_Alignment_End:
//         default:
//           if (it.iteration < element->child_count) {
//             current_x += computed_child_gap;
//           }
//           break;
//         }

//       } break;
//       case Element_Position_Absolute:
//         position_absolute_element(child);
//         break;
//       }

//       if (child->child_count > 0) {
//         layout_children(child);
//       }
//     }
//   } break;
//   case Element_Layout_Kind_Column: {
//     content_height -=
//         (f32)(element->relative_child_count - 1) * computed_child_gap;

//     f32 remaining_height = content_height;

//     Element_Iterator it = element_iterator(element->first_child);
//     while (element_has_next(&it)) {
//       Element *child = element_next(&it);

//       if (child->info.position.kind == Element_Position_Relative) {
//         remaining_height -= child->computed_rect.height;
//       }
//     }

//     remaining_height = grow_children_along_axis(
//         element, Element_Axis_Vertical, remaining_height
//     );
//     grow_children_across_axis(element, Element_Axis_Horizontal,
//     content_width);

//     f32 current_y = start_y;
//     if (info->alignment.vertical == Element_Alignment_Center) {
//       current_y += remaining_height * 0.5f;
//     } else if (info->alignment.vertical == Element_Alignment_End) {
//       current_y += remaining_height;
//     } else if (info->alignment.vertical == Element_Alignment_Space_Evenly) {
//       current_y += remaining_height / (f32)(element->relative_child_count +
//       1);
//     }

//     it = element_iterator(element->first_child);
//     while (element_has_next(&it)) {
//       Element *child = element_next(&it);

//       switch (child->info.position.kind) {
//       case Element_Position_Relative: {
//         child->computed_rect.x = start_x;
//         child->computed_rect.y = current_y;

//         if (info->alignment.horizontal == Element_Alignment_Center) {
//           child->computed_rect.x +=
//               (content_width - child->computed_rect.width) * 0.5f;
//         } else if (info->alignment.horizontal == Element_Alignment_End) {
//           child->computed_rect.x += content_width -
//           child->computed_rect.width;
//         }

//         current_y += child->computed_rect.height;
//         switch (info->alignment.vertical) {
//         case Element_Alignment_Space_Evenly:
//           current_y +=
//               remaining_height / (f32)(element->relative_child_count + 1);
//           break;
//         case Element_Alignment_Space_Between:
//           if (element->relative_child_count > 1) {
//             current_y +=
//                 remaining_height / (f32)(element->relative_child_count - 1);
//           }
//           break;
//         case Element_Alignment_Start:
//         case Element_Alignment_Center:
//         case Element_Alignment_End:
//         default:
//           if (it.iteration < element->child_count) {
//             current_y += computed_child_gap;
//           }
//           break;
//         }

//       } break;
//       case Element_Position_Absolute:
//         position_absolute_element(child);
//         break;
//       }

//       if (child->child_count > 0) {
//         layout_children(child);
//       }
//     }
//   } break;
//   }
// }

// ///////////////////////
// // Public API
// ///////////////////////
// // FIXME(nico): Provide a way to query the minimum size the context will need
// Element_Error element_context_init(
//     Element_Context *ctx, Element_Context_Create_Info *info, Allocator
//     allocator
// ) {
//   usize init_cap = info->init_cap ? info->init_cap : 256;

//   ctx->allocator = allocator;
//   ctx->flags = info->flags;
//   ctx->elements = make_element_list(init_cap, allocator);
//   ctx->element_roots = make_element_index_list(init_cap, allocator);
//   ctx->element_stack = make_element_index_list(init_cap, allocator);
//   ctx->element_cache = make_open_map(
//       Element_Id,
//       Element_Cached_Info,
//       init_cap,
//       element_id_hash,
//       element_id_eq,
//       allocator
//   );

//   usize cmd_cap = init_cap * 2;
//   ctx->commands = make_element_render_list(cmd_cap, allocator);

//   // Callbacks
//   ctx->measure_text = info->measure_text_proc;

//   return Element_Error_None;
// }

// Element_Error close_element_context(Element_Context *ctx) {
//   delete_element_list(&ctx->elements);
//   delete_element_index_list(&ctx->element_roots);
//   delete_element_index_list(&ctx->element_stack);
//   delete_open_map(ctx->element_cache);

//   delete_element_render_list(&ctx->commands);

//   return Element_Error_None;
// }

// void set_context_current(Element_Context *ctx) {
//   _ctx = ctx;
// }

// void set_screen_state(Element_Context *ctx, Element_Dimensions dimensions) {
//   ctx->screen_dimensions = dimensions;
// }

// void set_pointer_state(
//     Element_Context *ctx, Element_Point m_pos, bool32 m_left, bool32 m_right
// ) {
//   ctx->m_previous_pos = ctx->m_pos;
//   ctx->m_pos = m_pos;
//   ctx->m_delta = (Element_Point){
//     .x = m_pos.x - ctx->m_previous_pos.x,
//     .y = m_pos.y - ctx->m_previous_pos.y,
//   };

//   ctx->m_left = (Element_Input_Info){
//     .previously_pressed = ctx->m_left.pressed,
//     .previously_just_pressed = ctx->m_left.just_pressed,
//     .previously_just_released = ctx->m_left.just_released,
//     .pressed = (bool8)m_left,
//     .just_pressed = m_left && !ctx->m_left.pressed,
//     .just_released = !m_left && ctx->m_left.pressed,
//   };
//   ctx->m_right = (Element_Input_Info){
//     .previously_pressed = ctx->m_right.pressed,
//     .previously_just_pressed = ctx->m_right.just_pressed,
//     .previously_just_released = ctx->m_right.just_released,
//     .pressed = (bool8)m_right,
//     .just_pressed = m_right && !ctx->m_right.pressed,
//     .just_released = !m_right && ctx->m_right.pressed,
//   };

//   ctx->m_previously_over_ui = ctx->m_over_ui;
//   ctx->m_over_ui = false;
// }

// void set_delta_time(Element_Context *ctx, f32 dt) {
//   ctx->dt = dt;
// }

// /**
//  * @brief ONLY use during a begin_ui/end_ui scope or after manually setting
//  the
//  * context
//  * @return return the pointer position stored by the context
//  */
// Element_Point get_mouse_delta() {
//   return _ctx->m_delta;
// }

// /**
//  * @brief ONLY use during a begin_ui/end_ui scope or after manually setting
//  the
//  * context
//  * @return return the pointer delta stored by the context
//  */
// Element_Point get_mouse_position() {
//   return _ctx->m_pos;
// }

// void begin_ui(Element_Context *ctx) {
//   ctx->id_counter = 0;
//   ctx->frame_counter += 1;
//   ctx->elements.len = 0;
//   ctx->element_roots.len = 0;
//   ctx->element_stack.len = 0;
//   ctx->commands.len = 0;

//   if (ctx->m_left.just_released) {
//     ctx->active_element = nullptr;
//   }

//   _ctx = ctx;
// }

// Element_Render_Command_Buffer end_ui(Element_Context *ctx) {
//   for (usize i = 0; i < ctx->element_roots.len; i += 1) {
//     Element *root = list_get(ctx->element_roots, Element *, i);

//     switch (root->info.position.kind) {
//     case Element_Position_Relative:
//       root->computed_rect.x = 0;
//       root->computed_rect.y = 0;
//       break;
//     case Element_Position_Absolute:
//       position_absolute_element(root);
//       break;
//     }

//     if (root->info.layout != Element_Layout_Kind_None) {
//       layout_children(root);
//     }

//     process_element(root);
//   }

//   Open_Map_Iterator it = open_map_iterator(ctx->element_cache);
//   while (open_map_has_next(&it)) {
//     open_map_next(&it);
//     Element_Cached_Info *cached_info = it.value;

//     // NOTE(nico): pretty arbitrary choice of frames. Could expose this to
//     the
//     // context options
//     if (ctx->frame_counter - cached_info->last_touched >= 30) {
//       assert(open_map_remove_raw(ctx->element_cache, it.key));
//     }
//   }

//   _ctx = nullptr;

//   return (Element_Render_Command_Buffer){
//     .items = ctx->commands,
//     .len = list_len(ctx->commands),
//   };
// }

// void begin_element(Element_Create_Info *info, Element_Flags flags) {
//   if (_ctx->flags & Element_Context_Flag_Validation_Layer) {
//     if (flags & Element_Flag_Render_Text && !info->style.font_data) {
//       assert(false);
//       // TODO(nico): Log to the define output
//     }
//   }

//   Element *element = list_push(
//       _ctx->elements,
//       ((Element){
//         .info =
//             (Element_Info){
//               .id = info->id,
//               .layout = info->layout,
//               .sizing =
//                   {.width = info->sizing.width, .height =
//                   info->sizing.height},
//               .alignment =
//                   {
//                     .horizontal = info->alignment.horizontal,
//                     .vertical = info->alignment.vertical,
//                   },
//               .position = info->position,
//               .text = info->text,
//               .image = info->image,
//             },
//         .style = info->style,
//         .flags = flags
//       })
//   );

//   Element *parent = _ctx->current_element;

//   if (parent != nullptr) {
//     element->parent = parent;

//     parent->child_count += 1;
//     if (info->position.kind == Element_Position_Relative) {
//       parent->relative_child_count += 1;
//     }

//     if (parent->first_child == nullptr) {
//       parent->first_child = element;
//     } else {
//       parent->last_child->next = element;
//     }
//     parent->last_child = element;
//   } else {
//     list_push(_ctx->element_roots, element);
//   }

//   if (element->info.id.hash == 0) {
//     element->info.id = (Element_Id){
//       .hash = RESERVED_ELEMENT_HASH,
//       .index = _ctx->id_counter,
//     };
//     _ctx->id_counter += 1;
//   }

//   list_push(_ctx->element_stack, element);
//   _ctx->current_element = element;
// }

// void end_element() {
//   if (_ctx->current_element == nullptr) {
//     assert(false);
//   }

//   Element *element = list_pop(_ctx->element_stack, Element *);
//   calculate_element_size(element);

//   if (element->parent != nullptr &&
//       element->info.position.kind == Element_Position_Relative) {
//     Element *parent = element->parent;

//     switch (parent->info.layout) {
//     case Element_Layout_Kind_None:
//       break;
//     case Element_Layout_Kind_Row:
//       parent->computed_rect.width += element->computed_rect.width;
//       parent->computed_rect.height =
//           fmaxf(parent->computed_rect.height, element->computed_rect.height);
//       break;
//     case Element_Layout_Kind_Column:
//       parent->computed_rect.width =
//           fmaxf(parent->computed_rect.width, element->computed_rect.width);
//       parent->computed_rect.height += element->computed_rect.height;
//       break;
//     }
//   }

//   usize stack_len = list_len(_ctx->element_stack);
//   if (stack_len > 0) {
//     _ctx->current_element =
//         list_get(_ctx->element_stack, Element *, stack_len - 1);
//   } else {
//     _ctx->current_element = nullptr;
//   }
// }

// // FIXME(nico): no null pointer guard
// Element_Client_Info get_current_element() {
//   Element_Cached_Info *cached_info =
//       open_map_get(_ctx->element_cache, _ctx->current_element->info.id);

//   if (cached_info == nullptr) {
//     return (Element_Client_Info){0};
//   }

//   return (Element_Client_Info){
//     .computed_rect = cached_info->computed_rect,
//     .events = cached_info->events,
//   };
// }