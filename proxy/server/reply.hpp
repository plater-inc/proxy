#ifndef PROXY_SERVER_REPLY_HPP
#define PROXY_SERVER_REPLY_HPP

#include "proxy/http/header.hpp"
#include <boost/asio.hpp>
#include <string>
#include <vector>

namespace proxy {
namespace server {

/// A reply to be sent to a client.
struct reply {
  /// The status of the reply.
  enum class status_type {
    bad_request = 400,
    bad_gateway = 502,
  } status{};

  /// The headers to be included in the reply.
  std::vector<http::header> headers{};

  /// The content to be sent in the reply.
  std::string content{};

  /// Convert the reply into a vector of buffers. The buffers do not own the
  /// underlying memory blocks, therefore the reply object must remain valid and
  /// not be changed until the write operation has completed.
  std::vector<boost::asio::const_buffer> to_buffers();

  /// Get a stock reply.
  static reply stock_reply(status_type status);
};

} // namespace server
} // namespace proxy

#endif // PROXY_SERVER_REPLY_HPP
