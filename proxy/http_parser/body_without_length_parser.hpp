#ifndef PROXY_HTTP_PARSER_BODY_WITHOUT_LENGTH_PARSER_HPP
#define PROXY_HTTP_PARSER_BODY_WITHOUT_LENGTH_PARSER_HPP

#include "chunk_parser.hpp"
#include "proxy/http/chunk.hpp"
#include "proxy/http/trailer.hpp"
#include <boost/logic/tribool.hpp>
#include <boost/tuple/tuple.hpp>

namespace proxy {
namespace http_parser {

/// Parser for incoming body without length.
class body_without_length_parser {
public:
  /// Reset to initial parser state.
  void reset();

  /// Parse some data. The tribool return value is true when a complete body
  /// has been parsed, false if the data is invalid, indeterminate when more
  /// data is required. The first InputIterator return value indicates how much
  /// of the input has been consumed and the second indicates where the last non
  /// zero chunk ends.
  template <typename InputIterator>
  [[nodiscard]] boost::tuple<boost::logic::tribool, InputIterator,
                             InputIterator>
  parse(http::trailer &trailer, InputIterator begin, InputIterator end) {
    InputIterator last_non_zero_chunk_it = begin;
    while (begin != end) {
      boost::tribool chunk_parser_result;
      boost::tie(chunk_parser_result, begin) =
          chunk_parser_.parse(chunk_, trailer, begin, end);
      if (chunk_parser_result) {
        if (chunk_.chunk_length == 0) {
          boost::logic::tribool result = true;
          return boost::make_tuple(result, begin, last_non_zero_chunk_it);
        }
        chunk_.reset();
        chunk_parser_.reset();
      } else if (!chunk_parser_result) {
        boost::logic::tribool result = false;
        return boost::make_tuple(result, begin, last_non_zero_chunk_it);
      } else if (begin == end && chunk_.chunk_length == 0) {
        break;
      }
      last_non_zero_chunk_it = begin;
    }
    // We've completely parsed chunks and run out of buffer.
    boost::logic::tribool result = boost::indeterminate;
    return boost::make_tuple(result, begin, last_non_zero_chunk_it);
  }

private:
  http::chunk chunk_{};
  chunk_parser chunk_parser_{};
};

} // namespace http_parser
} // namespace proxy

#endif // PROXY_HTTP_PARSER_BODY_WITHOUT_LENGTH_PARSER_HPP
