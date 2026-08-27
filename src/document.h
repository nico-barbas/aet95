#ifndef DOCUMENT_H
#define DOCUMENT_H

#include "core/allocator.h"
#include "core/strings.h"
#include "core/types.h"

typedef enum Document_Error {
  Document_Error_None,
  Document_Error_Failed_To_Allocate,
  Document_Error_Invalid_Position,
} Document_Error;

typedef struct Document_Line {
  usize logical_start;
  usize logical_end;
} Document_Line;

typedef struct Document_Line_Info {
  usize line;
  usize col;
} Document_Line_Info;

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
  Document_Line_List lines;
  usize current_line;
} Document;

typedef struct Document_Create_Info {
  usize initial_cap;
  usize gap_size;
} Document_Create_Info;

typedef Result(Document, Document_Error) Document_Create_Result;
typedef Result(Document_Line_Info, Document_Error) Document_Line_Info_Result;

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
Document_Line_Info_Result
document_query_line_from_logical_offset(Document *document, usize offset);

#endif