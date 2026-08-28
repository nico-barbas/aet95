#ifndef DOCUMENT_H
#define DOCUMENT_H

#include "core/allocator.h"
#include "core/strings.h"
#include "core/types.h"

typedef enum Document_Error {
  Document_Error_None,
  Document_Error_Failed_To_Allocate,
  Document_Error_Invalid_Position,
  Document_Error_Failed_To_Write,
} Document_Error;

typedef struct Document_Line {
  usize logical_offset;
} Document_Line;

typedef struct Document_Line_Content {
  String head;
  String tail;
} Document_Line_Content;

typedef struct Document_Span {
  usize start;
  usize end;
} Document_Span;

typedef struct Document_Position {
  usize line;
  usize col;
} Document_Position;

#define LIST_TYPE Document_Line
#define LIST_TYPE_NAME Document_Line_List
#define LIST_FUNCTION_PREFIX document_line_list
#include "core/list.h"

// NOTE(nico): This is a semi-generic document model for the multiple needs of
// this game. It uses a gap buffer to allow for fast modification
typedef struct Document {
  Allocator allocator;
  char *buffer;
  usize buffer_cap;
  usize buffer_len;
  usize gap_start;
  usize gap_end;
  usize gap_size;

  // Other views into the doc
  // NOTE(nico): [28-08-26] No gap buffer for the lines. Every newline
  // insertion/deletion is O(n) tail shift. I might revisit this later when it
  // shows real and measurable performance degradation. Tangentially,
  // performance is pretty important since we need to have real-time text
  // editing + real-time 3d rendering. This will be interesting
  Document_Line_List lines;
  usize current_line;
} Document;

typedef struct Document_Create_Info {
  usize initial_cap;
  usize gap_size;
} Document_Create_Info;

typedef Result(Document, Document_Error) Document_Create_Result;
typedef Result(Document_Position, Document_Error) Document_Position_Result;
typedef Result(usize, Document_Error) Document_Logical_Offset_Result;
typedef Result(
    Document_Line_Content, Document_Error
) Document_Line_Content_Result;

Document_Create_Result
make_document(Document_Create_Info *info, Allocator allocator);
void destroy_document(Document document);

void document_clear_content(Document *document);
usize document_text_len(Document *document);
Document_Error document_write_char(Document *document, char c);
Document_Error document_write_string(Document *document, String str);
Document_Error document_delete_chars(Document *document, usize n);
Document_Error document_move_gap(Document *document, usize pos);

// All query operations
Document_Position_Result
document_query_position_from_logical_offset(Document *document, usize offset);
Document_Logical_Offset_Result document_query_logical_offset_from_position(
    Document *document, Document_Position info
);
Document_Line_Content_Result
document_query_line_content(Document *document, usize i);

#endif