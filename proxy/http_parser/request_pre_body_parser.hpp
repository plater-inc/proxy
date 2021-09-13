#ifndef PROXY_HTTP_PARSER__REQUEST_PRE_BODY_PARSER_HPP
#define PROXY_HTTP_PARSER__REQUEST_PRE_BODY_PARSER_HPP

#include "proxy/http/request_pre_body.hpp"
#include <boost/logic/tribool.hpp>
#include <boost/tuple/tuple.hpp>

namespace proxy {
namespace http_parser {

/// Parser for incoming requests.
class request_pre_body_parser {
public:
  /// Reset to initial parser state.
  void reset();

  /// Parse some data. The tribool return value is true when a complete request
  /// pre body has been parsed, false if the data is invalid, indeterminate when
  /// more data is required. The InputIterator return value indicates how much
  /// of the input has been consumed.
  template <typename InputIterator>
  [[nodiscard]] boost::tuple<boost::tribool, InputIterator>
  parse(http::request_pre_body &req, InputIterator begin, InputIterator end) {
    while (begin != end) {
      boost::tribool result = consume(req, *begin++);
      if (result || !result)
        return boost::make_tuple(result, begin);
    }
    boost::tribool result = boost::indeterminate;
    return boost::make_tuple(result, begin);
  }

private:
  /// Handle the next character of input.
  [[nodiscard]] boost::tribool consume(http::request_pre_body &req, char input);

  void save_header_if_non_empty(http::request_pre_body &req);

  /// The current state of the parser.
  enum class state {
    method_start,
    method,
    uri,
    http_version_h,
    http_version_t_1,
    http_version_t_2,
    http_version_p,
    http_version_slash,
    http_version_major_start,
    http_version_major,
    http_version_minor_start,
    http_version_minor,
    expecting_newline_1,
    header_line_start,
    header_name,
    header_value,
    expecting_newline_2,
    expecting_newline_3
  } state_{state::method_start};

  std::string header_name_{};
  std::string header_value_{};
  bool header_value_space_{};
};

} // namespace http_parser
} // namespace proxy

#endif // PROXY_HTTP_PARSER__REQUEST_PRE_BODY_PARSER_HPP
