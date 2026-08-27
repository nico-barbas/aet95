#ifndef CORE_IO_H
#define CORE_IO_H

#include "core/allocator.h"
#include "core/strings.h"
#include "core/types.h"

typedef enum File_Error {
  File_Error_None,
  File_Error_Failed_To_Open_File,
  File_Error_Failed_To_Read_File,
} File_Error;

typedef Result(byte *, File_Error) File_Read_Result;

// NOTE(nico): Very simple implementation used for reading entire files in one
// go. If you needs streams, this is not for you
File_Read_Result
read_entire_file(String path, bool8 null_term, Allocator allocator);

#endif