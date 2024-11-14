/**
 * Copyright (c) 2024, Scitix Tech PTE. LTD. All rights reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <string.h>

#include "logger.h"

namespace ucommd {

const char* log_level_names[] = {
  "FATAL",
  "ERROR",
  "WARN",
  "INFO",
  "DEBUG",
  "TRACE",
  NULL
};

Logger& Logger::instance() {
  static Logger g_log;
  return g_log;
}

Logger::Logger() {
  const char* node_name_env = getenv("NODE_NAME");
  if (node_name_env && node_name_env[0]) {
    strncpy(hostname_, node_name_env, 64);
  } else {
    if (-1 == gethostname(hostname_, sizeof(hostname_))) {
      strncpy(hostname_, "unknown", 16);
    }
  }
}

void Logger::setLevel(LogLevel level) {
  log_level_ = level;
}

void Logger::_log(Logger::LogLevel level, const char* fmt, ...) {
  if (level > log_level_) return;

  struct timeval now;
  struct tm ltm;
  char time_stamp[80] = {0}, time_stamp_ms[84] = {0};

  gettimeofday(&now, NULL);
  localtime_r(&now.tv_sec, &ltm);
  strftime(time_stamp, sizeof(time_stamp), "%y-%m-%d %H:%M:%S", &ltm);
  snprintf(time_stamp_ms, sizeof(time_stamp_ms), "%s.%03d",
      time_stamp, static_cast<int>(now.tv_usec/1000));

  va_list va;
  va_start(va, fmt);

  {
    fprintf(stdout, "[SiCL:%s %s %s:%ld] ", log_level_names[level], time_stamp_ms,
        hostname_, static_cast<long>(getpid()));
    vfprintf(stdout, fmt, va);
    fprintf(stdout, "\n");
    fflush(stdout);
  }

  va_end(va);
}

} // namespace ucommd
