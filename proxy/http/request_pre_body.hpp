#ifndef PROXY_HTTP_REQUEST_PRE_BODY_HPP
#define PROXY_HTTP_REQUEST_PRE_BODY_HPP

#include "body_length_representation.hpp"
#include "header.hpp"
#include <boost/asio.hpp>
#include <string>

namespace proxy {
namespace http {

/// A request received from a client.
struct request_pre_body {
  std::string method{};
  std::string uri{};
  std::string http_version_string{}; // e.g. HTTP/1.1
  header_container headers{};

  /// Convert the request into a vector of buffers. The buffers do not own the
  /// underlying memory blocks, therefore the request object must remain valid
  /// and not be changed until the write operation has completed.
  std::vector<boost::asio::const_buffer> to_buffers();
};

bool request_has_body(body_length_representation representation,
                      unsigned long long length);

} // namespace http
} // namespace proxy

#endif // PROXY_HTTP_REQUEST_PRE_BODY_HPP
