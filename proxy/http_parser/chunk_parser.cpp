#include "chunk_parser.hpp"
#include "proxy/util/misc_strings.hpp"
#include <climits>

namespace proxy {
namespace http_parser {

const unsigned long long MAX_LENGTH_BEFORE_LAST_DIGIT = (ULLONG_MAX - 15) / 16;

void chunk_parser::reset() {
  state_ = state::length_hex;
  extension_.clear();
  header_name_.clear();
  header_value_.clear();
  header_value_space_ = false;
}

void chunk_parser::save_header_if_non_empty(http::trailer &trailer) {
  if (!header_name_.empty()) {
    trailer.headers.push_back({header_name_, header_value_});
    header_name_.clear();
    header_value_.clear();
    header_value_space_ = false;
  }
}

[[nodiscard]] boost::tribool
chunk_parser::consume(http::chunk &chunk, http::trailer &trailer, char input) {
  switch (state_) {
  case state::length_hex:
    if (input == '\r') {
      state_ = state::expecting_newline_1;
      return boost::indeterminate;
    } else if (input == ';') {
      state_ = state::optional_extension;
      extension_.push_back(input);
      return boost::indeterminate;
    } else if (chunk.chunk_length <= MAX_LENGTH_BEFORE_LAST_DIGIT) {
      int hex_digit = util::misc_strings::hex_digit_to_int_safe(input);
      if (hex_digit > -1) {
        chunk.chunk_length = chunk.chunk_length * 16 + hex_digit;
        return boost::indeterminate;
      } else {
        return false;
      }
    }
  case state::optional_extension:
    if (input == '\r') {
      state_ = state::expecting_newline_1;
      return boost::indeterminate;
    } else if (util::misc_strings::is_ctl(input)) {
      return false;
    } else {
      extension_.push_back(input);
      return boost::indeterminate;
    }
  case state::expecting_newline_1:
    if (input == '\n') {
      remaining_data_count_ = chunk.chunk_length;
      if (chunk.chunk_length == 0) {
        trailer.extension = extension_;
        state_ = state::header_line_start;
      } else {
        state_ = state::data;
      }
      return boost::indeterminate;
    } else {
      return false;
    }
  case state::data:
    if (remaining_data_count_ == 0) {
      if (input == '\r') {
        state_ = state::expecting_newline_3;
        return boost::indeterminate;
      } else {
        return false;
      }
    } else {
      remaining_data_count_--;
      return boost::indeterminate;
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
    save_header_if_non_empty(trailer);
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
