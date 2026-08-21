#ifndef UI_H
#define UI_H

#include "core/imgui.h"
#include "game.h"

void game_view(Game_State *model);

void render_game_view(Renderer2D *renderer, Element_Render_Command_Buffer cmds);

#endif