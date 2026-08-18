#ifndef ASM_H
#define ASM_H

#include "core/allocator.h"
#include "core/strings.h"
#include "hal.h"

typedef enum Aet_Assembler_Error {
  Aet_Assembler_Error_None,
  Aet_Assembler_Error_Malformed_Number,
  Aet_Assembler_Error_Invalid_Identifier,
  Aet_Assembler_Error_Invalid_Syntax,
  Aet_Assembler_Error_Invalid_Immediate_Value,
} Aet_Assembler_Error;

typedef Result(Aet_Program, Aet_Assembler_Error) Aet_Assembler_Result;

typedef enum Aet_Disassembler_Error {
  Aet_Disassembler_Error_None,
  Aet_Disassembler_Error_Invalid_Program,
  Aet_Disassembler_Error_Invalid_Opcode,
} Aet_Disassembler_Error;

typedef Result(String, Aet_Disassembler_Error) Aet_Disassembler_Result;

Aet_Assembler_Result aet_assemble(String source, Allocator allocator);
Aet_Disassembler_Result
aet_disassemble(Aet_Program program, Allocator allocator);

#endif