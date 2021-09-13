#ifndef PROXY_HTTP_RESPONSE_PRE_BODY_HPP
#define PROXY_HTTP_RESPONSE_PRE_BODY_HPP

#include "body_length_representation.hpp"
#include "header.hpp"
#include <boost/asio.hpp>
#include <string>

namespace proxy {
namespace http {

/// A response pre body received from upstream.
struct response_pre_body {
  std::string http_version_string{};
  std::string code{};
  std::string reason{};
  header_container headers{};

  /// Convert the response into a vector of buffers. The buffers do not own the
  /// underlying memory blocks, therefore the response object must remain valid
  /// and not be changed until the write operation has completed.
  std::vector<boost::asio::const_buffer> to_buffers();
};

bool response_has_body(body_length_representation representation,
                       unsigned long long length);

} // namespace http
} // namespace proxy

#endif // PROXY_HTTP_RESPONSE_PRE_BODY_HPP
