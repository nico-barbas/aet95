#include "core/log.h"

#include "stdio.h"

static const char *level_str[Log_Level_MAX] = {
  [Log_Level_Debug] = "[DEBUG]",
  [Log_Level_Info] = "[INFO]",
  [Log_Level_Warn] = "[WARN]",
  [Log_Level_Error] = "[ERROR]",
};

static void console_log(Log_Level level, const char *msg) {
  printf("%s: %s\n", level_str[level], msg);
}

Logger console_logger(Log_Level minimum_level) {
  return (Logger){
    .minimum_level = minimum_level,
    .log_proc = console_log,
    .data = nullptr,
  };
}

void log_debug(Logger *logger, const char *msg) {
  if (logger->minimum_level <= Log_Level_Debug) {
    logger->log_proc(Log_Level_Debug, msg);
  }
}

void log_info(Logger *logger, const char *msg) {
  if (logger->minimum_level <= Log_Level_Info) {
    logger->log_proc(Log_Level_Info, msg);
  }
}

void log_warn(Logger *logger, const char *msg) {
  if (logger->minimum_level <= Log_Level_Warn) {
    logger->log_proc(Log_Level_Warn, msg);
  }
}

void log_error(Logger *logger, const char *msg) {
  if (logger->minimum_level <= Log_Level_Error) {
    logger->log_proc(Log_Level_Error, msg);
  }
}