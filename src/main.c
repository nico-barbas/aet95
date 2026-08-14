#include "asm.h"
#include "core/allocator.h"
#include "core/strings.h"

#include <assert.h>
#include <stdio.h>

const char *src = "mul r0, r1, r2\n"
                  "addi r0, r1, 100";

int main(void) {
  Allocator allocator = heap_allocator();

  Aet_Assembler_Result result = aet_assemble(from_c_str(src), allocator);
  assert(result.err == Aet_Assembler_Error_None);

  Aet_Disassembler_Result disasm_result =
      aet_disassemble(result.output, allocator);
  assert(disasm_result.ok);
  printf("%s\n", disasm_result.output.data);
}