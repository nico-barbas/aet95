#ifndef ASM_H
#define ASM_H

#include "core/allocator.h"
#include "core/strings.h"
#include "hal.h"

typedef enum Aet_Assembler_Error {
  Aet_Assembler_Error_None,
  Aet_Assembler_Error_Malformed_Number_Literal,
  Aet_Assembler_Error_Invalid_Identifier,
  Aet_Assembler_Error_Invalid_Syntax,
} Aet_Assembler_Error;

typedef struct Aet_Assembler_Result {
  Aet_Assembler_Error err;
  Aet_Program output;
} Aet_Assembler_Result;

typedef struct Aet_Disassembler {
  Allocator allocator;
  Aet_Program program;
  usize offset;
} Aet_Disassembler;

typedef struct Aet_Disassembler_Result {
  bool32 ok;
  String output;
} Aet_Disassembler_Result;

Aet_Assembler_Result aet_assemble(String source, Allocator allocator);
Aet_Disassembler_Result
aet_disassemble(Aet_Program program, Allocator allocator);

#endif