#ifndef PROXY_HTTP_CHUNK_HPP
#define PROXY_HTTP_CHUNK_HPP

namespace proxy {
namespace http {

struct chunk {
  unsigned long long chunk_length{0};

  /// Reset to initial chunk state.
  void reset();
};

} // namespace http
} // namespace proxy

#endif // PROXY_HTTP_CHUNK_HPP
