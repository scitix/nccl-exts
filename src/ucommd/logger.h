/**
 * Copyright (c) 2024, Scitix Tech PTE. LTD. All rights reserved.
 */

#pragma once

#define Log(level, ...) do {                            \
  ucommd::Logger::instance()._log(level, __VA_ARGS__);  \
} while (0)
#define LogFatal(...)   Log(ucommd::Logger::FATAL,  __VA_ARGS__)
#define LogError(...)   Log(ucommd::Logger::ERROR,  __VA_ARGS__)
#define LogWarn(...)    Log(ucommd::Logger::WARN,   __VA_ARGS__)
#define LogInfo(...)    Log(ucommd::Logger::INFO,   __VA_ARGS__)
#define LogDebug(...)   Log(ucommd::Logger::DEBUG,  __VA_ARGS__)
#ifndef NDEBUG
#define LogTrace(...)   Log(ucommd::Logger::TRACE,  __VA_ARGS__)
#else
#define LogTrace(...)   do { static_cast<void>(0); } while(0)
#endif

namespace ucommd {

class Logger {
 public:
  enum  LogLevel {
    FATAL = 0,
    ERROR,
    WARN,
    INFO,
    DEBUG,
    TRACE,
  };

  static Logger& instance();

  Logger();
  virtual ~Logger() {};

  void setLevel(LogLevel level);

  void _log(LogLevel level, const char* fmt, ...);

 private:
  LogLevel log_level_ = WARN;
  char hostname_[256] = {0};
};

} // namespace ucommd
