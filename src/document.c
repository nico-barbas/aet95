#include "document.h"

#include "core/allocator.h"
#include "core/math.h"
#include "core/strings.h"
#include "core/types.h"

#include <assert.h>
#include <string.h>

#define DEFAULT_DOCUMENT_GAP_SIZE 64

Document_Create_Result
make_document(Document_Create_Info *info, Allocator allocator) {
  usize gap_size =
      info->gap_size > 0 ? info->gap_size : DEFAULT_DOCUMENT_GAP_SIZE;
  usize initial_cap =
      info->initial_cap > gap_size ? info->initial_cap : gap_size;

  Document document = {
    .allocator = allocator,
    .buffer_cap = initial_cap,
    .buffer_len = gap_size,
    .gap_start = 0,
    .gap_end = gap_size,
    .gap_size = gap_size,
  };

  document.lines = or_return(
      make_document_line_list(32, allocator),
      err(Document_Create_Result, Document_Error_Failed_To_Allocate)
  );

  Document_Line_List_Push_Result line_result =
      document_line_list_push(&document.lines, &(Document_Line){0});
  if (!line_result.ok) {
    delete_document_line_list(&document.lines);
    return err(Document_Create_Result, Document_Error_Failed_To_Allocate);
  }

  Allocation_Result alloc =
      allocator.alloc(allocator, sizeof(byte) * initial_cap);
  if (alloc.err != Allocation_Error_None) {
    delete_document_line_list(&document.lines);
    return err(Document_Create_Result, Document_Error_Failed_To_Allocate);
  }

  document.buffer = alloc.allocation;
  return ok(Document_Create_Result, document);
}

void destroy_document(Document document) {
  delete_document_line_list(&document.lines);
  document.allocator.free(document.allocator, document.buffer);
}

void document_clear_content(Document *document) {
  document->buffer_len = document->gap_size;
  document->gap_start = 0;
  document->gap_end = document->gap_size;
  document->lines.len = 1;
  document->lines.items[0] = (Document_Line){0};
  document->current_line = 0;
}

static Document_Error document_grow_buffer(Document *document) {
  char *old_buffer = document->buffer;

  usize new_cap = document->buffer_cap > 0 ? document->buffer_cap * 2 : 512;

  Allocation_Result alloc =
      document->allocator.alloc(document->allocator, sizeof(byte) * new_cap);
  if (alloc.err != Allocation_Error_None) {
    return Document_Error_Failed_To_Allocate;
  }

  document->buffer = alloc.allocation;
  memmove(document->buffer, old_buffer, sizeof(byte) * document->buffer_cap);
  document->buffer_cap = new_cap;
  document->allocator.free(document->allocator, old_buffer);

  return Document_Error_None;
}

static usize document_current_gap_size(Document *document) {
  return document->gap_end - document->gap_start;
}

static usize document_suffix_len(Document *document) {
  return document->buffer_len - document->gap_end;
}

usize document_text_len(Document *document) {
  return document->buffer_len - document_current_gap_size(document);
}

static Document_Error document_expand_gap(Document *document) {
  usize suffix_len = document_suffix_len(document);
  while (document->gap_end + document->gap_size + suffix_len >
         document->buffer_cap) {
    Document_Error error = document_grow_buffer(document);
    if (error != Document_Error_None) {
      return error;
    }
  }

  memmove(
      document->buffer + document->gap_end + document->gap_size,
      document->buffer + document->gap_end,
      sizeof(byte) * suffix_len
  );
  document->gap_end += document->gap_size;
  document->buffer_len += document->gap_size;

  return Document_Error_None;
}

static void document_collapse_gap(Document *document) {
  memmove(
      document->buffer + document->gap_start,
      document->buffer + document->gap_end,
      document_suffix_len(document)
  );
  document->buffer_len -= document->gap_end - document->gap_start;
  document->gap_end = document->gap_start;
}

// NOTE(nico): This process is not atomic sadly
Document_Error document_write_char(Document *document, char c) {
  while (document->gap_end >= document->buffer_cap) {
    Document_Error error = document_grow_buffer(document);
    if (error != Document_Error_None) {
      return error;
    }
  }

  if (document->gap_start >= document->gap_end) {
    Document_Error error = document_expand_gap(document);
    if (error != Document_Error_None) {
      return error;
    }
  }

  document->buffer[document->gap_start] = c;
  document->gap_start += 1;

  // NOTE(nico): Still need to increment the logical offset on newlines
  usize next_line_index = document->current_line + 1;
  for (usize i = next_line_index; i < document->lines.len; i += 1) {
    document->lines.items[i].logical_offset += 1;
  }

  if (c == '\n') {
    // FIXME(nico): There needs to be a rollback on the write I think
    Document_Line_List_Insert_Result line_result = document_line_list_insert_at(
        &document->lines,
        &(Document_Line){.logical_offset = document->gap_start},
        document->current_line + 1
    );
    if (!line_result.ok) {
      return Document_Error_Failed_To_Write;
    }
    document->current_line += 1;
  }

  return Document_Error_None;
}

// FIXME(nico): [27-08-26] This should be the fast path. Needs to be rewrote for
// fast insert but I'm lazy. It should also return the number of characters
// successfully written
Document_Error document_write_string(Document *document, String str) {
  for (usize i = 0; i < str.len; i += 1) {
    Document_Error error = document_write_char(document, str.data[i]);
    if (error != Document_Error_None) {
      return error;
    }
  }

  return Document_Error_None;
}

Document_Error document_delete_chars(Document *document, usize n) {
  usize _n = n;
  if (_n > document->gap_start) {
    _n = document->gap_start;
  }

  usize a = document->gap_start - _n;
  usize deleted_line_count = 0;
  for (usize i = document->current_line; i > 0; i -= 1) {
    usize s = document->lines.items[i].logical_offset;
    if (a >= s) {
      break;
    }

    if (s <= document->gap_start) {
      deleted_line_count += 1;
    }
  }

  List_Error error = document_line_list_ordered_remove_range(
      &document->lines,
      document->current_line - deleted_line_count + 1,
      deleted_line_count
  );
  if (error != List_Error_None) {
    return Document_Error_Failed_To_Write;
  }

  document->current_line -= deleted_line_count;
  for (usize i = document->current_line + 1; i < document->lines.len; i += 1) {
    document->lines.items[i].logical_offset -= _n;
  }

  document->gap_start -= _n;
  return Document_Error_None;
}

Document_Error document_move_gap(Document *document, usize pos) {
  if (pos > document_text_len(document)) {
    return Document_Error_Invalid_Position;
  }

  document_collapse_gap(document);
  document->gap_start = pos;
  document->gap_end = pos;

  // NOTE(nico): This should never error out since we already check the same
  // condition beforehand
  Document_Position_Result new_line_result =
      document_query_position_from_logical_offset(document, pos);
  if (!new_line_result.ok) {
    return new_line_result.error;
  }
  document->current_line = new_line_result.value.line;

  return document_expand_gap(document);
}

Document_Position_Result
document_query_position_from_logical_offset(Document *document, usize offset) {
  if (offset > document_text_len(document)) {
    return err(Document_Position_Result, Document_Error_Invalid_Position);
  }

  usize lo = 0;
  usize hi = document->lines.len;
  while (lo < hi) {
    usize mid = lo + (hi - lo) / 2;

    if (document->lines.items[mid].logical_offset <= offset) {
      lo = mid + 1;
    } else {
      hi = mid;
    }
  }

  return ok(
      Document_Position_Result,
      ((Document_Position){
        .line = lo - 1,
        .col = offset - document->lines.items[lo - 1].logical_offset
      })
  );
}

static Document_Span document_query_line_span(Document *document, usize i) {
  return (Document_Span){
    .start = document->lines.items[i].logical_offset,
    .end = (i + 1 < document->lines.len)
               ? document->lines.items[i + 1].logical_offset - 1
               : document_text_len(document),
  };
}

Document_Logical_Offset_Result document_query_logical_offset_from_position(
    Document *document, Document_Position info
) {
  if (info.line >= document->lines.len) {
    return err(Document_Logical_Offset_Result, Document_Error_Invalid_Position);
  }

  Document_Span span = document_query_line_span(document, info.line);

  if (info.col > span.end - span.start) {
    return err(Document_Logical_Offset_Result, Document_Error_Invalid_Position);
  }

  return ok(Document_Logical_Offset_Result, span.start + info.col);
}

Document_Line_Content_Result
document_query_line_content(Document *document, usize i) {
  if (i >= document->lines.len) {
    return err(Document_Line_Content_Result, Document_Error_Invalid_Position);
  }

  Document_Span span = document_query_line_span(document, i);
  usize gap_width = document_current_gap_size(document);
  usize split = clamp_usize(document->gap_start, span.start, span.end);

  return ok(
      Document_Line_Content_Result,
      ((Document_Line_Content){
        .head =
            {
              .data = document->buffer + span.start,
              .len = split - span.start,
            },
        .tail = {
          .data = document->buffer + split + gap_width,
          .len = span.end - split,
        }
      })
  );
}