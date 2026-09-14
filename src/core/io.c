#include "core/io.h"

#include "core/allocator.h"
#include "core/runtime.h"

#include <assert.h>
#include <stdio.h>

File_Read_Result
read_entire_file(String path, bool8 null_term, Allocator allocator) {
  FILE *f = fopen(path.data, "rbe");
  if (f == nullptr) {
    return err(File_Read_Result, File_Error_Failed_To_Open_File);
  }
  defer {
    fclose(f);
  };

  fseek(f, 0, SEEK_END);
  usize file_size = (usize)ftell(f);
  rewind(f);

  // NOTE(nico): I ended up supporting null term.. most of the things I need
  // this for are libraries that expect null termination
  // NOTE(nico): this is a bit opaque to return this from an allocation
  // failure, but oh well..
  byte *file_buf = (byte *)or_return(
      alloc(allocator, file_size + (null_term ? 1 : 0)),
      err(File_Read_Result, File_Error_Failed_To_Read_File)
  );

  usize read = fread(file_buf, 1, file_size, f);
  if (read < file_size && ferror(f)) {
    // NOTE(nico): Again, pretty opaque, but lazy to handle OS specific errno
    // shit
    free_(allocator, file_buf);
    return err(File_Read_Result, File_Error_Failed_To_Read_File);
  }

  if (null_term) {
    file_buf[file_size] = '\0';
  }
  return ok(File_Read_Result, file_buf);
}