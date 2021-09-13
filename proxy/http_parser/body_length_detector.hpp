#ifndef PROXY_HTTP_PARSER_BODY_LENGTH_DETECTOR_HPP
#define PROXY_HTTP_PARSER_BODY_LENGTH_DETECTOR_HPP

#include "proxy/http/body_length_representation.hpp"
#include "proxy/http/header.hpp"
#include <boost/logic/tribool.hpp>
#include <boost/tuple/tuple.hpp>
#include <list>

namespace proxy {
namespace http_parser {

// Return:
// <false, none, 0> for error
// <true, content_length, 0+> for length
// <true, chunked, 0> for chunked
// <true, none, 0> for lack of header (infinite response or empty
// request).
[[nodiscard]] boost::tuple<bool, http::body_length_representation,
                           unsigned long long>
detect_body_length(http::header_container &headers);

} // namespace http_parser
} // namespace proxy

#endif // PROXY_HTTP_PARSER_BODY_LENGTH_DETECTOR_HPP