#include "asm.h"

#include "core/array.h"
#include "core/map.h"
#include "core/math.h"
#include "core/runtime.h"
#include "core/strings.h"
#include "core/types.h"
#include "hal.h"

#include <assert.h>
#include <string.h>

/*
  TODO(nico):
  - [Performance] 1.8x for every assemble by lexing only once and relying on a
  token buffer
*/

#define AET_KEYWORD_MAX_LEN 6
#define AET_FIRST_PSEUDO_ROW ((usize)Aet_CPU_Opcode_MAX)

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
      Aet_Assembly_Token_Kind_pseudo_end_
} Aet_Assembly_Token_Kind;

typedef enum Aet_Assembly_Instruction_Variant {
  Aet_Assembly_Instruction_Variant_Invalid,
  Aet_Assembly_Instruction_Variant_Direct,
  Aet_Assembly_Instruction_Variant_Pseudo,
} Aet_Assembly_Instruction_Variant;

#define aet_token_to_instruction_info_index(token)                             \
  ((usize)((token).kind) - Aet_Assembly_Token_Kind_instruction_start_ - 1)
#define aet_token_to_pseudo_info_index(token)                                  \
  ((usize)((token).kind) - Aet_Assembly_Token_Kind_pseudo_start_ - 1)

// NOTE(nico): the assembler's view of AET_INSTRUCTIONS. Indexed by opcode, so
// the numbering in hal.h has to stay dense; the static_assert there enforces
// it. sizeof(text) - 1 is the literal's length at compile time, so no strlen
// happens per keyword comparison.
typedef struct Aet_Assembly_Instruction_Info {
  Aet_Assembly_Instruction_Variant variant;
  const char *text;
  usize len;
  Aet_Assembly_Token_Kind kind;
  Aet_CPU_Opcode opcode;
  Aet_Instruction_Form form;
  Aet_Bit_Extension extension;
  usize instruction_count;
  bool32 label_allowed;
} Aet_Assembly_Instruction_Info;

static const Aet_Assembly_Instruction_Info
    aet_instruction_lookup[Aet_CPU_Opcode_MAX] = {
#define X(name, mnemonic, _opcode, _form, ext, instr_count, _label_allowed)    \
  [(_opcode)] = {                                                              \
    .variant = Aet_Assembly_Instruction_Variant_Direct,                        \
    .text = (mnemonic),                                                        \
    .len = sizeof((mnemonic)) - 1,                                             \
    .kind = Aet_Assembly_Token_Kind_##name,                                    \
    .opcode = Aet_CPU_Opcode_##name,                                           \
    .form = Aet_Instruction_Form_##_form,                                      \
    .extension = Aet_Bit_Extension_##ext,                                      \
    .instruction_count = (instr_count),                                        \
    .label_allowed = (_label_allowed),                                         \
  },
      AET_INSTRUCTIONS(X)
#undef X
};

static const Aet_Assembly_Instruction_Info aet_pseudo_lookup
    [Aet_Assembly_Token_Kind_pseudo_end_ -
     Aet_Assembly_Token_Kind_pseudo_start_] = {
#define X(name, mnemonic, _form, instr_count, _label_allowed)                  \
  [(Aet_Assembly_Token_Kind_##name) - Aet_Assembly_Token_Kind_pseudo_start_ -  \
      1] = {                                                                   \
    .variant = Aet_Assembly_Instruction_Variant_Pseudo,                        \
    .text = (mnemonic),                                                        \
    .len = sizeof((mnemonic)) - 1,                                             \
    .kind = Aet_Assembly_Token_Kind_##name,                                    \
    .form = Aet_Instruction_Form_##_form,                                      \
    .instruction_count = (instr_count),                                        \
    .label_allowed = (_label_allowed),                                         \
  },
      AET_PSEUDO_INSTRUCTIONS(X)
#undef X
};

typedef union Aet_Assembly_Keyword {
  char text[8];
  u64 packed;
} Aet_Assembly_Keyword;

static_assert(
    sizeof(Aet_Assembly_Keyword) == 8,
    "Assembly keyword key must be exactly one word"
);

static const Aet_Assembly_Keyword aet_assembly_keyword_keys[] = {
#define X(name, mnemonic, opcode, form, ext, instr_count, label_allowed)       \
  {.text = (mnemonic)},
  AET_INSTRUCTIONS(X)
#undef X
#define X(name, mnemonic, form, instr_count, label_allowed)                    \
  {.text = (mnemonic)},
      AET_PSEUDO_INSTRUCTIONS(X)
#undef X
};

typedef struct Aet_Assembly_Token {
  Aet_Assembly_Token_Kind kind;
  usize start;
  usize end;
  String lexeme;
} Aet_Assembly_Token;

typedef struct Aet_Assembly_Parse_Info {
  usize instruction_count;
  Open_Map symbol_table;
} Aet_Assembly_Parse_Info;

typedef Result(
    Aet_Assembly_Parse_Info, Aet_Assembler_Error
) Aet_Assembly_Parse_Info_Result;

typedef struct Aet_Assembly_Parser {
  Allocator allocator;
  String_Reader reader;
  Aet_Assembly_Token previous;
  Aet_Assembly_Token current;
  Open_Map symbol_table;
  usize pc;
} Aet_Assembly_Parser;

typedef struct Aet_Assembly_Instruction_Binary {
  u32 bin[16];
  usize len;
} Aet_Assembly_Instruction_Binary;

typedef Result(
    Aet_Assembly_Token_Kind, Aet_Assembler_Error
) Aet_Assembly_Number_Literal_Result;
typedef Result(
    Aet_Assembly_Token, Aet_Assembler_Error
) Aet_Assembly_Token_Result;
typedef Option(Aet_Assembly_Token_Kind) Aet_Assembly_Token_Option;
typedef Result(u32, Aet_Assembler_Error) Aet_Assembly_Immediate_Result;

typedef Result(
    Aet_Assembly_Instruction_Binary, Aet_Assembler_Error
) Aet_Assembly_Instruction_Result;

typedef struct Aet_Immediate_Range {
  i64 min;
  i64 max;
} Aet_Immediate_Range;

static bool32 char_is_letter(char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

static bool32 char_is_whitespace(char c) {
  return c == '\r' || c == ' ' || c == '\b' || c == '\t';
}

static bool32 aet_assembler_starts_number(String_Reader *reader, char c) {
  if (char_is_number(c)) {
    return true;
  }

  return c == '-' && !string_reader_is_eof(reader) &&
         char_is_number(string_reader_peek(reader));
}

static bool32 aet_assembler_ends_number(char c) {
  return char_is_whitespace(c) || c == ';' || c == ',' || c == '\n';
}

static inline u64 aet_assembly_pack_keyword(String str) {
  if (str.len > AET_KEYWORD_MAX_LEN) {
    return 0;
  }

  u64 k;
  u32 half;
  u16 pair;
  const char *p = str.data;

  switch (str.len) {
  case 6:
    memcpy(&half, p, 4);
    memcpy(&pair, p + 4, 2);
    k = (u64)half | ((u64)pair << 32);
    break;
  case 5:
    memcpy(&half, p, 4);
    k = (u64)half | ((u64)(byte)p[4] << 32);
    break;
  case 4:
    memcpy(&half, p, 4);
    k = (u64)half;
    break;
  case 3:
    memcpy(&pair, p, 2);
    k = (u64)pair | ((u64)(byte)p[2] << 16);
    break;
  case 2:
    memcpy(&pair, p, 2);
    k = (u64)pair;
    break;
  case 1:
    k = (u64)(byte)p[0];
    break;
  default:
    k = 0;
    break;
  }

  return k;
}

static i32 aet_assembly_lookup_keyword_index(String str) {
  u64 key = aet_assembly_pack_keyword(str);
  if (key == 0) {
    return -1;
  }

  for (usize i = 0; i < countof(aet_assembly_keyword_keys); i += 1) {
    if (aet_assembly_keyword_keys[i].packed == key) {
      return (i32)i;
    }
  }

  return -1;
}

static Aet_Assembly_Token_Kind aet_assembly_keyword(usize index) {
  return index < AET_FIRST_PSEUDO_ROW
             ? (Aet_Assembly_Token_Kind)(Aet_Assembly_Token_Kind_instruction_start_ +
                                         1 + index)
             : (Aet_Assembly_Token_Kind)(Aet_Assembly_Token_Kind_pseudo_start_ +
                                         1 + index - AET_FIRST_PSEUDO_ROW);
}

static Aet_Assembly_Instruction_Variant
aet_assembly_token_is_instruction(Aet_Assembly_Token token) {
  if ((token.kind > Aet_Assembly_Token_Kind_instruction_start_ &&
       token.kind < Aet_Assembly_Token_Kind_instruction_end_)) {
    return Aet_Assembly_Instruction_Variant_Direct;
  } else if (
      token.kind > Aet_Assembly_Token_Kind_pseudo_start_ &&
      token.kind < Aet_Assembly_Token_Kind_pseudo_end_
  ) {
    return Aet_Assembly_Instruction_Variant_Pseudo;
  }

  return Aet_Assembly_Instruction_Variant_Invalid;
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
    if (!char_is_whitespace(n)) {
      break;
    }

    string_reader_advance(reader);
  }
}

static Aet_Assembly_Number_Literal_Result
aet_assembler_lex_number(String_Reader *reader) {
  char leading = reader->input.data[reader->current - 1];
  if (leading == '-') {
    // NOTE(nico): This is safe because the starts_number proc already validated
    // that there is another char after. This is the first number after the '-'
    leading = string_reader_advance(reader);
  }

  if (string_reader_is_eof(reader)) {
    return ok(
        Aet_Assembly_Number_Literal_Result,
        Aet_Assembly_Token_Kind_Integer_Literal
    );
  }

  Aet_Assembly_Token_Kind result = Aet_Assembly_Token_Kind_Integer_Literal;
  char shape = string_reader_peek(reader);

  switch (shape) {
  case 'x':
  case 'X': {
    if (leading != '0') {
      return err(
          Aet_Assembly_Number_Literal_Result,
          Aet_Assembler_Error_Malformed_Hex_Literal
      );
    }

    string_reader_advance(reader);

    bool32 has_hex_part = false;

    while (true) {
      if (string_reader_is_eof(reader)) {
        break;
      }
      char n = string_reader_peek(reader);

      if (aet_assembler_ends_number(n)) {
        break;
      }

      if (!char_is_hex(n)) {
        return err(
            Aet_Assembly_Number_Literal_Result,
            Aet_Assembler_Error_Malformed_Hex_Literal
        );
      }

      has_hex_part = true;
      string_reader_advance(reader);
    }

    if (!has_hex_part) {
      return err(
          Aet_Assembly_Number_Literal_Result,
          Aet_Assembler_Error_Malformed_Hex_Literal
      );
    }
  } break;
  case 'b':
  case 'B': {
    if (leading != '0') {
      return err(
          Aet_Assembly_Number_Literal_Result,
          Aet_Assembler_Error_Malformed_Binary_Literal
      );
    }

    string_reader_advance(reader);

    bool32 has_binary_part = false;
    while (true) {
      if (string_reader_is_eof(reader)) {
        break;
      }

      char n = string_reader_peek(reader);

      if (aet_assembler_ends_number(n)) {
        break;
      }

      if (!char_is_binary(n)) {
        return err(
            Aet_Assembly_Number_Literal_Result,
            Aet_Assembler_Error_Malformed_Binary_Literal
        );
      }

      has_binary_part = true;
      string_reader_advance(reader);
    }

    if (!has_binary_part) {
      return err(
          Aet_Assembly_Number_Literal_Result,
          Aet_Assembler_Error_Malformed_Binary_Literal
      );
    }
  } break;
  default: {
    bool32 has_fract_char = false;
    bool32 has_fract_part = false;

    while (true) {
      if (string_reader_is_eof(reader)) {
        break;
      }

      char n = string_reader_peek(reader);
      if (aet_assembler_ends_number(n)) {
        break;
      }

      if (n == '.') {
        if (has_fract_char) {
          return err(
              Aet_Assembly_Number_Literal_Result,
              Aet_Assembler_Error_Malformed_Decimal_Literal
          );
        }

        has_fract_char = true;
        result = Aet_Assembly_Token_Kind_Float_Literal;
        string_reader_advance(reader);
        continue;
      }

      if (!char_is_number(n)) {
        return err(
            Aet_Assembly_Number_Literal_Result,
            Aet_Assembler_Error_Malformed_Decimal_Literal
        );
      }

      if (has_fract_char) {
        has_fract_part = true;
      }
      string_reader_advance(reader);
    }

    if (has_fract_char && !has_fract_part) {
      return err(
          Aet_Assembly_Number_Literal_Result,
          Aet_Assembler_Error_Malformed_Decimal_Literal
      );
    }
  } break;
  }

  return ok(Aet_Assembly_Number_Literal_Result, result);
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

// static Aet_Assembly_Instruction_Info_Option
// aet_assembler_match_instruction(String str) {
//   for (usize i = 0; i < countof(aet_instruction_lookup); i += 1) {
//     const Aet_Assembly_Instruction_Info *keyword =
//     &aet_instruction_lookup[i];

//     if (keyword->len == str.len &&
//         memcmp(keyword->text, str.data, str.len) == 0) {
//       return some(Aet_Assembly_Instruction_Info_Option, *keyword);
//     }
//   }

//   return none(Aet_Assembly_Instruction_Info_Option);
// }

static Aet_Assembly_Token_Option aet_assembler_match_register(String str) {
  if (str.len < 2 || str.len > 3 ||
      (str.data[0] != 'r' && str.data[0] != 'R')) {
    return none(Aet_Assembly_Token_Option);
  }

  if (str.data[1] == 'x' && str.len == 3) {
    switch (str.data[2]) {
    case '0':
      return some(Aet_Assembly_Token_Option, Aet_Assembly_Token_Kind_Rx0);
    case '1':
      return some(Aet_Assembly_Token_Option, Aet_Assembly_Token_Kind_Rx1);
    case '2':
      return some(Aet_Assembly_Token_Option, Aet_Assembly_Token_Kind_Rx2);
    default:
      return none(Aet_Assembly_Token_Option);
    }
  }

  u32 n = 0;
  for (usize i = 1; i < str.len; i += 1) {
    if (!char_is_number(str.data[i])) {
      return none(Aet_Assembly_Token_Option);
    }

    n = n * 10 + (u32)(str.data[i] - '0');
  }

  if (n > 12) {
    return none(Aet_Assembly_Token_Option);
  }

  Aet_Assembly_Token_Kind reg = (Aet_Assembly_Token_Kind_R0 + n);
  return some(Aet_Assembly_Token_Option, reg);
}

static Aet_Assembly_Token_Result
aet_assembler_next_token(String_Reader *reader) {
  aet_assembler_skip_whitespace(reader);
  if (string_reader_is_eof(reader)) {
    return ok(
        Aet_Assembly_Token_Result,
        ((Aet_Assembly_Token){
          .kind = Aet_Assembly_Token_Kind_EOF,
          .start = reader->current,
          .end = reader->current,
          .lexeme = from_c_str(""),
        })
    );
  }

  Aet_Assembly_Token token = {
    .start = reader->current,
  };
  Aet_Assembler_Error err = Aet_Assembler_Error_None;

  char c = string_reader_advance(reader);
  switch (c) {
  case '\n':
    token.kind = Aet_Assembly_Token_Kind_Newline;
    break;
  case ',':
    token.kind = Aet_Assembly_Token_Kind_Comma;
    break;
  case ':':
    token.kind = Aet_Assembly_Token_Kind_Colon;
    break;
  case ';': {
    token.kind = Aet_Assembly_Token_Kind_Comment;
    while (true) {
      if (string_reader_is_eof(reader)) {
        break;
      }

      char next = string_reader_peek(reader);
      if (next == '\n') {
        break;
      }

      string_reader_advance(reader);
    }
  } break;
  default: {
    if (aet_assembler_starts_number(reader, c)) {
      token.kind =
          try(Aet_Assembly_Token_Result, aet_assembler_lex_number(reader));
    } else if (char_is_letter(c)) {
      Aet_Assembler_Error identifier_err =
          aet_assembler_lex_identifier(reader, true);

      if (identifier_err != Aet_Assembler_Error_None) {
        err = identifier_err;
        goto exit;
      }

      // TODO(nico): needs to fetch the pseudo instructions too
      String identifier =
          string_slice(reader->input, token.start, reader->current);

      i32 keyword_index = aet_assembly_lookup_keyword_index(identifier);
      if (keyword_index >= 0) {
        token.kind = aet_assembly_keyword((usize)keyword_index);
      } else {
        Aet_Assembly_Token_Option reg_opt =
            aet_assembler_match_register(identifier);

        if (reg_opt.some) {
          token.kind = reg_opt.value;
        } else {
          token.kind = Aet_Assembly_Token_Kind_Identifier;
        }
      }
    } else {
      err = Aet_Assembler_Error_Invalid_Identifier;
      goto exit;
    }
  } break;
  }

  token.end = reader->current;
  token.lexeme = string_slice(reader->input, token.start, token.end);

exit:
  return err == Aet_Assembler_Error_None ? ok(Aet_Assembly_Token_Result, token)
                                         : err(Aet_Assembly_Token_Result, err);
}

static Aet_Assembly_Token_Result
aet_assembler_consume_token(Aet_Assembly_Parser *parser) {
  parser->previous = parser->current;

  Aet_Assembly_Token_Result token_result =
      aet_assembler_next_token(&parser->reader);
  if (token_result.ok) {
    parser->current = token_result.value;
  }

  return token_result;
}

static Aet_Assembly_Token_Result aet_assembler_expect_token(
    Aet_Assembly_Parser *parser, Aet_Assembly_Token_Kind kind
) {
  Aet_Assembly_Token next =
      try(Aet_Assembly_Token_Result, aet_assembler_consume_token(parser));

  return next.kind == kind ? ok(Aet_Assembly_Token_Result, next)
                           : err(Aet_Assembly_Token_Result,
                                 Aet_Assembler_Error_Invalid_Syntax);
}

// NOTE(nico): the accepted range of an immediate field follows entirely from
// its extension and its width, so the encoders below never spell out bounds.
static Aet_Immediate_Range
aet_immediate_range(Aet_Bit_Extension extension, u32 bits) {
  switch (extension) {
  case Aet_Bit_Extension_Signed:
    return (Aet_Immediate_Range){
      .min = -(1ll << (bits - 1)),
      .max = (1ll << (bits - 1)) - 1,
    };
  case Aet_Bit_Extension_Zero:
    return (Aet_Immediate_Range){.min = 0, .max = (1ll << bits) - 1};
  case Aet_Bit_Extension_None:
    break;
  }

  // NOTE(nico): reachable only if an instruction whose table row says it has
  // no immediate is routed through one of the immediate-bearing encoders.
  assert(false);
  return (Aet_Immediate_Range){.min = 0, .max = 0};
}

static Aet_Assembly_Immediate_Result aet_parse_immediate(
    Aet_Assembly_Parser *parser, Aet_Assembly_Instruction_Info *info, u32 length
) {
  if (length != 16 && length != 24) {
    return err(
        Aet_Assembly_Immediate_Result, Aet_Assembler_Error_Internal_Failure
    );
  }

  Aet_Assembly_Token token =
      try(Aet_Assembly_Immediate_Result, aet_assembler_consume_token(parser));

  i64 value = 0;
  if (token.kind == Aet_Assembly_Token_Kind_Integer_Literal) {
    if (!string_to_i64(token.lexeme, &value)) {
      return err(
          Aet_Assembly_Immediate_Result,
          Aet_Assembler_Error_Invalid_Immediate_Value
      );
    }
  } else if (
      token.kind == Aet_Assembly_Token_Kind_Identifier && info->label_allowed
  ) {
    usize *label_addr_ptr =
        (usize *)open_map_get(parser->symbol_table, token.lexeme);
    if (label_addr_ptr == nullptr) {
      return err(
          Aet_Assembly_Immediate_Result, Aet_Assembler_Error_Unknown_Symbol
      );
    }

    value = (i64)*label_addr_ptr - (i64)parser->pc;
  } else {
    return err(
        Aet_Assembly_Immediate_Result, Aet_Assembler_Error_Invalid_Syntax
    );
  }

  Aet_Immediate_Range range = aet_immediate_range(info->extension, length);

  if (value < range.min || value > range.max) {
    return err(
        Aet_Assembly_Immediate_Result,
        Aet_Assembler_Error_Invalid_Immediate_Value
    );
  }

  return ok(Aet_Assembly_Immediate_Result, (u32)value);
}

static Aet_Assembly_Instruction_Result
aet_assemble_rrr_op(Aet_Assembly_Parser *parser, Aet_CPU_Opcode op) {
  Aet_Assembly_Token arg1 =
      try(Aet_Assembly_Instruction_Result, aet_assembler_consume_token(parser));
  if (!aet_assembly_token_is_register(arg1)) {
    return err(
        Aet_Assembly_Instruction_Result, Aet_Assembler_Error_Invalid_Syntax
    );
  }

  try(Aet_Assembly_Instruction_Result,
      aet_assembler_expect_token(parser, Aet_Assembly_Token_Kind_Comma));

  Aet_Assembly_Token arg2 =
      try(Aet_Assembly_Instruction_Result, aet_assembler_consume_token(parser));
  if (!aet_assembly_token_is_register(arg2)) {
    return err(
        Aet_Assembly_Instruction_Result, Aet_Assembler_Error_Invalid_Syntax
    );
  }

  try(Aet_Assembly_Instruction_Result,
      aet_assembler_expect_token(parser, Aet_Assembly_Token_Kind_Comma));

  Aet_Assembly_Token arg3 =
      try(Aet_Assembly_Instruction_Result, aet_assembler_consume_token(parser));
  if (!aet_assembly_token_is_register(arg3)) {
    return err(
        Aet_Assembly_Instruction_Result, Aet_Assembler_Error_Invalid_Syntax
    );
  }

  Aet_Register rd = aet_assembly_token_to_register(arg1);
  Aet_Register rs1 = aet_assembly_token_to_register(arg2);
  Aet_Register rs2 = aet_assembly_token_to_register(arg3);

  return ok(
      Aet_Assembly_Instruction_Result,
      ((Aet_Assembly_Instruction_Binary){
        .bin =
            {
              [0] = (u32)(op) | ((u32)rd << 8) | ((u32)rs1 << 12) |
                    ((u32)rs2 << 16),
            },
        .len = 1,
      })
  );
}

static Aet_Assembly_Instruction_Result
aet_assemble_rri_op(Aet_Assembly_Parser *parser, Aet_CPU_Opcode op) {
  Aet_Assembly_Instruction_Info info = aet_instruction_lookup[op];

  Aet_Assembly_Token arg1 =
      try(Aet_Assembly_Instruction_Result, aet_assembler_consume_token(parser));
  if (!aet_assembly_token_is_register(arg1)) {
    return err(
        Aet_Assembly_Instruction_Result, Aet_Assembler_Error_Invalid_Syntax
    );
  }

  try(Aet_Assembly_Instruction_Result,
      aet_assembler_expect_token(parser, Aet_Assembly_Token_Kind_Comma));

  Aet_Assembly_Token arg2 =
      try(Aet_Assembly_Instruction_Result, aet_assembler_consume_token(parser));
  if (!aet_assembly_token_is_register(arg2)) {
    return err(
        Aet_Assembly_Instruction_Result, Aet_Assembler_Error_Invalid_Syntax
    );
  }

  try(Aet_Assembly_Instruction_Result,
      aet_assembler_expect_token(parser, Aet_Assembly_Token_Kind_Comma));

  Aet_Register rd = aet_assembly_token_to_register(arg1);
  Aet_Register rs1 = aet_assembly_token_to_register(arg2);

  u32 immediate =
      try(Aet_Assembly_Instruction_Result,
          aet_parse_immediate(parser, &info, 16));

  // u32 instr = (u32)(op) | ((u32)rd << 8) | ((u32)rs1 << 12) | (immediate <<
  // 16);
  return ok(
      Aet_Assembly_Instruction_Result,
      ((Aet_Assembly_Instruction_Binary){
        .bin =
            {
              [0] = (u32)(op) | ((u32)rd << 8) | ((u32)rs1 << 12) |
                    (immediate << 16),
            },
        .len = 1,
      })
  );
}

static Aet_Assembly_Instruction_Result
aet_assemble_ri_op(Aet_Assembly_Parser *parser, Aet_CPU_Opcode op) {
  Aet_Assembly_Instruction_Info info = aet_instruction_lookup[op];

  Aet_Assembly_Token arg1 =
      try(Aet_Assembly_Instruction_Result, aet_assembler_consume_token(parser));
  if (!aet_assembly_token_is_register(arg1)) {
    return err(
        Aet_Assembly_Instruction_Result, Aet_Assembler_Error_Invalid_Syntax
    );
  }

  try(Aet_Assembly_Instruction_Result,
      aet_assembler_expect_token(parser, Aet_Assembly_Token_Kind_Comma));

  Aet_Register rd = aet_assembly_token_to_register(arg1);

  u32 immediate =
      try(Aet_Assembly_Instruction_Result,
          aet_parse_immediate(parser, &info, 16));

  // u32 instr = (u32)(op) | ((u32)rd << 8) | (immediate << 16);
  return ok(
      Aet_Assembly_Instruction_Result,
      ((Aet_Assembly_Instruction_Binary){
        .bin =
            {
              [0] = (u32)(op) | ((u32)rd << 8) | (immediate << 16),
            },
        .len = 1,
      })
  );
}

static Aet_Assembly_Instruction_Result
aet_assemble_i_op(Aet_Assembly_Parser *parser, Aet_CPU_Opcode op) {
  Aet_Assembly_Instruction_Info info = aet_instruction_lookup[op];

  u32 immediate =
      try(Aet_Assembly_Instruction_Result,
          aet_parse_immediate(parser, &info, 24));

  return ok(
      Aet_Assembly_Instruction_Result,
      ((Aet_Assembly_Instruction_Binary){
        .bin =
            {
              [0] = (u32)(op) | (immediate << 8),
            },
        .len = 1,
      })
  );
}

static Aet_Assembly_Instruction_Result
aet_assemble_direct_instruction(Aet_Assembly_Parser *parser) {
  // Aet_Assembly_Instruction_Info_Option info_opt =
  //     aet_assembler_match_instruction(parser->current.lexeme);
  // if (!info_opt.some) {
  //   return err(
  //       Aet_Assembly_Instruction_Result, Aet_Assembler_Error_Internal_Failure
  //   );
  // }
  // Aet_Assembly_Instruction_Info info = info_opt.value;

  usize lookup_index = aet_token_to_instruction_info_index(parser->current);
  Aet_Assembly_Instruction_Info info = aet_instruction_lookup[lookup_index];
  Aet_Assembly_Instruction_Result result = {0};

  switch (info.form) {
  case Aet_Instruction_Form_None:
    result =
        ok(Aet_Assembly_Instruction_Result,
           ((Aet_Assembly_Instruction_Binary){
             .bin = {[0] = (u32)info.opcode},
             .len = 1,
           }));
    break;
  case Aet_Instruction_Form_RRR:
    result = aet_assemble_rrr_op(parser, info.opcode);
    break;
  case Aet_Instruction_Form_RRI:
    result = aet_assemble_rri_op(parser, info.opcode);
    break;
  case Aet_Instruction_Form_RI:
    result = aet_assemble_ri_op(parser, info.opcode);
    break;
  case Aet_Instruction_Form_I:
    result = aet_assemble_i_op(parser, info.opcode);
    break;
  }

#if defined(DEBUG)
  if (result.ok) {
    assert(info.instruction_count == result.value.len);
  }
#endif

  return result;
}

static Aet_Assembly_Instruction_Result
aet_assemble_pseudo_instruction(Aet_Assembly_Parser *parser) {
  Aet_Assembly_Instruction_Result result = {0};

  // NOTE(nico): rawdogging the parsing for now since there is only 1
  // pseudo instruction

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wswitch-enum"
  switch (parser->current.kind) {
  case Aet_Assembly_Token_Kind_Lf: {
    Aet_Assembly_Token arg1 =
        try(Aet_Assembly_Instruction_Result,
            aet_assembler_consume_token(parser));
    if (!aet_assembly_token_is_register(arg1)) {
      return err(
          Aet_Assembly_Instruction_Result, Aet_Assembler_Error_Invalid_Syntax
      );
    }

    try(Aet_Assembly_Instruction_Result,
        aet_assembler_expect_token(parser, Aet_Assembly_Token_Kind_Comma));

    Aet_Register rd = aet_assembly_token_to_register(arg1);

    Aet_Assembly_Token immediate_token =
        try(Aet_Assembly_Instruction_Result,
            aet_assembler_consume_token(parser));
    f32 immediate = 0.f;
    if (!string_to_f32(immediate_token.lexeme, &immediate)) {
      return err(
          Aet_Assembly_Instruction_Result,
          Aet_Assembler_Error_Invalid_Immediate_Value
      );
    }

    u32 bits = aet_f32_to_u32(immediate);

    result =
        ok(Aet_Assembly_Instruction_Result,
           ((Aet_Assembly_Instruction_Binary){
             .bin =
                 {
                   [0] = (u32)Aet_CPU_Opcode_Lui | ((u32)rd << 8) |
                         (bits & 0xffff0000),
                   [1] = ((u32)Aet_CPU_Opcode_Ori) | ((u32)rd << 8) |
                         ((u32)rd << 12) | ((bits & 0xffff) << 16),
                 },
             .len = 2
           }));
  } break;
  default:
    result =
        err(Aet_Assembly_Instruction_Result,
            Aet_Assembler_Error_Invalid_Syntax);
  }
#pragma clang diagnostic pop

  return result;
}

static Aet_Assembly_Parse_Info_Result
aet_assembler_reserve_size_and_build_symbol_table(
    String source, Allocator allocator
) {
  usize instruction_count = 0;
  // NOTE(nico): need a better heuristic for the initial size
  Open_Map symbol_table = make_string_open_map(usize, 32, allocator);
  if (symbol_table == nullptr) {
    return err(
        Aet_Assembly_Parse_Info_Result, Aet_Assembler_Error_Internal_Failure
    );
  }

  Aet_Assembly_Parser parser = {
    .reader = {.input = source},
  };
  Aet_Assembler_Error error = Aet_Assembler_Error_None;

  while (true) {
    Aet_Assembly_Token_Result token_result =
        aet_assembler_consume_token(&parser);
    if (!token_result.ok) {
      error = token_result.error;
      goto exit;
    }

    Aet_Assembly_Token token = token_result.value;
    if (token.kind == Aet_Assembly_Token_Kind_EOF) {
      break;
    }

    if (token.kind == Aet_Assembly_Token_Kind_Newline ||
        token.kind == Aet_Assembly_Token_Kind_Comment) {
      continue;
    }

    if (token.kind == Aet_Assembly_Token_Kind_Identifier) {
      if (!aet_assembler_expect_token(&parser, Aet_Assembly_Token_Kind_Colon)
               .ok) {
        error = Aet_Assembler_Error_Invalid_Syntax;
        goto exit;
      }
      if (open_map_get(symbol_table, token.lexeme) != nullptr) {
        error = Aet_Assembler_Error_Duplicate_Symbol;
        goto exit;
      }

      open_map_set(symbol_table, token.lexeme, instruction_count);
      continue;
    }

    Aet_Assembly_Instruction_Variant variant =
        aet_assembly_token_is_instruction(token);

    switch (variant) {
    case Aet_Assembly_Instruction_Variant_Invalid:
      error = Aet_Assembler_Error_Invalid_Syntax;
      goto exit;
    case Aet_Assembly_Instruction_Variant_Direct: {
      usize lookup_index = aet_token_to_instruction_info_index(token);
      instruction_count +=
          aet_instruction_lookup[lookup_index].instruction_count;
    } break;
    case Aet_Assembly_Instruction_Variant_Pseudo: {
      usize lookup_index = aet_token_to_pseudo_info_index(token);
      instruction_count += aet_pseudo_lookup[lookup_index].instruction_count;
    } break;
    }

    while (true) {
      Aet_Assembly_Token_Result next_result =
          aet_assembler_consume_token(&parser);
      if (!next_result.ok) {
        error = next_result.error;
        goto exit;
      }

      if (next_result.value.kind == Aet_Assembly_Token_Kind_Newline ||
          next_result.value.kind == Aet_Assembly_Token_Kind_EOF) {
        break;
      }
    }
  }

exit:
  if (error != Aet_Assembler_Error_None) {
    delete_open_map(symbol_table);
    return err(Aet_Assembly_Parse_Info_Result, error);
  } else {
    return ok(
        Aet_Assembly_Parse_Info_Result,
        ((Aet_Assembly_Parse_Info){
          .instruction_count = instruction_count,
          .symbol_table = symbol_table,
        })
    );
  }
}

/*
  NOTE(nico):
  This assembler is working in two passes:
  - The first one measure a program's size and collect all the symbols (only
  labels for now)
  - The second one handles the code generation

  Some work is done in both passes and is a bit redundant. Maybe I will find
  a way to fold it in the future but for now this is good enough
*/
Aet_Assembler_Result aet_assemble(String source, Allocator allocator) {
  Aet_Assembly_Parser parser = {
    .allocator = allocator,
    .reader = {
      .input = source,
    }
  };

  Aet_Assembly_Parse_Info parse_info =
      try(Aet_Assembler_Result,
          aet_assembler_reserve_size_and_build_symbol_table(source, allocator));
  defer {
    delete_open_map(parse_info.symbol_table);
  };

  parser.symbol_table = parse_info.symbol_table;
  Aet_Program output =
      make_array(output, parse_info.instruction_count, allocator);
  if (output.items == nullptr) {
    return err(Aet_Assembler_Result, Aet_Assembler_Error_Internal_Failure);
  }
  Aet_Assembler_Error error = Aet_Assembler_Error_None;

  while (true) {
    Aet_Assembly_Token_Result token_result =
        aet_assembler_consume_token(&parser);
    if (!token_result.ok) {
      error = token_result.error;
      break;
    }

    Aet_Assembly_Token token = token_result.value;

    if (token.kind == Aet_Assembly_Token_Kind_EOF) {
      break;
    }

    if (token.kind == Aet_Assembly_Token_Kind_Newline ||
        token.kind == Aet_Assembly_Token_Kind_Comment) {
      continue;
    }

    if (token.kind == Aet_Assembly_Token_Kind_Identifier) {
      Aet_Assembly_Token_Result colon_result =
          aet_assembler_expect_token(&parser, Aet_Assembly_Token_Kind_Colon);
      if (!colon_result.ok) {
        error = Aet_Assembler_Error_Invalid_Syntax;
        break;
      }
      continue;
    }

    Aet_Assembly_Instruction_Variant variant =
        aet_assembly_token_is_instruction(token);

    Aet_Assembly_Instruction_Result instr_result = {0};

    switch (variant) {
    case Aet_Assembly_Instruction_Variant_Direct:
      instr_result = aet_assemble_direct_instruction(&parser);
      break;
    case Aet_Assembly_Instruction_Variant_Pseudo:
      instr_result = aet_assemble_pseudo_instruction(&parser);
      break;
    case Aet_Assembly_Instruction_Variant_Invalid:
      error = Aet_Assembler_Error_Invalid_Syntax;
      break;
    }

    if (!instr_result.ok) {
      error = instr_result.error;
      break;
    }

    Aet_Assembly_Instruction_Binary *instr = &instr_result.value;

    memcpy(
        output.items + parser.pc, instr->bin, instr->len * sizeof(instr->bin[0])
    );
    parser.pc += instr->len;

    Aet_Assembly_Token_Result end_result = aet_assembler_consume_token(&parser);
    if (!end_result.ok) {
      error = end_result.error;
      break;
    }

    if (end_result.value.kind != Aet_Assembly_Token_Kind_Newline &&
        end_result.value.kind != Aet_Assembly_Token_Kind_EOF &&
        end_result.value.kind != Aet_Assembly_Token_Kind_Comment) {
      error = Aet_Assembler_Error_Invalid_Syntax;
      break;
    }
  }

  if (error == Aet_Assembler_Error_None) {
    return ok(Aet_Assembler_Result, output);
  } else {
    delete_array(output);
    return err(Aet_Assembler_Result, error);
  }
}

Aet_Disassembler_Result
aet_disassemble(Aet_Program program, Allocator allocator) {
  static char buf[4096] = {0};
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
    return err(Aet_Disassembler_Result, Aet_Disassembler_Error_Invalid_Program);
  }

  // FIXME(nico): Need to design a way to have a scratch buffer that can extend.
  // The problem with passing a temp allocator for those allocation (which
  // should be arenas):
  // - There is no realloc in the allocators for now
  // - Arena realloc would just copy the memory over, leading to a lot of memory
  // overuse.
  // I think passing a temp allocator is still the way to go. However,
  // it should not rely on a standard dynamic array.
  // It should be a bloc allocation with a next ptr, and then reconstruction
  // pass at the end.
  String_Builder builder = make_builder_from_buf(buf, 4096);
  usize current = 0;

  while (current < program.len) {
    u32 instr = array_get(program, current);
    current += 1;

    Aet_CPU_Opcode opcode = (Aet_CPU_Opcode)(instr & 0xff);

    switch (opcode) {
    case Aet_CPU_Opcode_Addi:
    case Aet_CPU_Opcode_Lb:
    case Aet_CPU_Opcode_Lbu:
    case Aet_CPU_Opcode_Lh:
    case Aet_CPU_Opcode_Lhu:
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
          aet_instruction_lookup[opcode].text,
          register_lookup[rd],
          register_lookup[rs1],
          immediate
      );
    } break;
    case Aet_CPU_Opcode_Lui: {
      Aet_Register rd = (Aet_Register)((instr >> 8) & 0x0f);
      i32 immediate = (instr >> 16) & 0xffff;

      builder_write(
          &builder,
          "%ss %ss, %d",
          aet_instruction_lookup[opcode].text,
          register_lookup[rd],
          immediate
      );
    } break;
    case Aet_CPU_Opcode_Ori: {
      Aet_Register rd = (Aet_Register)((instr >> 8) & 0x0f);
      Aet_Register rs1 = (Aet_Register)((instr >> 12) & 0x0f);
      u32 immediate = (instr >> 16) & 0xffff;

      builder_write(
          &builder,
          "%ss %ss, %ss, %d",
          aet_instruction_lookup[opcode].text,
          register_lookup[rd],
          register_lookup[rs1],
          immediate
      );
    } break;
    case Aet_CPU_Opcode_Add:
    case Aet_CPU_Opcode_Sub:
    case Aet_CPU_Opcode_Mul:
    case Aet_CPU_Opcode_Div:
    case Aet_CPU_Opcode_Divu:
    case Aet_CPU_Opcode_And:
    case Aet_CPU_Opcode_Or:
    case Aet_CPU_Opcode_Xor:
    case Aet_CPU_Opcode_Shl:
    case Aet_CPU_Opcode_Shr:
    case Aet_CPU_Opcode_Addf:
    case Aet_CPU_Opcode_Subf:
    case Aet_CPU_Opcode_Mulf:
    case Aet_CPU_Opcode_Divf: {
      Aet_Register rd = (Aet_Register)((instr >> 8) & 0x0f);
      Aet_Register rs1 = (Aet_Register)((instr >> 12) & 0x0f);
      Aet_Register rs2 = (Aet_Register)((instr >> 16) & 0x0f);

      builder_write(
          &builder,
          "%ss %ss, %ss, %ss",
          aet_instruction_lookup[opcode].text,
          register_lookup[rd],
          register_lookup[rs1],
          register_lookup[rs2]
      );
    } break;
    case Aet_CPU_Opcode_Jmp:
    case Aet_CPU_Opcode_Call: {
      i32 offset = sign_extend_i32(instr >> 8, 24);

      builder_write(
          &builder, "%ss %d", aet_instruction_lookup[opcode].text, offset
      );
    } break;
    case Aet_CPU_Opcode_Ret:
      builder_write(&builder, "%ss", aet_instruction_lookup[opcode].text);
      break;
    case Aet_CPU_Opcode_MAX:
    default:
      return err(
          Aet_Disassembler_Result, Aet_Disassembler_Error_Invalid_Opcode
      );
    }

    builder_write_char(&builder, '\n');
  }

  String output = builder_clone_string(&builder, allocator);
  return ok(Aet_Disassembler_Result, output);
}
