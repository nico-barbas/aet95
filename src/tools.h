#ifndef TOOLS_H
#define TOOLS_H

#include "core/strings.h"

typedef enum Syntax_Token_Kind {
  Syntax_Token_Kind_Error,
  Syntax_Token_Kind_EOF,
  Syntax_Token_Kind_Unsupported,
  Syntax_Token_Kind_Comment,
  Syntax_Token_Kind_Literal,
  Syntax_Token_Kind_Punctuation,
  Syntax_Token_Kind_Delimiter,
  Syntax_Token_Kind_Identifier,
  Syntax_Token_Kind_Keyword,
  Syntax_Token_Kind_Type,
  Syntax_Token_Kind_Operator,
  Syntax_Token_Kind_Control_Flow,
} Syntax_Token_Kind;

typedef struct Syntax_Token {
  Syntax_Token_Kind kind;
  usize start;
  usize end;
  String lexeme;
} Syntax_Token;

typedef struct Syntax_Highlighter Syntax_Highlighter;
struct Syntax_Highlighter {
  String_Reader reader;
  Syntax_Token (*lex_next_token)(Syntax_Highlighter *highlighter);
};

Syntax_Highlighter aet_asm_syntax_highlighter(String source);

#endif