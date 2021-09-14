#ifndef PROXY_SERVER_SERVER_HPP
#define PROXY_SERVER_SERVER_HPP

#include "connection.hpp"
#include "connection_manager.hpp"
#include "proxy/callbacks/callbacks.hpp"
#include <boost/asio.hpp>
#include <boost/noncopyable.hpp>
#include <string>
#include <unordered_map>

namespace proxy {
namespace server {

/// The top-level class of the proxy server.
class server : private boost::noncopyable {
public:
  /// Construct the server to listen on the specified TCP address and port, and
  /// serve up files from the given directory.
  explicit server(const std::string &address, const std::string &port,
                  callbacks::proxy_callbacks &callbacks);

  /// Run the server's io_context loop.
  void run();

private:
  /// Initiate an asynchronous accept operation.
  void start_accept();

  /// Handle completion of an asynchronous accept operation.
  void handle_accept(const boost::system::error_code &e);

  /// Handle a request to stop the server.
  void handle_stop();

  /// The io_context used to perform asynchronous operations.
  boost::asio::io_context io_context_;

  /// The signal_set is used to register for process termination notifications.
  boost::asio::signal_set signals_;

  /// Acceptor used to listen for incoming connections.
  boost::asio::ip::tcp::acceptor acceptor_;

  /// The connection manager which owns all live connections.
  connection_manager connection_manager_;

  /// The next connection to be accepted.
  connection_ptr new_connection_;

  callbacks::connection_id connection_id_ = 0;

  boost::shared_ptr<boost::asio::ip::tcp::resolver> resolver_;

  std::string ca_private_key_;
  std::string ca_certificate_;
  std::unordered_map<std::string, boost::tuple<std::string, std::string>>
      domain_certificates_{};

  boost::asio::ssl::context upstream_ssl_context_;

  callbacks::proxy_callbacks &callbacks_;
};

} // namespace server
} // namespace proxy

#endif // PROXY_SERVER_SERVER_HPP
