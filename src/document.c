#include "document.h"

#include "core/allocator.h"
#include "core/strings.h"

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

  Allocation_Result alloc =
      allocator.alloc(allocator, sizeof(byte) * initial_cap);
  if (alloc.err != Allocation_Error_None) {
    return err(Document_Create_Result, Document_Error_Failed_To_Allocate);
  }

  document.buffer = alloc.allocation;
  return ok(Document_Create_Result, document);
}

void destroy_document(Document document) {
  document.allocator.free(document.allocator, document.buffer);
}

void document_clear_content(Document *document) {
  document->buffer_len = document->gap_size;
  document->gap_start = 0;
  document->gap_end = document->gap_size;
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

  return document_expand_gap(document);
}

// Document_Error document_write_char(Document *document, byte c) {
//   if ()
// }