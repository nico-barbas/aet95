#include "core/platform.h"
#include "game.h"

#include <assert.h>

#if defined(PLATFORM_WEB)
#include <emscripten/emscripten.h>
#endif

static void update_draw_frame(void);

int main(void) {
  init_game();

#if defined(PLATFORM_WEB)
  emscripten_set_main_loop(update_draw_frame, 0, 1);
#else
  while (app_update(&_game.app)) {
    update_draw_frame();
  }

  close_game();
#endif

  return 0;
}

void update_draw_frame(void) {
#if defined(PLATFORM_WEB)
  app_update(&_state.app);
#endif
  update_game();

  app_begin_frame(&_game.app);
  render_game();
  app_end_frame(&_game.app);
}