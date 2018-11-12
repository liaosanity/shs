#pragma once

#include <iosfwd>
#include <string>
#include <ostream>
#include <sstream>

#include "comm/logging.h"
#include "log/log_message.h"

#define SLOG_TRACE \
    shs::LogMessage(TRACE).stream()
#define SLOG_DEBUG \
    shs::LogMessage(DEBUG).stream()
#define SLOG_INFO \
    shs::LogMessage(INFO).stream()
#define SLOG_WARN \
    shs::LogMessage(WARN).stream()
#define SLOG_ERROR \
    shs::LogMessage(ERROR).stream()
#define SLOG_FATAL \
    shs::LogMessage(FATAL).stream()

#define SLOG(level) SLOG_ ## level

