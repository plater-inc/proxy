#include "misc_strings.hpp"
#include <string>

namespace proxy {
namespace util {

namespace misc_strings {

const long long MAX_VALUE_BEFORE_LAST_DIGIT = (LLONG_MAX - 9) / 10;
const char name_value_separator[NAME_VALUE_SEPARATOR_COUNT] = {':', ' '};
const char method_line_separator[METHOD_LINE_SEPARATOR_COUNT] = {' '};
const char status_line_separator[STATUS_LINE_SEPARATOR_COUNT] = {' '};
const char crlf[CLRF_COUNT] = {'\r', '\n'};

bool is_char(int c) { return c >= 0 && c <= 127; }

bool is_ctl(int c) { return (c >= 0 && c <= 31) || (c == 127); }

bool is_tspecial(int c) {
  switch (c) {
  case '(':
  case ')':
  case '<':
  case '>':
  case '@':
  case ',':
  case ';':
  case ':':
  case '\\':
  case '"':
  case '/':
  case '[':
  case ']':
  case '?':
  case '=':
  case '{':
  case '}':
  case ' ':
  case '\t':
    return true;
  default:
    return false;
  }
}

bool is_digit(int c) { return c >= '0' && c <= '9'; }

bool is_hexdigit(int c) {
  return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') ||
         (c >= 'a' && c <= 'f');
}

int hex_digit_to_int_safe(int c) {
  if (c >= '0' && c <= '9') {
    return c - '0';
  } else if (c >= 'A' && c <= 'F') {
    return c - 'A' + 10;
  } else if (c >= 'a' && c <= 'f') {
    return c - 'a' + 10;
  } else {
    return -1;
  }
}

int int_to_hex_digit(int i) {
  if (i < 10) {
    return '0' + i;
  } else {
    return 'a' + (i - 10);
  }
}

int digit_to_int_safe(int c) {
  if (c >= '0' && c <= '9') {
    return c - '0';
  } else {
    return -1;
  }
}

[[nodiscard]] boost::tuple<bool, long long>
string_to_positive_long_long(std::string &str) {

  long long number = 0LL;
  bool success = true;
  bool is_negative = false;
  for (int i = 0; i < str.length(); i++) {
    if (i == 0 && str[0] == '-') {
      is_negative = true;
    } else {
      if (number <= MAX_VALUE_BEFORE_LAST_DIGIT) {
        int digit = util::misc_strings::digit_to_int_safe(str[i]);
        if (digit > -1) {
          number = number * 10 + digit;
        } else {
          success = false;
          break;
        }
      } else {
        success = false;
        break;
      }
    }
  }
  return boost::make_tuple(success, success && !is_negative ? number : 0LL);
}

std::string urldecode(boost::string_view source) {
  std::string result;
  result.reserve(source.size());
  int source_index = 0;
  while (source_index < source.size()) {
    char c = source[source_index++];
    if (c == '%' && source_index + 2 <= source.size()) {
      char c2 = source[source_index++];
      char c3 = source[source_index++];
      if (is_hexdigit(c2) && is_hexdigit(c3)) {
        c2 = hex_digit_to_int_safe(c2);
        c3 = hex_digit_to_int_safe(c3);
        result.push_back(16 * c2 + c3);
      } else {
        result.push_back(c);
        result.push_back(c2);
        result.push_back(c3);
      }
    } else {
      result.push_back(c == '+' ? ' ' : c);
    }
  }
  return result;
}

std::string urlencode(boost::string_view source) {
  std::string result;
  result.reserve(source.size());
  for (boost::string_view::iterator it = source.begin(); it != source.end();
       it++) {
    if ((*it >= 'A' && *it <= 'Z') || (*it >= 'a' && *it <= 'z') ||
        (*it >= '0' && *it <= '9') || *it == '-' || *it == '_' || *it == '.' ||
        *it == '!' || *it == '~' || *it == '*' || *it == '\'' || *it == '(' ||
        *it == ')') {
      result.push_back(*it);
    } else {
      result.push_back('%');
      result.push_back(int_to_hex_digit((*it) / 16));
      result.push_back(int_to_hex_digit((*it) % 16));
    }
  }
  return result;
}

std::string to_lowercase(std::string value) {
  std::string result(value.length(), 0);
  std::transform(value.begin(), value.end(), result.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  return result;
}

} // namespace misc_strings

} // namespace util
} // namespace proxy