#ifndef CORE_LOG_H
#define CORE_LOG_H

#include "core/types.h"

/*
  NOTE(nico): This is fine for now but support for variable arguments would be
  better
*/

typedef enum Log_Level {
  Log_Level_Debug,
  Log_Level_Info,
  Log_Level_Warn,
  Log_Level_Error,
  Log_Level_MAX,
} Log_Level;

typedef void (*Log_Proc)(Log_Level level, const char *msg);

typedef struct Logger {
  Log_Level minimum_level;
  Log_Proc log_proc;
  rawptr data;
} Logger;

Logger console_logger(Log_Level minimum_level);

void log_debug(Logger *logger, const char *msg);
void log_info(Logger *logger, const char *msg);
void log_warn(Logger *logger, const char *msg);
void log_error(Logger *logger, const char *msg);

#endif