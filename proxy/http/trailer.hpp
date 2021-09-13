#ifndef PROXY_HTTP_TRAILER_HPP
#define PROXY_HTTP_TRAILER_HPP

#include "header.hpp"
#include <boost/asio.hpp>
#include <string>

namespace proxy {
namespace http {

/// A trailer - last 0 length chunk with optional headers.
struct trailer {
  // Everything after the chunk length - including front ; if extension is
  // present
  std::string extension{};
  header_container headers{};

  /// Convert the request into a vector of buffers. The buffers do not own the
  /// underlying memory blocks, therefore the object must remain valid
  /// and not be changed until the write operation has completed.
  std::vector<boost::asio::const_buffer> to_buffers();

private:
  static const std::string prefix; // 0
};

} // namespace http
} // namespace proxy

#endif // PROXY_HTTP_TRAILER_HPP
