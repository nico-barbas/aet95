#include "asm.h"
#include "core/allocator.h"
#include "core/log.h"
#include "core/platform.h"
#include "core/strings.h"
#include "hal.h"

#include <assert.h>
#include <stdio.h>

static const char *src = "loadb r0, r1, -64";

int main(void) {
  Allocator allocator = heap_allocator();

  // App app = {0};
  // assert(init_app(
  //     &(App_Create_Info){
  //       .app = &app,
  //       .logger = console_logger(Log_Level_Debug),
  //       .window_title = from_c_str("aet95"),
  //       .window_width = 1280,
  //       .window_height = 720,
  //     },
  //     allocator
  // ));

  Aet_Assembler_Result result = aet_assemble(from_c_str(src), allocator);
  assert(result.err == Aet_Assembler_Error_None);

  Aet_Disassembler_Result disasm_result =
      aet_disassemble(result.output, allocator);
  assert(disasm_result.ok);
  printf("%s\n", disasm_result.output.data);

  Aet_RAM ram = {0};
  Aet_CPU cpu = {0};
  assert(aet_cpu_init(&cpu) == Aet_CPU_Error_None);

  aet_cpu_execute(&cpu, &ram, 4000);

  // while (app_update(&app)) {
  //   app_begin_frame(&app);

  //   app_end_frame(&app);
  // }
}
