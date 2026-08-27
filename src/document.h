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

// NOTE(nico): This is a semi-generic document model for the multiple needs of
// this game. It uses a gap buffer to allow for modification
typedef struct Document {
  Allocator allocator;
  char *buffer;
  usize buffer_cap;
  usize buffer_len;
  usize gap_start;
  usize gap_end;
  usize gap_size;
} Document;

typedef struct Document_Create_Info {
  usize initial_cap;
  usize gap_size;
} Document_Create_Info;

typedef Result(Document, Document_Error) Document_Create_Result;

Document_Create_Result
make_document(Document_Create_Info *info, Allocator allocator);
void destroy_document(Document document);

void document_clear_content(Document *document);
usize document_text_len(Document *document);
Document_Error document_write_char(Document *document, char c);
Document_Error document_write_string(Document *document, String str);
Document_Error document_delete_chars(Document *document, usize n);
Document_Error document_move_gap(Document *document, usize pos);

#endif