#ifndef PROXY_HTTP_PARSER_BODY_WITH_LENGTH_PARSER_HPP
#define PROXY_HTTP_PARSER_BODY_WITH_LENGTH_PARSER_HPP

#include <boost/logic/tribool.hpp>
#include <boost/tuple/tuple.hpp>

namespace proxy {
namespace http_parser {

/// Parser for incoming body with length.
class body_with_length_parser {
public:
  /// Reset to initial parser state.
  void reset(unsigned long long length);

  /// Parse some data. The tribool return value is true when a complete body
  /// has been parsed, false if the data is invalid, indeterminate when more
  /// data is required. The InputIterator return value indicates how much of the
  /// input has been consumed.
  template <typename InputIterator>
  [[nodiscard]] boost::tuple<boost::tribool, InputIterator>
  parse(InputIterator begin, InputIterator end) {
    if (remaining_body_count_ == 0) {
      boost::tribool result = true;
      return boost::make_tuple(result, begin);
    }
    size_t skip = end - begin;
    if (skip >= remaining_body_count_) {
      begin += remaining_body_count_;
      remaining_body_count_ = 0;
      boost::tribool result = true;
      return boost::make_tuple(result, begin);
    } else {
      begin = end;
      remaining_body_count_ -= skip;
      boost::tribool result = boost::indeterminate;
      return boost::make_tuple(result, begin);
    }
  }

  unsigned long long remaining_body_count_{0};
};

} // namespace http_parser
} // namespace proxy

#endif // PROXY_HTTP_PARSER_BODY_WITH_LENGTH_PARSER_HPP
