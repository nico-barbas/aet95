#include "ui.h"

#include "core/imgui.h"
#include "core/math.h"
#include "core/platform.h"
#include "core/strings.h"
#include "core/types.h"
#include "db.h"
#include "game.h"
#include "render.h"

#include <assert.h>
#include <stdio.h>

void game_view(Game_State *model) {
  (void)model;
  Font_Atlas *font = &_db.font_table[Font_ID_IBM_Default];

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

  // NOTE(nico): measure_texture_wrapper hard crashes on any size that is not
  // the baked one, so the atlas drives the font size
  // f32 font_size = font->line_height;

  container((&(Element_Create_Info){
    .override_flags = Element_Flag_Ignore_Events,
    .layout = Element_Layout_Kind_Column,
    .sizing = {.width = element_sizing_fit(), .height = element_sizing_fit()},
    .style = {
      .base =
          {
            .constraints.padding = element_constraint(40, 40, 40, 40),
            .colors.background = color(1, 0, 1, 1),
          },
      .variants =
          {
            [Element_Style_Variant_Enter] =
                {
                  .constraints.padding = element_constraint(0, 0, 0, 0),
                },
          },
      .variant_masks =
          {
            [Element_Style_Variant_Enter] = bitmask(Element_Property_Padding),
          },
      .transition_set = bitmask(Element_State_Enter, Element_State_Normal),
      .transitions = {
        [Element_State_Normal] = {
          .duration = 10.f,
        },
      },
    }
  })) {
    label((&(Element_Create_Info){
      .text = from_c_str("hello world"),
      .style = {
        .base =
            {
              .linears.font_size = 18.f,
              .colors.text = color(1, 1, 1, 1),
            },
        .font_data = font,
      },
    }));
  }
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
    }
  }
}