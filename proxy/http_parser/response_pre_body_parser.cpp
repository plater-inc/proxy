#include "response_pre_body_parser.hpp"
#include "proxy/util/misc_strings.hpp"

namespace proxy {
namespace http_parser {

void response_pre_body_parser::reset() {
  state_ = state::http_version_h;
  header_name_.clear();
  header_value_.clear();
  header_value_space_ = false;
}

void response_pre_body_parser::save_header_if_non_empty(
    http::response_pre_body &res) {
  if (!header_name_.empty()) {
    res.headers.push_back({header_name_, header_value_});
    header_name_.clear();
    header_value_.clear();
    header_value_space_ = false;
  }
}

[[nodiscard]] boost::tribool
response_pre_body_parser::consume(http::response_pre_body &res, char input) {
  switch (state_) {
  case state::http_version_h:
    if (input == 'H') {
      res.http_version_string.push_back(input);
      state_ = state::http_version_t_1;
      return boost::indeterminate;
    } else {
      return false;
    }
  case state::http_version_t_1:
    if (input == 'T') {
      res.http_version_string.push_back(input);
      state_ = state::http_version_t_2;
      return boost::indeterminate;
    } else {
      return false;
    }
  case state::http_version_t_2:
    if (input == 'T') {
      res.http_version_string.push_back(input);
      state_ = state::http_version_p;
      return boost::indeterminate;
    } else {
      return false;
    }
  case state::http_version_p:
    if (input == 'P') {
      res.http_version_string.push_back(input);
      state_ = state::http_version_slash;
      return boost::indeterminate;
    } else {
      return false;
    }
  case state::http_version_slash:
    if (input == '/') {
      res.http_version_string.push_back(input);
      state_ = state::http_version_major_start;
      return boost::indeterminate;
    } else {
      return false;
    }
  case state::http_version_major_start:
    if (util::misc_strings::is_digit(input)) {
      res.http_version_string.push_back(input);
      state_ = state::http_version_major;
      return boost::indeterminate;
    } else {
      return false;
    }
  case state::http_version_major:
    if (input == '.') {
      res.http_version_string.push_back(input);
      state_ = state::http_version_minor_start;
      return boost::indeterminate;
    } else if (util::misc_strings::is_digit(input)) {
      res.http_version_string.push_back(input);
      return boost::indeterminate;
    } else {
      return false;
    }
  case state::http_version_minor_start:
    if (util::misc_strings::is_digit(input)) {
      res.http_version_string.push_back(input);
      state_ = state::http_version_minor;
      return boost::indeterminate;
    } else {
      return false;
    }
  case state::http_version_minor:
    if (input == ' ') {
      state_ = state::code_1;
      return boost::indeterminate;
    } else if (util::misc_strings::is_digit(input)) {
      res.http_version_string.push_back(input);
      return boost::indeterminate;
    } else {
      return false;
    }
  case state::code_1:
    if (util::misc_strings::is_digit(input)) {
      res.code.push_back(input);
      state_ = state::code_2;
      return boost::indeterminate;
    } else {
      return false;
    }
  case state::code_2:
    if (util::misc_strings::is_digit(input)) {
      res.code.push_back(input);
      state_ = state::code_3;
      return boost::indeterminate;
    } else {
      return false;
    }
  case state::code_3:
    if (util::misc_strings::is_digit(input)) {
      res.code.push_back(input);
      state_ = state::space_after_code;
      return boost::indeterminate;
    } else {
      return false;
    }
  case state::space_after_code:
    if (input == ' ') {
      state_ = state::reason;
      return boost::indeterminate;
    } else {
      return false;
    }
  case state::reason:
    if (input == '\r') {
      state_ = state::expecting_newline_1;
      return boost::indeterminate;
    } else if (util::misc_strings::is_ctl(input)) {
      return false;
    } else {
      res.reason.push_back(input);
      return boost::indeterminate;
    }
  case state::expecting_newline_1:
    if (input == '\n') {
      state_ = state::header_line_start;
      return boost::indeterminate;
    } else {
      return false;
    }
  case state::header_line_start:
    if (input == ' ' || input == '\t') {
      if (header_name_.empty()) {
        return false;
      }
      state_ = state::header_value;
      header_value_space_ = !header_value_.empty();
      return boost::indeterminate;
    }
    save_header_if_non_empty(res);
    if (input == '\r') {
      state_ = state::expecting_newline_3;
      return boost::indeterminate;
    } else if (!util::misc_strings::is_char(input) ||
               util::misc_strings::is_ctl(input) ||
               util::misc_strings::is_tspecial(input)) {
      return false;
    } else {
      state_ = state::header_name;
      header_name_.push_back(input);
      return boost::indeterminate;
    }
  case state::header_name:
    if (input == ':') {
      state_ = state::header_value;
      return boost::indeterminate;
    } else if (!util::misc_strings::is_char(input) ||
               util::misc_strings::is_ctl(input) ||
               util::misc_strings::is_tspecial(input)) {
      return false;
    } else {
      header_name_.push_back(input);
      return boost::indeterminate;
    }
  case state::header_value:
    if (input == '\r') {
      state_ = state::expecting_newline_2;
      return boost::indeterminate;
    } else if (input == ' ' || input == '\t') {
      header_value_space_ = !header_value_.empty();
      return boost::indeterminate;
    } else if (util::misc_strings::is_ctl(input)) {
      return false;
    } else {
      if (header_value_space_) {
        header_value_.push_back(' ');
        header_value_space_ = false;
      }
      header_value_.push_back(input);
      return boost::indeterminate;
    }
  case state::expecting_newline_2:
    if (input == '\n') {
      state_ = state::header_line_start;
      return boost::indeterminate;
    } else {
      return false;
    }
  case state::expecting_newline_3:
    return (input == '\n');
  default:
    return false;
  }
}

} // namespace http_parser
} // namespace proxy
