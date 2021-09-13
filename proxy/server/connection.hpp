#ifndef PROXY_SERVER_CONNECTION_HPP
#define PROXY_SERVER_CONNECTION_HPP

#include "proxy/callbacks/callbacks.hpp"
#include "proxy/http/body_length_representation.hpp"
#include "proxy/http/chunk.hpp"
#include "proxy/http/request_pre_body.hpp"
#include "proxy/http/response_pre_body.hpp"
#include "proxy/http/trailer.hpp"
#include "proxy/http_parser/body_with_length_parser.hpp"
#include "proxy/http_parser/body_without_length_parser.hpp"
#include "proxy/http_parser/request_pre_body_parser.hpp"
#include "proxy/http_parser/response_pre_body_parser.hpp"
#include "reply.hpp"
#include <boost/array.hpp>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/logic/tribool.hpp>
#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/tuple/tuple.hpp>
#include <unordered_map>

namespace proxy {
namespace server {

class connection_manager;

/// Represents a single connection from a client.
/// If the client sends CONNECT request we may tunnel or bump the connection
/// depending on the target hostname.
///
/// Flow for non tunnel http connections:
/// - start
/// - read_from_downstream (*) - may jump to (**) if some leftovers in buffer
/// - handle_downstream_read
/// - process_downstream_read (**) - may jump to (*) if not a whole request
/// pre body was read, may jump to (****) if we are writing body already, here
/// it is decided that it is not tunnel and not bump
/// - connect_to_upstream - may jump to (****) if we are already connected to
/// coorect upstream host
/// - handle_upstream_resolve
/// - upstream_connect (*****)
/// - handle_upstream_connect - may jump to (*****) if we have to iterate over
/// endpoints
/// - write_to_upstream (****)
/// - handle_upstream_write - may jump to (*) if not whole request body was sent
/// so far
/// - read_from_upstream (***)
/// - handle_upstream_read
/// - process_upstream_read - may jump to (***) if not whole response pre body
/// was read
/// - downstream_write
/// - handle_downstream_write - will jump to (*) if whole body was written or
/// there was no body, will jump to (***) if there is more body
///
/// Flow for tunnel connections (it starts like above until tunnel is detected):
/// - start
/// - read_from_downstream (*) - may jump to (**) if some leftovers in buffer
/// - downstream_read_some
/// - handle_downstream_read
/// - process_downstream_read (**) - may jump to (*) if not a whole request
/// pre body was read, here it is decided that it is tunnel
/// - connect_to_upstream
/// - handle_upstream_resolve
/// - upstream_connect (*****)
/// - handle_upstream_connect - may jump to (*****) if we have to iterate over
/// endpoints, if not this starts two simultaneous loops:
/// Loop 1:
/// - read_from_downstream
/// - downstream_read_some
/// - handle_downstream_read
/// - handle_upstream_write
/// Loop 2:
/// - downstream_write (first will write 200 Connection established)
/// - handle_downstream_write
/// - read_from_upstream
/// - downstream_write
/// - handle_upstream_read
///
/// Flow for bump connections (it starts like above until bump is detected, note
/// that connection to upstream is delayed till we get the first request
/// requiring it, not immediately after CONNECT):
/// - start
/// - read_from_downstream (*) - may jump to (**) if some leftovers in buffer
/// - handle_downstream_read
/// - process_downstream_read (**) - may jump to (*) if not a whole request
/// pre body was read, here it is decided that it is bump
/// - downstream_write (200 Connection established)
/// - handle_downstream_write
/// - handle_downstream_handshake
/// - read_from_downstream
/// - downstream_read_some
/// - handle_downstream_read
/// - process_downstream_read
/// - connect_to_upstream
/// - handle_upstream_resolve
/// - upstream_connect (***)
/// - handle_upstream_connect - may jump to (***) if we have to iterate over
/// endpoints
/// - the rest is the same as in the case of non-tunnel case with the upstream
/// already connected

class connection : public boost::enable_shared_from_this<connection>,
                   private boost::noncopyable {
public:
  /// Construct a connection with the given io_context.
  explicit connection(
      boost::asio::io_context &io_context, connection_manager &manager,
      boost::shared_ptr<boost::asio::ip::tcp::resolver> resolver,
      const std::string &ca_private_key, const std::string &ca_certificate,
      std::unordered_map<std::string, boost::tuple<std::string, std::string>>
          &domain_certificates,
      const callbacks::connection_id connection_id,
      callbacks::proxy_callbacks &callbacks);

  /// Get the socket associated with the connection.
  boost::asio::ip::tcp::socket &downstream_socket();

  /// Start the first asynchronous operation for the connection.
  void start();

  /// Stop all asynchronous operations associated with the connection.
  void stop();

private:
  /// Handle completion of a dowstream read operation.
  void handle_downstream_read(const boost::system::error_code &e,
                              std::size_t bytes_transferred);

  /// Handle completion of a downstream error write operation.
  void handle_downstream_error_write(const boost::system::error_code &e);

  boost::tuple<bool, std::string, std::string> extract_host_and_fix_request();

  void connect_to_upstream(std::string &upstream_host,
                           std::string &upstream_service);

  void handle_upstream_resolve(
      const boost::system::error_code &e,
      boost::asio::ip::tcp::resolver::iterator endpoint_iterator);

  void handle_upstream_connect(
      const boost::system::error_code &e,
      boost::asio::ip::tcp::resolver::iterator endpoint_iterator);

  void
  upstream_connect(boost::asio::ip::tcp::resolver::iterator endpoint_iterator);

  void handle_upstream_write(const boost::system::error_code &e);

  void handle_downstream_write(const boost::system::error_code &e);

  void handle_downstream_handshake(const boost::system::error_code &e);

  void handle_upstream_handshake(const boost::system::error_code &e);

  void read_from_downstream();

  void read_from_upstream();

  void write_to_upstream();

  void reset();

  void finish_request();

  bool verify_certificate(bool preverified,
                          boost::asio::ssl::verify_context &ctx);

  /// Handle completion of a dowstream read operation.
  void handle_upstream_read(const boost::system::error_code &e,
                            std::size_t bytes_transferred);

  bool should_response_have_body();

  void process_downstream_read_step_1_pre_body();

  void process_downstream_read_step_2_no_body(bool send_immediately);

  void process_downstream_read_step_2_body(bool send_immediately);

  void process_downstream_read_step_3_action();

  void async_on_connect_method_callback(bool bump);

  void process_upstream_read_step_1_pre_body();

  void process_upstream_read_step_2_no_body(bool send_immediately);

  void process_upstream_read_step_2_body(bool send_immediately);

  void process_upstream_read_step_3_action();

  void connection_manager_stop();

  void shutdown();

  bool is_connection_close(std::string &http_version_string,
                           http::header_container &headers);

  /// Socket for the downstream connection.
  boost::asio::ip::tcp::socket downstream_socket_;

  boost::asio::ssl::context downstream_ssl_context_;

  boost::shared_ptr<boost::asio::ssl::stream<boost::asio::ip::tcp::socket &>>
      downstream_ssl_socket_;

  /// Socket for the downstream connection.
  boost::asio::ip::tcp::socket upstream_socket_;

  boost::asio::ssl::context upstream_ssl_context_;

  boost::shared_ptr<boost::asio::ssl::stream<boost::asio::ip::tcp::socket &>>
      upstream_ssl_socket_;

  /// The manager for this connection.
  connection_manager &connection_manager_;

  /// Buffer for incoming downstream data.
  std::string downstream_read_buffer_ = std::string(8192, 0);

  // First unconsumed byte in the downstream read buffer.
  std::string::iterator downstream_read_buffer_begin_{};
  std::string::iterator downstream_read_buffer_end_{};

  /// Buffer for incoming downstream data.
  std::string upstream_read_buffer_ = std::string(8192, 0);

  // First unconsumed byte in the upstream read buffer.
  std::string::iterator upstream_read_buffer_begin_{};
  std::string::iterator upstream_read_buffer_end_{};

  /// The incoming request.
  http::request_pre_body request_pre_body_{};

  /// The parser for the incoming request pre body.
  http_parser::request_pre_body_parser request_pre_body_parser_{};

  /// The incoming response.
  http::response_pre_body response_pre_body_{};
  http::response_pre_body response_pre_body_100_continue_{};

  http::trailer request_chunked_trailer_{};
  http::trailer response_chunked_trailer_{};

  /// The parser for the incoming response pre body.
  http_parser::response_pre_body_parser response_pre_body_parser_{};

  http_parser::body_with_length_parser request_body_with_length_parser_;
  http_parser::body_without_length_parser request_body_without_length_parser_{};

  http_parser::body_with_length_parser response_body_with_length_parser_;
  http_parser::body_without_length_parser
      response_body_without_length_parser_{};

  std::string private_key_;
  std::string certificate_;

  // Both request and response statuses represent how far along we are reading a
  // request from downstream or reading response from upstream, they have
  // nothing to do with what is the state of what is written to upstream or
  // written do downstream due to callback activity.
  enum class request_state {
    pre_body,
    body,
    finished,
    tunnel,
  } request_state_{request_state::pre_body};

  bool wrote_something_to_upstream_{};
  bool expect_100_continue_from_upstream_{};
  boost::tribool expect_body_continue_from_downstream_what_upstream_sent_{
      boost::indeterminate};
  bool expect_body_continue_from_downstream_{true};

  enum class response_state {
    pre_body,
    body,
    finished,
    tunnel,
  } response_state_{};

  bool response_body_forbidden_{};

  enum class bump_state {
    no_bump,
    handshake,
    established
  } bump_state_{bump_state::no_bump};

  http::body_length_representation request_body_length_representation_{};
  unsigned long long request_body_length_{};
  http::body_length_representation response_body_length_representation_{};
  unsigned long long response_body_length_{};

  template <typename WriteHandler>
  void downstream_write(BOOST_ASIO_MOVE_ARG(WriteHandler) handler);

  template <typename MutableBufferSequence, typename ReadHandler>
  void downstream_read_some(const MutableBufferSequence &buffer,
                            BOOST_ASIO_MOVE_ARG(ReadHandler) handler);

  std::string upstream_requested_host_{};
  std::string upstream_requested_service_{};

  std::vector<boost::asio::const_buffer> outgoing_upstream_buffers_{};
  std::vector<std::unique_ptr<std::string>>
      outgoing_upstream_buffers_strings_{};

  std::vector<boost::asio::const_buffer> outgoing_downstream_buffers_{};
  std::vector<std::unique_ptr<std::string>>
      outgoing_downstream_buffers_strings_{};

  std::string upstream_connected_host_{};
  std::string upstream_connected_service_{};

  std::string connect_connection_established_outgoing_line_{};

  bool connection_close_{};

  bool upstream_died_while_reading_from_it_{};
  bool stopped_{};

  /// The stock reply to be sent back to the client.
  reply reply_{};

  boost::shared_ptr<boost::asio::ip::tcp::resolver> resolver_;

  const std::string &ca_private_key_;
  const std::string &ca_certificate_;
  std::unordered_map<std::string, boost::tuple<std::string, std::string>>
      &domain_certificates_;

  // Unique identifier for connection.
  const callbacks::connection_id connection_id_;

  // Unique (per connection_id_) identifier for request.
  callbacks::request_id request_id_{};

  callbacks::proxy_callbacks &callbacks_;
};

typedef boost::shared_ptr<connection> connection_ptr;

} // namespace server
} // namespace proxy

#endif // PROXY_SERVER_CONNECTION_HPP
