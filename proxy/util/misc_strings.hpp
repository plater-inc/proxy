#ifndef PROXY_UTIL_MISC_STRINGS_HPP
#define PROXY_UTIL_MISC_STRINGS_HPP

#include <boost/tuple/tuple.hpp>
#include <boost/utility/string_view.hpp>
#include <string>

namespace proxy {
namespace util {

namespace misc_strings {

#define NAME_VALUE_SEPARATOR_COUNT 2
#define METHOD_LINE_SEPARATOR_COUNT 1
#define STATUS_LINE_SEPARATOR_COUNT 1
#define CLRF_COUNT 2

extern const char name_value_separator[NAME_VALUE_SEPARATOR_COUNT];
extern const char method_line_separator[METHOD_LINE_SEPARATOR_COUNT];
extern const char status_line_separator[STATUS_LINE_SEPARATOR_COUNT];
extern const char crlf[CLRF_COUNT];

/// Check if a byte is an HTTP character.
bool is_char(int c);

/// Check if a byte is an HTTP control character.
bool is_ctl(int c);

/// Check if a byte is defined as an HTTP tspecial character.
bool is_tspecial(int c);

/// Check if a byte is a digit.
bool is_digit(int c);

/// Check if a byte is hex digit (0-9/A-F/a-f).
bool is_hexdigit(int c);

/// Check if a byte is hex digit (0-9/A-F/a-f) and return corresponding int or
/// -1 if it is not.
int hex_digit_to_int_safe(int c);

/// Check if a byte is digit (0-9) and return corresponding int or -1 if it is
/// not.
int digit_to_int_safe(int c);

// Convert int 0-15 to hex digit character '[0-9a-f]'.
int int_to_hex_digit(int i);

/// Try to convert string to positive long long, return tuple (success, number).
/// Set number to 0 if unsuccessfull.
[[nodiscard]] boost::tuple<bool, long long>
string_to_positive_long_long(std::string &str);

std::string urldecode(boost::string_view source);

std::string urlencode(boost::string_view source);

std::string to_lowercase(std::string value);

} // namespace misc_strings

} // namespace util
} // namespace proxy

#endif // PROXY_UTIL_MISC_STRINGS_HPP