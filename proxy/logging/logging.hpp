#ifndef PROXY_LOGGING_LOGGING_HPP
#define PROXY_LOGGING_LOGGING_HPP

#include <glog/logging.h>

namespace proxy {
namespace logging {

extern const char *FORMAT_FG_BLACK;
extern const char *FORMAT_FG_RED;
extern const char *FORMAT_FG_GREEN;
extern const char *FORMAT_FG_YELLOW;
extern const char *FORMAT_FG_BLUE;
extern const char *FORMAT_FG_MAGENTA;
extern const char *FORMAT_FG_CYAN;
extern const char *FORMAT_FG_WHITE;

extern const char *FORMAT_BG_BLACK;
extern const char *FORMAT_BG_RED;
extern const char *FORMAT_BG_GREEN;
extern const char *FORMAT_BG_YELLOW;
extern const char *FORMAT_BG_BLUE;
extern const char *FORMAT_BG_MAGENTA;
extern const char *FORMAT_BG_CYAN;
extern const char *FORMAT_BG_WHITE;

extern const char *FORMAT_RESET;
extern const char *FORMAT_BOLD;
extern const char *FORMAT_UNDERLINE;
extern const char *FORMAT_INVERSE;

#if DCHECK_IS_ON()

#define DVLOG_IF(verboselevel, condition) VLOG_IF(verboselevel, condition)
#define DVLOG_EVERY_N(verboselevel, n) VLOG_EVERY_N(verboselevel, n)
#define DVLOG_IF_EVERY_N(verboselevel, condition, n)                           \
  VLOG_IF_EVERY_N(verboselevel, condition, n)

#else // !DCHECK_IS_ON()

#define DVLOG(verboselevel)                                                    \
  static_cast<void>(0), (true || !VLOG_IS_ON(verboselevel))                    \
                            ? (void)0                                          \
                            : google::LogMessageVoidify() & LOG(INFO)

#define DVLOG_IF(verboselevel, condition)                                      \
  static_cast<void>(0), (true || !VLOG_IS_ON(verboselevel) || !(condition))    \
                            ? (void)0                                          \
                            : google::LogMessageVoidify() & LOG(INFO)

#define DVLOG_EVERY_N(verboselevel, n)                                         \
  static_cast<void>(0), (true || !VLOG_IS_ON(verboselevel))                    \
                            ? (void)0                                          \
                            : google::LogMessageVoidify() & LOG(INFO)

#define DLOG_IF_EVERY_N(verboselevel, condition, n)                            \
  static_cast<void>(0), (true || !VLOG_IS_ON(verboselevel) || !(condition))    \
                            ? (void)0                                          \
                            : google::LogMessageVoidify() & LOG(INFO)

#endif // DCHECK_IS_ON()

void init(char *argv0);

} // namespace logging
} // namespace proxy

#endif // PROXY_LOGGING_LOGGING_HPP