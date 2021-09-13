#ifndef PROXY_HTTP_BODY_LENGTH_REPRESENTATION_HPP
#define PROXY_HTTP_BODY_LENGTH_REPRESENTATION_HPP

namespace proxy {
namespace http {

enum class body_length_representation {
  content_length,
  chunked,
  none,
};

} // namespace http
} // namespace proxy

#endif // PROXY_HTTP_BODY_LENGTH_REPRESENTATION_HPP
