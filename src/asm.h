#ifndef ASM_H
#define ASM_H

#include "core/allocator.h"
#include "core/strings.h"
#include "hal.h"

typedef enum Aet_Assembler_Error {
  Aet_Assembler_Error_None,
  Aet_Assembler_Error_Internal_Failure,
  Aet_Assembler_Error_Malformed_Decimal_Literal,
  Aet_Assembler_Error_Malformed_Hex_Literal,
  Aet_Assembler_Error_Malformed_Binary_Literal,
  Aet_Assembler_Error_Invalid_Identifier,
  Aet_Assembler_Error_Invalid_Syntax,
  Aet_Assembler_Error_Invalid_Immediate_Value,
  Aet_Assembler_Error_Duplicate_Symbol,
  Aet_Assembler_Error_Unknown_Symbol,
} Aet_Assembler_Error;

/*
  NOTE(nico):
  single source of truth for all the pseudo-instructions.
  internal value name | mnemonic | form | instruction count | label allowed
*/
#define AET_PSEUDO_INSTRUCTIONS(X) X(Lf, "loadf", RI, 2, false)

typedef enum Aet_Assembly_Token_Kind {
  Aet_Assembly_Token_Kind_EOF,
  Aet_Assembly_Token_Kind_Newline,
  Aet_Assembly_Token_Kind_Comment,
  Aet_Assembly_Token_Kind_Identifier,

  Aet_Assembly_Token_Kind_Comma,
  Aet_Assembly_Token_Kind_Colon,
  Aet_Assembly_Token_Kind_Integer_Literal,
  Aet_Assembly_Token_Kind_Float_Literal,

  // Registers
  Aet_Assembly_Token_Kind_register_start_,
  Aet_Assembly_Token_Kind_Rx0,
  Aet_Assembly_Token_Kind_Rx1,
  Aet_Assembly_Token_Kind_Rx2,
  Aet_Assembly_Token_Kind_R0,
  Aet_Assembly_Token_Kind_R1,
  Aet_Assembly_Token_Kind_R2,
  Aet_Assembly_Token_Kind_R3,
  Aet_Assembly_Token_Kind_R4,
  Aet_Assembly_Token_Kind_R5,
  Aet_Assembly_Token_Kind_R6,
  Aet_Assembly_Token_Kind_R7,
  Aet_Assembly_Token_Kind_R8,
  Aet_Assembly_Token_Kind_R9,
  Aet_Assembly_Token_Kind_R10,
  Aet_Assembly_Token_Kind_R11,
  Aet_Assembly_Token_Kind_R12,
  Aet_Assembly_Token_Kind_register_end_,

  Aet_Assembly_Token_Kind_instruction_start_,
#define X(name, text, opcode, form, ext, instr_count, label_allowed)           \
  Aet_Assembly_Token_Kind_##name,
  AET_INSTRUCTIONS(X)
#undef X
      Aet_Assembly_Token_Kind_instruction_end_,

  Aet_Assembly_Token_Kind_pseudo_start_,
#define X(name, text, form, instr_count, label_allowed)                        \
  Aet_Assembly_Token_Kind_##name,
  AET_PSEUDO_INSTRUCTIONS(X)
#undef X
      Aet_Assembly_Token_Kind_pseudo_end_,
  Aet_Assembly_Token_Kind_MAX,
} Aet_Assembly_Token_Kind;

typedef struct Aet_Assembly_Token {
  Aet_Assembly_Token_Kind kind;
  usize start;
  usize end;
  String lexeme;
} Aet_Assembly_Token;

typedef Result(Aet_Program, Aet_Assembler_Error) Aet_Assembler_Result;
typedef Result(
    Aet_Assembly_Token, Aet_Assembler_Error
) Aet_Assembly_Token_Result;

Aet_Assembly_Token_Result aet_assembler_next_token(String_Reader *reader);
Aet_Assembler_Result aet_assemble(String source, Allocator allocator);

typedef enum Aet_Disassembler_Error {
  Aet_Disassembler_Error_None,
  Aet_Disassembler_Error_Invalid_Program,
  Aet_Disassembler_Error_Invalid_Opcode,
} Aet_Disassembler_Error;

typedef Result(String, Aet_Disassembler_Error) Aet_Disassembler_Result;

Aet_Disassembler_Result
aet_disassemble(Aet_Program program, Allocator allocator);

#endif