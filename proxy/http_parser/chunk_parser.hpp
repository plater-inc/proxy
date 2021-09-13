#ifndef PROXY_HTTP_PARSER_CHUNK_PARSER_HPP
#define PROXY_HTTP_PARSER_CHUNK_PARSER_HPP

#include "proxy/http/chunk.hpp"
#include "proxy/http/trailer.hpp"
#include <boost/logic/tribool.hpp>
#include <boost/tuple/tuple.hpp>

namespace proxy {
namespace http_parser {

/// Parser for incoming chunks.
class chunk_parser {
public:
  /// Reset to initial parser state.
  void reset();

  /// Parse some data. The tribool return value is true when a complete chunk
  /// has been parsed, false if the data is invalid, indeterminate when more
  /// data is required. The InputIterator return value indicates how much of the
  /// input has been consumed.
  template <typename InputIterator>
  [[nodiscard]] boost::tuple<boost::tribool, InputIterator>
  parse(http::chunk &chunk, http::trailer &trailer, InputIterator begin,
        InputIterator end) {
    while (begin != end) {
      // Fast skip over chunk data
      if (state_ == state::data && remaining_data_count_ > 0) {
        size_t skip = end - begin;
        if (skip > remaining_data_count_) {
          begin += remaining_data_count_;
          remaining_data_count_ = 0;
        } else {
          begin = end;
          remaining_data_count_ -= skip;
          break;
        }
      } else {
        boost::tribool result = consume(chunk, trailer, *begin++);
        if (result || !result)
          return boost::make_tuple(result, begin);
      }
    }
    boost::tribool result = boost::indeterminate;
    return boost::make_tuple(result, begin);
  }

private:
  /// Handle the next character of input.
  [[nodiscard]] boost::tribool consume(http::chunk &chunk,
                                       http::trailer &trailer, char input);

  void save_header_if_non_empty(http::trailer &trailer);

  /// The current state of the parser.
  enum class state {
    length_hex,
    optional_extension,
    expecting_newline_1,
    data,
    header_line_start,
    header_name,
    header_value,
    expecting_newline_2,
    expecting_newline_3,
  } state_{state::length_hex};

  unsigned long long remaining_data_count_;

  std::string extension_{};
  std::string header_name_{};
  std::string header_value_{};
  bool header_value_space_{};
};

} // namespace http_parser
} // namespace proxy

#endif // PROXY_HTTP_PARSER_CHUNK_PARSER_HPP
