#include "request_pre_body_parser.hpp"
#include "proxy/util/misc_strings.hpp"
#include <iostream>
#include <regex>

namespace proxy {
namespace http_parser {

void request_pre_body_parser::reset() {
  state_ = state::method_start;
  header_name_.clear();
  header_value_.clear();
  header_value_space_ = false;
}

void request_pre_body_parser::save_header_if_non_empty(
    http::request_pre_body &req) {
  if (!header_name_.empty()) {
    req.headers.push_back({header_name_, header_value_});
    header_name_.clear();
    header_value_.clear();
    header_value_space_ = false;
  }
}

[[nodiscard]] boost::tribool
request_pre_body_parser::consume(http::request_pre_body &req, char input) {
  switch (state_) {
  case state::method_start:
    if (!util::misc_strings::is_char(input) ||
        util::misc_strings::is_ctl(input) ||
        util::misc_strings::is_tspecial(input)) {
      return false;
    } else {
      state_ = state::method;
      req.method.push_back(input);
      return boost::indeterminate;
    }
  case state::method:
    if (input == ' ') {
      state_ = state::uri;
      return boost::indeterminate;
    } else if (!util::misc_strings::is_char(input) ||
               util::misc_strings::is_ctl(input) ||
               util::misc_strings::is_tspecial(input)) {
      return false;
    } else {
      req.method.push_back(input);
      return boost::indeterminate;
    }
  case state::uri:
    if (input == ' ') {
      state_ = state::http_version_h;
      return boost::indeterminate;
    } else if (util::misc_strings::is_ctl(input)) {
      return false;
    } else {
      req.uri.push_back(input);
      return boost::indeterminate;
    }
  case state::http_version_h:
    if (input == 'H') {
      req.http_version_string.push_back(input);
      state_ = state::http_version_t_1;
      return boost::indeterminate;
    } else {
      return false;
    }
  case state::http_version_t_1:
    if (input == 'T') {
      req.http_version_string.push_back(input);
      state_ = state::http_version_t_2;
      return boost::indeterminate;
    } else {
      return false;
    }
  case state::http_version_t_2:
    if (input == 'T') {
      req.http_version_string.push_back(input);
      state_ = state::http_version_p;
      return boost::indeterminate;
    } else {
      return false;
    }
  case state::http_version_p:
    if (input == 'P') {
      req.http_version_string.push_back(input);
      state_ = state::http_version_slash;
      return boost::indeterminate;
    } else {
      return false;
    }
  case state::http_version_slash:
    if (input == '/') {
      req.http_version_string.push_back(input);
      state_ = state::http_version_major_start;
      return boost::indeterminate;
    } else {
      return false;
    }
  case state::http_version_major_start:
    if (util::misc_strings::is_digit(input)) {
      req.http_version_string.push_back(input);
      state_ = state::http_version_major;
      return boost::indeterminate;
    } else {
      return false;
    }
  case state::http_version_major:
    if (input == '.') {
      req.http_version_string.push_back(input);
      state_ = state::http_version_minor_start;
      return boost::indeterminate;
    } else if (util::misc_strings::is_digit(input)) {
      req.http_version_string.push_back(input);
      return boost::indeterminate;
    } else {
      return false;
    }
  case state::http_version_minor_start:
    if (util::misc_strings::is_digit(input)) {
      req.http_version_string.push_back(input);
      state_ = state::http_version_minor;
      return boost::indeterminate;
    } else {
      return false;
    }
  case state::http_version_minor:
    if (input == '\r') {
      state_ = state::expecting_newline_1;
      return boost::indeterminate;
    } else if (util::misc_strings::is_digit(input)) {
      req.http_version_string.push_back(input);
      return boost::indeterminate;
    } else {
      return false;
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
    save_header_if_non_empty(req);
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
