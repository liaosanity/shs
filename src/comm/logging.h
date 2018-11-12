#ifndef LOGGING_H
#define LOGGING_H

#include <string>
#include "slog.h"

using namespace slog;

#define LOG_NAME(name) static const char* _shs_logger = name
#define LOG(level) sLog(_shs_logger, level)
#define NLOG(name, level) sLog(name, level)
#define LOG_INIT(conf) sLogConfig(conf)

const int TRACE = slog::SLOG_TRACE;
const int DEBUG = slog::SLOG_DEBUG;
const int INFO  = slog::SLOG_INFO;
const int WARN  = slog::SLOG_WARN;
const int ERROR = slog::SLOG_ERROR;
const int FATAL = slog::SLOG_FATAL;

#endif
