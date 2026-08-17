#include "asm.h"

#include "core/math.h"

#include <assert.h>
#include <string.h>

// NOTE(nico): single source of truth for instruction mnemonics. Feeds the
// token kind enum, the keyword table below, and (eventually) the disassembler's
// kind -> text mapping. Adding an instruction should only mean adding a line
// here.
#define AET_INSTRUCTIONS(X)                                                    \
  X(Addi, "addi")                                                              \
  X(Add, "add")                                                                \
  X(Sub, "sub")                                                                \
  X(Mul, "mul")                                                                \
  X(Div, "div")                                                                \
  X(And, "and")                                                                \
  X(Or, "or")                                                                  \
  X(Xor, "xor")                                                                \
  X(Shl, "shiftl")                                                             \
  X(Shr, "shiftr")                                                             \
  X(Lb, "loadb")                                                               \
  X(Lh, "loadh")                                                               \
  X(Lw, "loadw")                                                               \
  X(Sb, "storeb")                                                              \
  X(Sh, "storeh")                                                              \
  X(Sw, "storew")                                                              \
  X(Beq, "beq")                                                                \
  X(Bneq, "bneq")                                                              \
  X(Blt, "blt")                                                                \
  X(Bgeq, "bgeq")                                                              \
  X(Bltu, "bltu")                                                              \
  X(Bgequ, "bgequ")                                                            \
  X(Jmp, "jump")                                                               \
  X(Call, "call")                                                              \
  X(Ret, "ret")

typedef enum Aet_Assembly_Token_Kind {
  Aet_Assembly_Token_Kind_EOF,
  Aet_Assembly_Token_Kind_Newline,

  Aet_Assembly_Token_Kind_Comma,
  Aet_Assembly_Token_Kind_Number_Literal,

  // Keywords
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
#define X(name, text) Aet_Assembly_Token_Kind_##name,
  AET_INSTRUCTIONS(X)
#undef X
      Aet_Assembly_Token_Kind_instruction_end_,
} Aet_Assembly_Token_Kind;

typedef struct Aet_Assembly_Token {
  Aet_Assembly_Token_Kind kind;
  usize start;
  usize end;
  String lexeme;
} Aet_Assembly_Token;

typedef struct Aet_Assembler {
  Allocator allocator;
  String_Reader reader;
  Aet_Assembly_Token previous;
  Aet_Assembly_Token current;
} Aet_Assembler;

typedef Option(Aet_Assembly_Token_Kind) Aet_Assembly_Keyword_Option;

typedef struct Aet_Assembly_Keyword {
  const char *text;
  usize len;
  Aet_Assembly_Token_Kind kind;
} Aet_Assembly_Keyword;

typedef struct Aet_Assembler_Instruction_Result {
  Aet_Assembler_Error err;
  u32 instr;
} Aet_Assembler_Instruction_Result;

// NOTE(nico): sizeof(text) - 1 is the literal's length at compile time, so no
// strlen happens per comparison.
static const Aet_Assembly_Keyword aet_assembly_keywords[] = {
#define X(name, text) {text, sizeof(text) - 1, Aet_Assembly_Token_Kind_##name},
  AET_INSTRUCTIONS(X)
#undef X
};

static bool32 char_is_letter(char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

static bool32 aet_assembler_starts_number(String_Reader *reader, char c) {
  if (char_is_number(c)) {
    return true;
  }

  return c == '-' && !string_reader_is_eof(reader) &&
         char_is_number(string_reader_peek(reader));
}

static bool32 aet_assembly_token_is_instruction(Aet_Assembly_Token token) {
  return token.kind > Aet_Assembly_Token_Kind_instruction_start_ &&
         token.kind < Aet_Assembly_Token_Kind_instruction_end_;
}

static bool32 aet_assembly_token_is_register(Aet_Assembly_Token token) {
  return token.kind > Aet_Assembly_Token_Kind_register_start_ &&
         token.kind < Aet_Assembly_Token_Kind_register_end_;
}

static Aet_Register aet_assembly_token_to_register(Aet_Assembly_Token token) {
  return (Aet_Register)(token.kind - Aet_Assembly_Token_Kind_register_start_ -
                        1);
}

static void aet_assembler_skip_whitespace(String_Reader *reader) {
  while (true) {
    if (string_reader_is_eof(reader)) {
      break;
    }

    char n = string_reader_peek(reader);
    if (n != '\r' && n != ' ' && n != '\b' && n != '\t') {
      break;
    }

    string_reader_advance(reader);
  }
}

// NOTE(nico): called with the sign (if any) and the first digit already
// consumed, so this only walks the remaining digits and the decimal point.
// NOTE(nico): [17-08-26] Removed the floating point parsing as the decision on
// if the ISA will support floating point instruction at the hardware level or
// soft-float will be implemented on top is undefined
static Aet_Assembler_Error aet_assembler_lex_number(String_Reader *reader) {
  while (true) {
    if (string_reader_is_eof(reader)) {
      break;
    }

    char n = string_reader_peek(reader);
    if (!char_is_number(n)) {
      break;
    }

    string_reader_advance(reader);
  }

  return Aet_Assembler_Error_None;
}

static Aet_Assembler_Error
aet_assembler_lex_identifier(String_Reader *reader, bool32 allow_numbers) {
  while (true) {
    if (string_reader_is_eof(reader)) {
      break;
    }

    char n = string_reader_peek(reader);
    if (!char_is_letter(n) && !(allow_numbers && char_is_number(n))) {
      break;
    }

    string_reader_advance(reader);
  }

  return Aet_Assembler_Error_None;
}

static Aet_Assembly_Keyword_Option aet_assembler_match_keyword(String str) {
  for (usize i = 0; i < countof(aet_assembly_keywords); i += 1) {
    const Aet_Assembly_Keyword *keyword = &aet_assembly_keywords[i];

    if (keyword->len == str.len &&
        memcmp(keyword->text, str.data, str.len) == 0) {
      return some(Aet_Assembly_Keyword_Option, keyword->kind);
    }
  }

  return none(Aet_Assembly_Keyword_Option);
}

static Aet_Assembly_Keyword_Option aet_assembler_match_register(String str) {
  if (str.len < 2 || str.len > 3 ||
      (str.data[0] != 'r' && str.data[0] != 'R')) {
    return none(Aet_Assembly_Keyword_Option);
  }

  if (str.data[1] == 'x') {
    switch (str.data[2]) {
    case '0':
      return some(Aet_Assembly_Keyword_Option, Aet_Assembly_Token_Kind_Rx0);
    case '1':
      return some(Aet_Assembly_Keyword_Option, Aet_Assembly_Token_Kind_Rx1);
    case '2':
      return some(Aet_Assembly_Keyword_Option, Aet_Assembly_Token_Kind_Rx2);
    default:
      return none(Aet_Assembly_Keyword_Option);
    }
  }

  u32 n = 0;
  for (usize i = 1; i < str.len; i += 1) {
    if (!char_is_number(str.data[i])) {
      return none(Aet_Assembly_Keyword_Option);
    }

    n = n * 10 + (u32)(str.data[i] - '0');
  }

  if (n >= 16) {
    return none(Aet_Assembly_Keyword_Option);
  }

  Aet_Assembly_Token_Kind reg = (Aet_Assembly_Token_Kind_R0 + n);
  return some(Aet_Assembly_Keyword_Option, reg);
}

static Aet_Assembly_Token aet_assembler_next_token(String_Reader *reader) {
  aet_assembler_skip_whitespace(reader);
  if (string_reader_is_eof(reader)) {
    return (Aet_Assembly_Token){
      .kind = Aet_Assembly_Token_Kind_EOF,
      .start = reader->current,
      .end = reader->current,
      .lexeme = from_c_str(""),
    };
  }

  Aet_Assembly_Token token = {
    .start = reader->current,
  };

  char c = string_reader_advance(reader);
  switch (c) {
  case '\n':
    token.kind = Aet_Assembly_Token_Kind_Newline;
    break;
  case ',':
    token.kind = Aet_Assembly_Token_Kind_Comma;
    break;
  default: {
    if (aet_assembler_starts_number(reader, c)) {
      // FIXME(nico): Need to refactor everything to have the error flow up
      // I really don't like passing out paramaters
      token.kind = Aet_Assembly_Token_Kind_Number_Literal;
      assert(aet_assembler_lex_number(reader) == Aet_Assembler_Error_None);
    } else if (char_is_letter(c)) {
      assert(
          aet_assembler_lex_identifier(reader, true) == Aet_Assembler_Error_None
      );

      String identifier =
          string_slice(reader->input, token.start, reader->current);
      Aet_Assembly_Keyword_Option opt = aet_assembler_match_keyword(identifier);

      if (opt.some) {
        token.kind = opt.value;
      } else {
        opt = aet_assembler_match_register(identifier);

        if (opt.some) {
          token.kind = opt.value;
        } else {
          assert(false);
        }
      }
    } else {
      assert(false);
    }
  } break;
  }

  token.end = reader->current;
  token.lexeme = string_slice(reader->input, token.start, token.end);

  return token;
}

static Aet_Assembly_Token
aet_assembler_consume_token(Aet_Assembler *assembler) {
  assembler->previous = assembler->current;
  assembler->current = aet_assembler_next_token(&assembler->reader);

  return assembler->current;
}

static bool32 aet_assembler_expect_token(
    Aet_Assembler *assembler, Aet_Assembly_Token_Kind kind
) {
  Aet_Assembly_Token next = aet_assembler_consume_token(assembler);
  return next.kind == kind;
}

static Aet_Assembler_Instruction_Result
aet_assemble_register_register_op(Aet_Assembler *assembler, Aet_CPU_Opcode op) {
  Aet_Assembly_Token arg1 = aet_assembler_consume_token(assembler);
  if (!aet_assembly_token_is_register(arg1)) {
    return (Aet_Assembler_Instruction_Result){
      .err = Aet_Assembler_Error_Invalid_Syntax
    };
  }

  if (!aet_assembler_expect_token(assembler, Aet_Assembly_Token_Kind_Comma)) {
    return (Aet_Assembler_Instruction_Result){
      .err = Aet_Assembler_Error_Invalid_Syntax
    };
  }

  Aet_Assembly_Token arg2 = aet_assembler_consume_token(assembler);
  if (!aet_assembly_token_is_register(arg2)) {
    return (Aet_Assembler_Instruction_Result){
      .err = Aet_Assembler_Error_Invalid_Syntax
    };
  }

  if (!aet_assembler_expect_token(assembler, Aet_Assembly_Token_Kind_Comma)) {
    return (Aet_Assembler_Instruction_Result){
      .err = Aet_Assembler_Error_Invalid_Syntax
    };
  }

  Aet_Assembly_Token arg3 = aet_assembler_consume_token(assembler);
  if (!aet_assembly_token_is_register(arg3)) {
    return (Aet_Assembler_Instruction_Result){
      .err = Aet_Assembler_Error_Invalid_Syntax
    };
  }

  Aet_Register rd = aet_assembly_token_to_register(arg1);
  Aet_Register rs1 = aet_assembly_token_to_register(arg2);
  Aet_Register rs2 = aet_assembly_token_to_register(arg3);

  return (Aet_Assembler_Instruction_Result){
    .instr = (u32)(op) | ((u32)rd << 8) | ((u32)rs1 << 12) | ((u32)rs2 << 16)
  };
}

static Aet_Assembler_Instruction_Result
aet_assemble_register_immediate_operation(
    Aet_Assembler *assembler, Aet_CPU_Opcode op
) {
  Aet_Assembly_Token arg1 = aet_assembler_consume_token(assembler);
  if (!aet_assembly_token_is_register(arg1)) {
    return (Aet_Assembler_Instruction_Result){
      .err = Aet_Assembler_Error_Invalid_Syntax
    };
  }

  if (!aet_assembler_expect_token(assembler, Aet_Assembly_Token_Kind_Comma)) {
    return (Aet_Assembler_Instruction_Result){
      .err = Aet_Assembler_Error_Invalid_Syntax
    };
  }

  Aet_Assembly_Token arg2 = aet_assembler_consume_token(assembler);
  if (!aet_assembly_token_is_register(arg2)) {
    return (Aet_Assembler_Instruction_Result){
      .err = Aet_Assembler_Error_Invalid_Syntax
    };
  }

  if (!aet_assembler_expect_token(assembler, Aet_Assembly_Token_Kind_Comma)) {
    return (Aet_Assembler_Instruction_Result){
      .err = Aet_Assembler_Error_Invalid_Syntax
    };
  }

  Aet_Assembly_Token arg3 = aet_assembler_consume_token(assembler);
  if (arg3.kind != Aet_Assembly_Token_Kind_Number_Literal) {
    return (Aet_Assembler_Instruction_Result){
      .err = Aet_Assembler_Error_Invalid_Syntax
    };
  }

  Aet_Register rd = aet_assembly_token_to_register(arg1);
  Aet_Register rs1 = aet_assembly_token_to_register(arg2);

  i64 immediate = 0;
  if (!string_to_i64(arg3.lexeme, &immediate)) {
    return (Aet_Assembler_Instruction_Result){
      .err = Aet_Assembler_Error_Invalid_Syntax,
    };
  }

  return (Aet_Assembler_Instruction_Result){
    .instr =
        (u32)(op) | ((u32)rd << 8) | ((u32)rs1 << 12) | ((u32)immediate << 16)
  };
}

static Aet_Assembler_Instruction_Result
aet_assemble_immediate_operation(Aet_Assembler *assembler, Aet_CPU_Opcode op) {
  Aet_Assembly_Token arg = aet_assembler_consume_token(assembler);
  if (arg.kind != Aet_Assembly_Token_Kind_Number_Literal) {
    return (Aet_Assembler_Instruction_Result){
      .err = Aet_Assembler_Error_Invalid_Syntax
    };
  }

  i64 immediate = 0;
  if (!string_to_i64(arg.lexeme, &immediate)) {
    return (Aet_Assembler_Instruction_Result){
      .err = Aet_Assembler_Error_Invalid_Syntax,
    };
  }

  return (Aet_Assembler_Instruction_Result){
    .instr = (u32)(op) | ((u32)immediate << 8)
  };
}

Aet_Assembler_Result aet_assemble(String source, Allocator allocator) {
  // NOTE(nico): stupid temporary fixed buffer
  static u32 output[4096] = {0};
  usize output_len = 0;

  Aet_Assembler assembler = {
    .allocator = allocator,
    .reader = {
      .input = source,
    }
  };

  Aet_Assembler_Result result = {0};

  while (true) {
    Aet_Assembly_Token token = aet_assembler_consume_token(&assembler);

    if (token.kind == Aet_Assembly_Token_Kind_EOF) {
      break;
    }

    if (!aet_assembly_token_is_instruction(token)) {
      result.err = Aet_Assembler_Error_Invalid_Syntax;
      break;
    }

    // NOTE(nico): the guard above restricts token.kind to the instruction
    // range, so the register/punctuation kinds are unreachable here. Listing
    // them just to satisfy -Wswitch-enum would add dead labels to this switch
    // every time a register or token kind is added.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wswitch-enum"
    Aet_Assembler_Instruction_Result instr_result = {0};

    switch (token.kind) {
    case Aet_Assembly_Token_Kind_Addi:
      instr_result = aet_assemble_register_immediate_operation(
          &assembler, Aet_CPU_Opcode_Addi
      );
      break;
    case Aet_Assembly_Token_Kind_Add:
      instr_result =
          aet_assemble_register_register_op(&assembler, Aet_CPU_Opcode_Add);
      break;
    case Aet_Assembly_Token_Kind_Sub:
      instr_result =
          aet_assemble_register_register_op(&assembler, Aet_CPU_Opcode_Sub);
      break;
    case Aet_Assembly_Token_Kind_Mul:
      instr_result =
          aet_assemble_register_register_op(&assembler, Aet_CPU_Opcode_Mul);
      break;
    case Aet_Assembly_Token_Kind_Div:
      instr_result =
          aet_assemble_register_register_op(&assembler, Aet_CPU_Opcode_Div);
      break;
    case Aet_Assembly_Token_Kind_And:
      instr_result =
          aet_assemble_register_register_op(&assembler, Aet_CPU_Opcode_And);
      break;
    case Aet_Assembly_Token_Kind_Or:
      instr_result =
          aet_assemble_register_register_op(&assembler, Aet_CPU_Opcode_Or);
      break;
    case Aet_Assembly_Token_Kind_Xor:
      instr_result =
          aet_assemble_register_register_op(&assembler, Aet_CPU_Opcode_Xor);
      break;
    case Aet_Assembly_Token_Kind_Shl:
      instr_result =
          aet_assemble_register_register_op(&assembler, Aet_CPU_Opcode_Shl);
      break;
    case Aet_Assembly_Token_Kind_Shr:
      instr_result =
          aet_assemble_register_register_op(&assembler, Aet_CPU_Opcode_Shr);
      break;
    case Aet_Assembly_Token_Kind_Lb:
      instr_result = aet_assemble_register_immediate_operation(
          &assembler, Aet_CPU_Opcode_Lb
      );
      break;
    case Aet_Assembly_Token_Kind_Lh:
      instr_result = aet_assemble_register_immediate_operation(
          &assembler, Aet_CPU_Opcode_Lh
      );
      break;
    case Aet_Assembly_Token_Kind_Lw:
      instr_result = aet_assemble_register_immediate_operation(
          &assembler, Aet_CPU_Opcode_Lw
      );
      break;
    case Aet_Assembly_Token_Kind_Sb:
      instr_result = aet_assemble_register_immediate_operation(
          &assembler, Aet_CPU_Opcode_Sb
      );
      break;
    case Aet_Assembly_Token_Kind_Sh:
      instr_result = aet_assemble_register_immediate_operation(
          &assembler, Aet_CPU_Opcode_Sh
      );
      break;
    case Aet_Assembly_Token_Kind_Sw:
      instr_result = aet_assemble_register_immediate_operation(
          &assembler, Aet_CPU_Opcode_Sw
      );
      break;
    case Aet_Assembly_Token_Kind_Beq:
      instr_result = aet_assemble_register_immediate_operation(
          &assembler, Aet_CPU_Opcode_Beq
      );
      break;
    case Aet_Assembly_Token_Kind_Bneq:
      instr_result = aet_assemble_register_immediate_operation(
          &assembler, Aet_CPU_Opcode_Bneq
      );
      break;
    case Aet_Assembly_Token_Kind_Blt:
      instr_result = aet_assemble_register_immediate_operation(
          &assembler, Aet_CPU_Opcode_Blt
      );
      break;
    case Aet_Assembly_Token_Kind_Bgeq:
      instr_result = aet_assemble_register_immediate_operation(
          &assembler, Aet_CPU_Opcode_Bgeq
      );
      break;
    case Aet_Assembly_Token_Kind_Bltu:
      instr_result = aet_assemble_register_immediate_operation(
          &assembler, Aet_CPU_Opcode_Bltu
      );
      break;
    case Aet_Assembly_Token_Kind_Bgequ:
      instr_result = aet_assemble_register_immediate_operation(
          &assembler, Aet_CPU_Opcode_Bgequ
      );
      break;
    case Aet_Assembly_Token_Kind_Jmp:
      instr_result =
          aet_assemble_immediate_operation(&assembler, Aet_CPU_Opcode_Jmp);
      break;
    case Aet_Assembly_Token_Kind_Call:
      instr_result =
          aet_assemble_immediate_operation(&assembler, Aet_CPU_Opcode_Call);
      break;
    case Aet_Assembly_Token_Kind_Ret:
      instr_result = (Aet_Assembler_Instruction_Result){
        .err = Aet_Assembler_Error_None,
        .instr = (u32)(Aet_CPU_Opcode_Ret),
      };
      break;
    default:
      break;
    }

    if (instr_result.err != Aet_Assembler_Error_None) {
      result.err = instr_result.err;
      break;
    }

    output[output_len] = instr_result.instr;
    output_len += 1;
#pragma clang diagnostic pop

    Aet_Assembly_Token end = aet_assembler_consume_token(&assembler);
    if (end.kind != Aet_Assembly_Token_Kind_Newline &&
        end.kind != Aet_Assembly_Token_Kind_EOF) {
      result.err = Aet_Assembler_Error_Invalid_Syntax;
      break;
    }
  }

  // NOTE(nico): realloc the fixed buffer into something dynamically allocated
  if (result.err == Aet_Assembler_Error_None) {
    result.output = make_array(result.output, output_len, allocator);
    memcpy(result.output.items, &output[0], output_len * sizeof(u32));
  }

  return result;
}

Aet_Disassembler_Result
aet_disassemble(Aet_Program program, Allocator allocator) {
  static char buf[4096] = {0};
  static const char *opcode_lookup[Aet_CPU_Opcode_MAX] = {
#define X(name, text) [Aet_CPU_Opcode_##name] = text,
    AET_INSTRUCTIONS(X)
#undef X
  };
  static const char *register_lookup[Aet_Register_MAX] = {
    [Aet_Register_Rx0] = "rx0",
    [Aet_Register_Rx1] = "rx1",
    [Aet_Register_Rx2] = "rx2",
    [Aet_Register_R0] = "r0",
    [Aet_Register_R1] = "r1",
    [Aet_Register_R2] = "r2",
    [Aet_Register_R3] = "r3",
    [Aet_Register_R4] = "r4",
    [Aet_Register_R5] = "r5",
    [Aet_Register_R6] = "r6",
    [Aet_Register_R7] = "r7",
    [Aet_Register_R8] = "r8",
    [Aet_Register_R9] = "r9",
    [Aet_Register_R10] = "r10",
    [Aet_Register_R11] = "r11",
    [Aet_Register_R12] = "r12",
  };

  if (program.len == 0) {
    return (Aet_Disassembler_Result){
      .ok = true,
      .output = from_c_str(""),
    };
  }

  String_Builder builder = make_builder_from_buf(buf, 4096);
  usize current = 0;

  while (current < program.len) {
    u32 instr = array_get(program, current);
    current += 1;

    Aet_CPU_Opcode opcode = (Aet_CPU_Opcode)(instr & 0xff);

    switch (opcode) {
    case Aet_CPU_Opcode_Addi:
    case Aet_CPU_Opcode_Lb:
    case Aet_CPU_Opcode_Lh:
    case Aet_CPU_Opcode_Lw:
    case Aet_CPU_Opcode_Sb:
    case Aet_CPU_Opcode_Sh:
    case Aet_CPU_Opcode_Sw:
    case Aet_CPU_Opcode_Beq:
    case Aet_CPU_Opcode_Bneq:
    case Aet_CPU_Opcode_Blt:
    case Aet_CPU_Opcode_Bgeq:
    case Aet_CPU_Opcode_Bltu:
    case Aet_CPU_Opcode_Bgequ: {
      Aet_Register rd = (Aet_Register)((instr >> 8) & 0x0f);
      Aet_Register rs1 = (Aet_Register)((instr >> 12) & 0x0f);
      i32 immediate = sign_extend_i32((instr >> 16) & 0xffff, 16);

      builder_write(
          &builder,
          "%ss %ss, %ss, %d",
          opcode_lookup[opcode],
          register_lookup[rd],
          register_lookup[rs1],
          immediate
      );
    } break;
    case Aet_CPU_Opcode_Add:
    case Aet_CPU_Opcode_Sub:
    case Aet_CPU_Opcode_Mul:
    case Aet_CPU_Opcode_Div:
    case Aet_CPU_Opcode_And:
    case Aet_CPU_Opcode_Or:
    case Aet_CPU_Opcode_Xor:
    case Aet_CPU_Opcode_Shl:
    case Aet_CPU_Opcode_Shr: {
      Aet_Register rd = (Aet_Register)((instr >> 8) & 0x0f);
      Aet_Register rs1 = (Aet_Register)((instr >> 12) & 0x0f);
      Aet_Register rs2 = (Aet_Register)((instr >> 16) & 0x0f);

      builder_write(
          &builder,
          "%ss %ss, %ss, %ss",
          opcode_lookup[opcode],
          register_lookup[rd],
          register_lookup[rs1],
          register_lookup[rs2]
      );
    } break;
    case Aet_CPU_Opcode_Jmp:
    case Aet_CPU_Opcode_Call: {
      i32 offset = sign_extend_i32(instr >> 8, 24);

      builder_write(&builder, "%ss %d", opcode_lookup[opcode], offset);
    } break;
    case Aet_CPU_Opcode_Ret:
      builder_write(&builder, "%ss", opcode_lookup[opcode]);
      break;
    case Aet_CPU_Opcode_MAX:
      // FIXME(nico): need a recovery path
      assert(false);
      break;
    }

    builder_write_char(&builder, '\n');
  }

  String output = builder_clone_string(&builder, allocator);

  return (Aet_Disassembler_Result){
    .ok = output.len != 0,
    .output = output,
  };
}
