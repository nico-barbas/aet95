#include "tools.h"

#include "asm.h"
#include "core/strings.h"
#include "hal.h"

#include <stdio.h>

static const Syntax_Token_Kind
    aet_asm_kind_lookup[Aet_Assembly_Token_Kind_MAX] = {
      [Aet_Assembly_Token_Kind_EOF] = Syntax_Token_Kind_EOF,
      [Aet_Assembly_Token_Kind_Comment] = Syntax_Token_Kind_Comment,
      [Aet_Assembly_Token_Kind_Identifier] = Syntax_Token_Kind_Identifier,
      [Aet_Assembly_Token_Kind_Comma] = Syntax_Token_Kind_Punctuation,
      [Aet_Assembly_Token_Kind_Colon] = Syntax_Token_Kind_Punctuation,
      [Aet_Assembly_Token_Kind_Integer_Literal] = Syntax_Token_Kind_Literal,
      [Aet_Assembly_Token_Kind_Float_Literal] = Syntax_Token_Kind_Literal,
      [Aet_Assembly_Token_Kind_Rx0] = Syntax_Token_Kind_Identifier,
      [Aet_Assembly_Token_Kind_Rx1] = Syntax_Token_Kind_Identifier,
      [Aet_Assembly_Token_Kind_Rx2] = Syntax_Token_Kind_Identifier,
      [Aet_Assembly_Token_Kind_R0] = Syntax_Token_Kind_Identifier,
      [Aet_Assembly_Token_Kind_R1] = Syntax_Token_Kind_Identifier,
      [Aet_Assembly_Token_Kind_R2] = Syntax_Token_Kind_Identifier,
      [Aet_Assembly_Token_Kind_R3] = Syntax_Token_Kind_Identifier,
      [Aet_Assembly_Token_Kind_R4] = Syntax_Token_Kind_Identifier,
      [Aet_Assembly_Token_Kind_R5] = Syntax_Token_Kind_Identifier,
      [Aet_Assembly_Token_Kind_R6] = Syntax_Token_Kind_Identifier,
      [Aet_Assembly_Token_Kind_R7] = Syntax_Token_Kind_Identifier,
      [Aet_Assembly_Token_Kind_R8] = Syntax_Token_Kind_Identifier,
      [Aet_Assembly_Token_Kind_R9] = Syntax_Token_Kind_Identifier,
      [Aet_Assembly_Token_Kind_R10] = Syntax_Token_Kind_Identifier,
      [Aet_Assembly_Token_Kind_R11] = Syntax_Token_Kind_Identifier,
      [Aet_Assembly_Token_Kind_R12] = Syntax_Token_Kind_Identifier,
#define X(name, text, opcode, form, ext, instr_count, label_allowed)           \
  [Aet_Assembly_Token_Kind_##name] = Syntax_Token_Kind_Keyword,
      AET_INSTRUCTIONS(X)
#undef X
#define X(name, mnemonic, _form, instr_count, _label_allowed)                  \
  [(Aet_Assembly_Token_Kind_##name)] = Syntax_Token_Kind_Keyword,
          AET_PSEUDO_INSTRUCTIONS(X)
#undef X
};

static Syntax_Token
aet_asm_syntax_highlighter_lex_next_token(Syntax_Highlighter *highlighter) {
  // NOTE(nico): This interface cannot fail and needs to collapse all the error
  // reported by the lexer into a valid token
  usize start = highlighter->reader.current;
  Aet_Assembly_Token_Result asm_token_result =
      aet_assembler_next_token(&highlighter->reader);

  if (!asm_token_result.ok) {
    return (Syntax_Token){
      .kind = Syntax_Token_Kind_Error,
      .start = start,
      .end = highlighter->reader.current,
      .lexeme = string_slice(
          highlighter->reader.input, start, highlighter->reader.current
      ),
    };
  }

  Aet_Assembly_Token asm_token = asm_token_result.value;
  Syntax_Token result = {
    .kind = aet_asm_kind_lookup[asm_token.kind],
    .start = asm_token.start,
    .end = asm_token.end,
  };

  if (result.kind != Syntax_Token_Kind_EOF) {
    result.lexeme =
        string_slice(highlighter->reader.input, asm_token.start, asm_token.end);
  }

  return result;
}

Syntax_Highlighter aet_asm_syntax_highlighter(String source) {
  return (Syntax_Highlighter){
    .reader = {.input = source},
    .lex_next_token = aet_asm_syntax_highlighter_lex_next_token,
  };
}
