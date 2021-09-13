#include "logging.hpp"

namespace proxy {
namespace logging {

const char *FORMAT_FG_BLACK = "\033[30m";
const char *FORMAT_FG_RED = "\033[31m";
const char *FORMAT_FG_GREEN = "\033[32m";
const char *FORMAT_FG_YELLOW = "\033[33m";
const char *FORMAT_FG_BLUE = "\033[34m";
const char *FORMAT_FG_MAGENTA = "\033[35m";
const char *FORMAT_FG_CYAN = "\033[36m";
const char *FORMAT_FG_WHITE = "\033[37m";

const char *FORMAT_BG_BLACK = "\033[40m";
const char *FORMAT_BG_RED = "\033[41m";
const char *FORMAT_BG_GREEN = "\033[42m";
const char *FORMAT_BG_YELLOW = "\033[43m";
const char *FORMAT_BG_BLUE = "\033[44m";
const char *FORMAT_BG_MAGENTA = "\033[45m";
const char *FORMAT_BG_CYAN = "\033[46m";
const char *FORMAT_BG_WHITE = "\033[47m";

const char *FORMAT_RESET = "\033[0m";
const char *FORMAT_BOLD = "\033[1m";
const char *FORMAT_UNDERLINE = "\033[4m";
const char *FORMAT_INVERSE = "\033[7m";

void init(char *argv0) { google::InitGoogleLogging(argv0); }

} // namespace logging
} // namespace proxy
