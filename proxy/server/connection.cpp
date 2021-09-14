#include "connection.hpp"
#include "connection_manager.hpp"
#include "proxy/cert/certificate_generator.hpp"
#include "proxy/http_parser/body_length_detector.hpp"
#include "proxy/logging/logging.hpp"
#include "proxy/util/misc_strings.hpp"
#include "proxy/util/utils.hpp"
#include <boost/algorithm/string.hpp>
#include <boost/bind/bind.hpp>
#include <ostream>
#include <vector>

namespace proxy {
namespace server {

// TODO: TCP_NODELAY for downstream
// TODO: connections with port number in URL HTTP/HTTPS (with and without HSTS)
// TODO: host passed to callbacks should be augmented with host header
// TODO: HTTP 0.9?
// TODO: reject HTTP 2.0
// TODO: queue of buffers?
// TODO: check if all boost functions are called nothrow (with error code param)
// TODO: adjust TE header:
// https://developer.mozilla.org/en-US/docs/Web/HTTP/Headers/TE
// TODO: https://pinning-test.badssl.com/ and https://revoked.badssl.com/

connection::connection(
    boost::asio::io_context &io_context, connection_manager &manager,
    boost::shared_ptr<boost::asio::ip::tcp::resolver> resolver,
    const std::string &ca_private_key, const std::string &ca_certificate,
    std::unordered_map<std::string, boost::tuple<std::string, std::string>>
        &domain_certificates,
    const callbacks::connection_id connection_id,
    callbacks::proxy_callbacks &callbacks)
    : downstream_socket_(io_context),
      downstream_ssl_context_(boost::asio::ssl::context::tlsv12),
      upstream_socket_(io_context),
      upstream_ssl_context_(boost::asio::ssl::context::tlsv12),
      connection_manager_(manager), resolver_(resolver),
      ca_private_key_(ca_private_key), ca_certificate_(ca_certificate),
      domain_certificates_(domain_certificates), connection_id_(connection_id),
      callbacks_(callbacks) {
  upstream_ssl_context_.set_default_verify_paths();
}

boost::asio::ip::tcp::socket &connection::downstream_socket() {
  return downstream_socket_;
}

void connection::start() {
  DVLOG(1) << "start(" << connection_id_ << ")\n";

  callbacks_.async_on_connection(
      connection_id_,
      boost::bind(&connection::read_from_downstream, shared_from_this()));
}

void connection::stop() {
  // Sometimes both read and write can fail at the same time and stop will be
  // called twice. Deduplicate here.
  if (!stopped_) {
    DVLOG(1) << "stop(" << connection_id_ << ")\n";

    stopped_ = true;
    if ((request_state_ != request_state::tunnel &&
         request_state_ != request_state::pre_body) &&
        (request_state_ != request_state::finished ||
         (wrote_something_to_upstream_ &&
          response_state_ != response_state::finished))) {
      callbacks_.async_on_response_finished(
          connection_id_, request_id_,
          boost::bind(&connection::shutdown, shared_from_this()));
    } else {
      shutdown();
    }
  }
}

void connection::shutdown() {
  boost::system::error_code ignored_ec;
  // TODO async_shutdown?
  // TODO SSL socket
  downstream_socket_.shutdown(boost::asio::socket_base::shutdown_both,
                              ignored_ec);
  downstream_socket_.close();
  upstream_socket_.shutdown(boost::asio::socket_base::shutdown_both,
                            ignored_ec);
  upstream_socket_.close();
  callbacks_.on_connection_finished(connection_id_);
}

[[nodiscard]] boost::tuple<bool, std::string, std::string>
connection::extract_host_and_fix_request() {
  int index_start = -1;
  int count = 0;
  int slash_count = 0;
  for (int i = 0; i < request_pre_body_.uri.length(); i++) {
    if (request_pre_body_.uri[i] == '/') {
      slash_count++;
    } else {
      if (slash_count == 2) {
        if (index_start == -1) {
          index_start = i;
        }
        count++;
      } else if (slash_count == 3) {
        break;
      }
    }
  }

  if (index_start == -1 || count == 0 || slash_count < 2) {
    return boost::make_tuple(false, "" /* ignored */, "" /* ignored */);
  }

  std::string upstream_host_port =
      request_pre_body_.uri.substr(index_start, count);
  std::vector<std::string> host_port_parts;
  boost::split(host_port_parts, upstream_host_port, boost::is_any_of(":"));
  // We check for count == 0 before, so here we know the vector will not be
  // empty.
  std::string upstream_host = host_port_parts[0];
  std::string upstream_port =
      host_port_parts.size() == 1 ? "80" : host_port_parts[1];

  if (slash_count == 2) {
    // Python urllib sends http://example.com without trailing slash
    request_pre_body_.uri = "/";
  } else {
    request_pre_body_.uri = request_pre_body_.uri.substr(index_start + count);
  }

  http::header_container::name_iterator proxy_connection_it =
      request_pre_body_.headers.find("proxy-connection");
  if (proxy_connection_it != request_pre_body_.headers.end()) {
    if (request_pre_body_.headers.find("connection") ==
        request_pre_body_.headers.end()) {
      request_pre_body_.headers.push_back(
          {"Connection", proxy_connection_it->value});
    }
    request_pre_body_.headers.erase_all("proxy-connection");
  }

  return boost::make_tuple(true, upstream_host, upstream_port);
}

void connection::connect_to_upstream(std::string &upstream_host,
                                     std::string &upstream_service) {
  DVLOG(2) << "connect_to_upstream(" << connection_id_ << ", " << request_id_
           << ", " << upstream_host << ", " << upstream_service << ")";

  if (!upstream_connected_host_.empty() ||
      !upstream_connected_service_.empty()) {
    if (upstream_connected_host_ != upstream_host ||
        upstream_connected_service_ != upstream_service) {
      connection_manager_stop();
    } else {
      write_to_upstream();
    }
    return;
  }
  upstream_connected_host_ = upstream_host;
  upstream_connected_service_ = upstream_service;
  boost::asio::ip::tcp::resolver::query query(upstream_connected_host_,
                                              upstream_connected_service_);
  resolver_->async_resolve(
      query, boost::bind(&connection::handle_upstream_resolve,
                         shared_from_this(), boost::asio::placeholders::error,
                         boost::asio::placeholders::iterator));
}

void connection::handle_upstream_resolve(
    const boost::system::error_code &e,
    boost::asio::ip::tcp::resolver::iterator endpoint_iterator) {
  if (!e) {
    upstream_connect(endpoint_iterator);

  } else if (e != boost::asio::error::operation_aborted) {
    connection_manager_stop();
  }
}

void connection::upstream_connect(
    boost::asio::ip::tcp::resolver::iterator endpoint_iterator) {
  upstream_socket_.open(boost::asio::ip::tcp::v4());
  upstream_socket_.set_option(boost::asio::ip::tcp::no_delay(true));
  upstream_socket_.async_connect(
      *endpoint_iterator,
      boost::bind(&connection::handle_upstream_connect, shared_from_this(),
                  boost::asio::placeholders::error, ++endpoint_iterator));
}

void connection::write_to_upstream() {
  DVLOG_IF(2, request_state_ != request_state::tunnel)
      << logging::FORMAT_FG_BLUE << "write_to_upstream(" << connection_id_
      << ", " << request_id_ << "):\n"
      << util::utils::vector_of_buffers_to_string(outgoing_upstream_buffers_)
      << logging::FORMAT_RESET;
  DVLOG_IF(10, request_state_ == request_state::tunnel)
      << logging::FORMAT_FG_BLUE << "write_to_upstream(" << connection_id_
      << ", " << request_id_ << "):\n"
      << util::utils::vector_of_buffers_to_string(outgoing_upstream_buffers_)
      << logging::FORMAT_RESET;

  if (bump_state_ == bump_state::established) {
    boost::asio::async_write(*upstream_ssl_socket_, outgoing_upstream_buffers_,
                             boost::bind(&connection::handle_upstream_write,
                                         shared_from_this(),
                                         boost::asio::placeholders::error));
  } else {
    boost::asio::async_write(upstream_socket_, outgoing_upstream_buffers_,
                             boost::bind(&connection::handle_upstream_write,
                                         shared_from_this(),
                                         boost::asio::placeholders::error));
  }
  outgoing_upstream_buffers_.clear();
}

template <typename WriteHandler>
void connection::downstream_write(BOOST_ASIO_MOVE_ARG(WriteHandler) handler) {
  DVLOG_IF(2, response_state_ != response_state::tunnel)
      << logging::FORMAT_FG_CYAN << "downstream_write(" << connection_id_
      << ", " << request_id_ << "):\n"
      << util::utils::vector_of_buffers_to_string(outgoing_downstream_buffers_)
      << logging::FORMAT_RESET;
  DVLOG_IF(10, response_state_ == response_state::tunnel)
      << logging::FORMAT_FG_CYAN << "downstream_write(" << connection_id_
      << ", " << request_id_ << "):\n"
      << util::utils::vector_of_buffers_to_string(outgoing_downstream_buffers_)
      << logging::FORMAT_RESET;

  if (bump_state_ == bump_state::established) {
    boost::asio::async_write(*downstream_ssl_socket_,
                             outgoing_downstream_buffers_, handler);
  } else {
    boost::asio::async_write(downstream_socket_, outgoing_downstream_buffers_,
                             handler);
  }
  outgoing_downstream_buffers_.clear();
}

template <typename MutableBufferSequence, typename ReadHandler>
void connection::downstream_read_some(const MutableBufferSequence &buffer,
                                      BOOST_ASIO_MOVE_ARG(ReadHandler)
                                          handler) {
  if (bump_state_ == bump_state::established) {
    downstream_ssl_socket_->async_read_some(buffer, handler);
  } else {
    downstream_socket_.async_read_some(buffer, handler);
  }
}

void connection::handle_upstream_connect(
    const boost::system::error_code &e,
    boost::asio::ip::tcp::resolver::iterator endpoint_iterator) {
  DVLOG(2) << "handle_upstream_connect(" << connection_id_ << ", "
           << request_id_ << ", " << e << ")";
  if (!e) {
    if (request_state_ == request_state::tunnel) {
      // The connection was successful. Start tunnel.
      connect_connection_established_outgoing_line_ =
          request_pre_body_.http_version_string +
          " 200 Connection established\r\n\r\n";
      outgoing_downstream_buffers_.emplace_back(
          boost::asio::buffer(connect_connection_established_outgoing_line_));
      downstream_write(boost::bind(&connection::handle_downstream_write,
                                   shared_from_this(),
                                   boost::asio::placeholders::error));
      read_from_downstream();
    } else if (bump_state_ == bump_state::established) {
      upstream_ssl_socket_.reset(
          new boost::asio::ssl::stream<boost::asio::ip::tcp::socket &>(
              upstream_socket_, upstream_ssl_context_));
      upstream_ssl_socket_->set_verify_mode(boost::asio::ssl::verify_peer);
      upstream_ssl_socket_->set_verify_callback(
          boost::asio::ssl::host_name_verification(upstream_connected_host_));
      SSL_set_tlsext_host_name(upstream_ssl_socket_->native_handle(),
                               upstream_connected_host_.c_str());

      upstream_ssl_socket_->async_handshake(
          boost::asio::ssl::stream_base::client,
          boost::bind(&connection::handle_upstream_handshake,
                      shared_from_this(), boost::asio::placeholders::error));

    } else {
      // The connection was successful. Send the request.
      write_to_upstream();
    }
  } else if (endpoint_iterator != boost::asio::ip::tcp::resolver::iterator()) {
    upstream_socket_.close();
    upstream_connect(endpoint_iterator);
  } else if (e != boost::asio::error::operation_aborted) {
    connection_manager_stop();
  }
}

void connection::handle_upstream_handshake(const boost::system::error_code &e) {
  if (!e) {
    write_to_upstream();
  } else if (e != boost::asio::error::operation_aborted) {
    connection_manager_stop();
  }
}

void connection::reset() {
  connection_close_ = false;

  request_pre_body_ = {};
  response_pre_body_ = {};
  response_pre_body_100_continue_ = {};
  request_chunked_trailer_ = {};
  response_chunked_trailer_ = {};

  expect_100_continue_from_upstream_ = false;
  expect_body_continue_from_downstream_ = true;
  expect_body_continue_from_downstream_what_upstream_sent_ =
      boost::indeterminate;

  request_pre_body_parser_.reset();
  request_body_without_length_parser_.reset();
  response_pre_body_parser_.reset();
  response_body_without_length_parser_.reset();

  request_state_ = request_state::pre_body;
  wrote_something_to_upstream_ = false;

  response_body_forbidden_ = false;

  request_body_length_representation_ = http::body_length_representation::none;
  request_body_length_ = 0ULL;
  response_body_length_representation_ = http::body_length_representation::none;
  response_body_length_ = 0ULL;

  request_id_++;
}

void connection::handle_upstream_write(const boost::system::error_code &e) {
  if (!e) {
    outgoing_upstream_buffers_strings_.clear();
    if (response_state_ != response_state::tunnel) {
      response_state_ = response_state::pre_body;
    }
    if (request_state_ == request_state::finished ||
        expect_100_continue_from_upstream_) {
      read_from_upstream();
    } else {
      read_from_downstream();
    }
  } else if (e != boost::asio::error::operation_aborted) {
    connection_manager_stop();
  }
}

void connection::finish_request() {
  callbacks::request_id request_id = request_id_;
  reset();
  callbacks_.async_on_response_finished(
      connection_id_, request_id,
      boost::bind(
          upstream_died_while_reading_from_it_ || connection_close_ ||
                  (expect_body_continue_from_downstream_what_upstream_sent_ !=
                       boost::indeterminate &&
                   expect_body_continue_from_downstream_ !=
                       expect_body_continue_from_downstream_what_upstream_sent_)
              ? &connection::connection_manager_stop
              : &connection::read_from_downstream,
          shared_from_this()));
}

void connection::handle_downstream_write(const boost::system::error_code &e) {
  if (!e) {
    outgoing_downstream_buffers_strings_.clear();
    if (bump_state_ == bump_state::handshake) {
      boost::tie(private_key_, certificate_) =
          domain_certificates_[upstream_requested_host_];
      if (private_key_ == "" || certificate_ == "") {
        boost::tie(private_key_, certificate_) = cert::generate_certificate(
            upstream_requested_host_, ca_private_key_, ca_certificate_);
        // Create chain
        certificate_ += ca_certificate_;
        domain_certificates_[upstream_requested_host_] =
            boost::make_tuple(private_key_, certificate_);
      }

      // TODO: context max length
      std::string session_id_context =
          upstream_requested_service_ + ':' + upstream_requested_host_;
      downstream_ssl_context_.use_certificate_chain(
          boost::asio::buffer(certificate_));
      downstream_ssl_context_.use_private_key(boost::asio::buffer(private_key_),
                                              boost::asio::ssl::context::pem);
      downstream_ssl_socket_.reset(
          new boost::asio::ssl::stream<boost::asio::ip::tcp::socket &>(
              downstream_socket_, downstream_ssl_context_));
      downstream_ssl_socket_->async_handshake(
          boost::asio::ssl::stream_base::server,
          boost::bind(&connection::handle_downstream_handshake,
                      shared_from_this(), boost::asio::placeholders::error));
    } else if (request_state_ == request_state::finished &&
               (!wrote_something_to_upstream_ ||
                response_state_ == response_state::finished)) {
      // We reverse direction - read next request
      finish_request();
    } else {
      if (request_state_ == request_state::body) {
        // We are reading body, but we already wrote something to downstream -
        // we have to read more body now.
        read_from_downstream();
      } else {
        read_from_upstream();
      }
    }
  } else if (e != boost::asio::error::operation_aborted) {
    connection_manager_stop();
  }
}

void connection::handle_downstream_handshake(
    const boost::system::error_code &e) {
  bump_state_ = bump_state::established;
  request_pre_body_ = {};
  request_pre_body_parser_.reset();
  read_from_downstream();
}

void connection::read_from_downstream() {
  DVLOG_IF(3, request_state_ != request_state::tunnel)
      << logging::FORMAT_FG_CYAN << "read_from_downstream(" << connection_id_
      << ", " << request_id_ << ")" << logging::FORMAT_RESET;
  DVLOG_IF(11, request_state_ == request_state::tunnel)
      << logging::FORMAT_FG_CYAN << "read_from_downstream(" << connection_id_
      << ", " << request_id_ << ")" << logging::FORMAT_RESET;
  if (downstream_read_buffer_begin_ == downstream_read_buffer_end_) {
    downstream_read_some(
        boost::asio::buffer(downstream_read_buffer_),
        boost::bind(&connection::handle_downstream_read, shared_from_this(),
                    boost::asio::placeholders::error,
                    boost::asio::placeholders::bytes_transferred));
  } else {
    process_downstream_read_step_1_pre_body();
  }
}

void connection::read_from_upstream() {
  DVLOG_IF(3, response_state_ != response_state::tunnel)
      << logging::FORMAT_FG_BLUE << "read_from_upstream(" << connection_id_
      << ", " << request_id_ << ")" << logging::FORMAT_RESET;
  DVLOG_IF(11, response_state_ == response_state::tunnel)
      << logging::FORMAT_FG_BLUE << "read_from_upstream(" << connection_id_
      << ", " << request_id_ << ")" << logging::FORMAT_RESET;
  if (upstream_read_buffer_begin_ == upstream_read_buffer_end_) {
    if (bump_state_ == bump_state::established) {
      upstream_ssl_socket_->async_read_some(
          boost::asio::buffer(upstream_read_buffer_),
          boost::bind(&connection::handle_upstream_read, shared_from_this(),
                      boost::asio::placeholders::error,
                      boost::asio::placeholders::bytes_transferred));
    } else {
      upstream_socket_.async_read_some(
          boost::asio::buffer(upstream_read_buffer_),
          boost::bind(&connection::handle_upstream_read, shared_from_this(),
                      boost::asio::placeholders::error,
                      boost::asio::placeholders::bytes_transferred));
    }
  } else {
    process_upstream_read_step_1_pre_body();
  }
}

bool connection::should_response_have_body() {
  // We cannot just check if we have Content-Length (or Transfer-Encoding),
  // because: "A server MAY send a Content-Length header field in a 304 (Not
  // Modified) response to a conditional GET request (Section 4.1 of
  // [RFC7232]); a server MUST NOT send Content-Length in such a response
  // unless its field-value equals the decimal number of octets that would
  // have been sent in the payload body of a 200 (OK) response to the same
  // request."
  if (request_pre_body_.method == "HEAD") {
    return false;
  }
  if (response_pre_body_.code.length() > 1 &&
      response_pre_body_.code[0] == '1') {
    return false;
  }
  if (response_pre_body_.code == "204" || response_pre_body_.code == "304") {
    return false;
  }
  return true;
}

void connection::handle_upstream_read(const boost::system::error_code &e,
                                      std::size_t bytes_transferred) {
  if (!e) {
    if (response_state_ == response_state::tunnel) {
      outgoing_downstream_buffers_.emplace_back(&*upstream_read_buffer_.begin(),
                                                bytes_transferred);
      downstream_write(boost::bind(&connection::handle_downstream_write,
                                   shared_from_this(),
                                   boost::asio::placeholders::error));
    } else {
      upstream_read_buffer_begin_ = upstream_read_buffer_.begin();
      upstream_read_buffer_end_ =
          upstream_read_buffer_.begin() + bytes_transferred;
      process_upstream_read_step_1_pre_body();
    }
  } else if (e != boost::asio::error::operation_aborted) {
    if (response_state_ == response_state::pre_body ||
        response_state_ == response_state::body) {
      // We were reading response and finished early (possible infinite
      // response).
      upstream_died_while_reading_from_it_ = true;
      response_state_ = response_state::finished;
      callbacks::proxy_callbacks::on_response_body_some_params
          on_response_body_some_params{
              .connection_id = connection_id_,
              .request_id = request_id_,
              .ssl = bump_state_ == bump_state::established,
              .host = upstream_requested_host_,
              .request_pre_body = request_pre_body_,
              .response_pre_body = response_pre_body_,
              .response_chunked_trailer = response_chunked_trailer_,
              .response_body_forbidden = response_body_forbidden_,
              .response_has_more_body = false,
              .body_begin = upstream_read_buffer_begin_,
              .body_end = upstream_read_buffer_end_,
              .response_body_length_representation =
                  response_body_length_representation_,
              .response_body_length = response_body_length_,
              .outgoing_downstream_buffers = outgoing_downstream_buffers_,
              .outgoing_downstream_buffers_strings =
                  outgoing_downstream_buffers_strings_,
              .callback =
                  boost::bind(&connection::process_upstream_read_step_3_action,
                              shared_from_this())};
      callbacks_.async_on_response_body_some(on_response_body_some_params);
    } else {
      connection_manager_stop();
    }
  }
}

void connection::connection_manager_stop() {
  connection_manager_.stop(shared_from_this());
}

bool connection::is_connection_close(std::string &http_version_string,
                                     http::header_container &headers) {
  http::header_container::name_iterator connection_it =
      headers.find("connection");
  return http_version_string != "HTTP/1.1" || connection_it->value == "close";
}

void connection::process_upstream_read_step_1_pre_body() {
  DVLOG(1) << logging::FORMAT_FG_YELLOW
           << "process_upstream_read_step_1_pre_body(" << connection_id_ << ", "
           << request_id_ << "):\n"
           << boost::string_view(&*upstream_read_buffer_begin_,
                                 upstream_read_buffer_end_ -
                                     upstream_read_buffer_begin_)
           << logging::FORMAT_RESET;

  if (response_state_ == response_state::pre_body) {
    boost::tribool result;
    boost::tie(result, upstream_read_buffer_begin_) =
        response_pre_body_parser_.parse(response_pre_body_,
                                        upstream_read_buffer_begin_,
                                        upstream_read_buffer_end_);
    if (result) {
      if (response_pre_body_.code == "100") {
        expect_100_continue_from_upstream_ = false;
        response_pre_body_100_continue_.code = response_pre_body_.code;
        response_pre_body_100_continue_.http_version_string =
            response_pre_body_.http_version_string;
        response_pre_body_100_continue_.reason = response_pre_body_.reason;
        expect_body_continue_from_downstream_what_upstream_sent_ = true;
        for (http::header_container::iterator it =
                 response_pre_body_.headers.begin();
             it != response_pre_body_.headers.end(); it++) {
          response_pre_body_100_continue_.headers.push_back(*it);
        }
        response_pre_body_ = {};
        response_pre_body_parser_.reset();
        wrote_something_to_upstream_ = false;
        callbacks::proxy_callbacks::on_response_pre_body_params
            on_response_pre_body_params{
                .connection_id = connection_id_,
                .request_id = request_id_,
                .ssl = bump_state_ == bump_state::established,
                .host = upstream_requested_host_,
                .request_pre_body = request_pre_body_,
                .response_pre_body = response_pre_body_100_continue_,
                .expect_body_continue_from_downstream =
                    expect_body_continue_from_downstream_,
                .response_body_forbidden = true,
                .response_body_length_representation =
                    response_body_length_representation_,
                .response_body_length = response_body_length_,
                .outgoing_downstream_buffers = outgoing_downstream_buffers_,
                .outgoing_downstream_buffers_strings =
                    outgoing_downstream_buffers_strings_,
                .callback =
                    boost::bind(&connection::process_upstream_read_step_2_body,
                                shared_from_this(), boost::placeholders::_1)};
        callbacks_.async_on_response_pre_body(on_response_pre_body_params);
        return;
      } else {
        if (expect_100_continue_from_upstream_) {
          expect_body_continue_from_downstream_ = false;
          expect_body_continue_from_downstream_what_upstream_sent_ = false;
          expect_100_continue_from_upstream_ = false;
        }
        if (is_connection_close(response_pre_body_.http_version_string,
                                response_pre_body_.headers)) {
          connection_close_ = true;
        }
        response_state_ = response_state::body;
        bool detect_body_length_result;
        boost::tie(detect_body_length_result,
                   response_body_length_representation_,
                   response_body_length_) =
            http_parser::detect_body_length(response_pre_body_.headers);
        if (should_response_have_body()) {
          if (!detect_body_length_result) {
            reply_ = reply::stock_reply(reply::status_type::bad_gateway);
            outgoing_downstream_buffers_ = reply_.to_buffers();
            downstream_write(boost::bind(
                &connection::handle_downstream_error_write, shared_from_this(),
                boost::asio::placeholders::error));
            return;
          } else if (response_body_length_representation_ !=
                     http::body_length_representation::chunked) {
            if (response_body_length_representation_ ==
                http::body_length_representation::none) {
              // Infinite response
              response_body_with_length_parser_.reset(ULLONG_MAX);
            } else if (response_body_length_ == 0) {
              response_state_ = response_state::finished;
            } else {
              response_body_with_length_parser_.reset(response_body_length_);
            }
          }
        } else {
          response_body_forbidden_ = true;
          response_state_ = response_state::finished;
        }
        callbacks::proxy_callbacks::on_response_pre_body_params
            on_response_pre_body_params{
                .connection_id = connection_id_,
                .request_id = request_id_,
                .ssl = bump_state_ == bump_state::established,
                .host = upstream_requested_host_,
                .request_pre_body = request_pre_body_,
                .response_pre_body = response_pre_body_,
                .expect_body_continue_from_downstream =
                    expect_body_continue_from_downstream_,
                .response_body_forbidden = response_body_forbidden_,
                .response_body_length_representation =
                    response_body_length_representation_,
                .response_body_length = response_body_length_,
                .outgoing_downstream_buffers = outgoing_downstream_buffers_,
                .outgoing_downstream_buffers_strings =
                    outgoing_downstream_buffers_strings_,
                .callback = boost::bind(
                    response_state_ == response_state::finished
                        ? &connection::process_upstream_read_step_2_no_body
                        : &connection::process_upstream_read_step_2_body,
                    shared_from_this(), boost::placeholders::_1)};
        callbacks_.async_on_response_pre_body(on_response_pre_body_params);
        return;
      }
    } else if (!result) {
      reply_ = reply::stock_reply(reply::status_type::bad_gateway);
      outgoing_downstream_buffers_ = reply_.to_buffers();
      downstream_write(boost::bind(&connection::handle_downstream_error_write,
                                   shared_from_this(),
                                   boost::asio::placeholders::error));
      return;
    } else {
      read_from_upstream();
      return;
    }
  }
  process_upstream_read_step_2_body(false);
}

void connection::process_upstream_read_step_2_no_body(bool send_immediately) {
  callbacks::proxy_callbacks::on_response_body_some_params
      on_response_body_some_params{
          .connection_id = connection_id_,
          .request_id = request_id_,
          .ssl = bump_state_ == bump_state::established,
          .host = upstream_requested_host_,
          .request_pre_body = request_pre_body_,
          .response_pre_body = response_pre_body_,
          .response_chunked_trailer = response_chunked_trailer_,
          .response_body_forbidden = response_body_forbidden_,
          .response_has_more_body = false,
          .body_begin = upstream_read_buffer_begin_,
          .body_end = upstream_read_buffer_end_,
          .response_body_length_representation =
              response_body_length_representation_,
          .response_body_length = response_body_length_,
          .outgoing_downstream_buffers = outgoing_downstream_buffers_,
          .outgoing_downstream_buffers_strings =
              outgoing_downstream_buffers_strings_,
          .callback =
              boost::bind(&connection::process_upstream_read_step_3_action,
                          shared_from_this())};
  callbacks_.async_on_response_body_some(on_response_body_some_params);
}

void connection::process_upstream_read_step_2_body(bool send_immediately) {
  if (!send_immediately && response_state_ == response_state::body &&
      upstream_read_buffer_begin_ != upstream_read_buffer_end_) {
    const std::string::iterator body_begin = upstream_read_buffer_begin_;
    bool response_has_more_body = true;
    std::string::iterator body_end;
    if (response_body_length_representation_ ==
        http::body_length_representation::chunked) {
      boost::logic::tribool result;
      boost::tie(result, upstream_read_buffer_begin_, body_end) =
          response_body_without_length_parser_.parse(
              response_chunked_trailer_, upstream_read_buffer_begin_,
              upstream_read_buffer_end_);
      if (result) {
        response_state_ = response_state::finished;
        response_has_more_body = false;
      } else if (!result) {
        connection_manager_stop();
        return;
      }
    } else {
      boost::tribool result;
      boost::tie(result, upstream_read_buffer_begin_) =
          response_body_with_length_parser_.parse(upstream_read_buffer_begin_,
                                                  upstream_read_buffer_end_);
      body_end = upstream_read_buffer_begin_;
      if (result) {
        response_state_ = response_state::finished;
        response_has_more_body = false;
      } else if (!result) {
        connection_manager_stop();
        return;
      }
    }

    callbacks::proxy_callbacks::on_response_body_some_params
        on_response_body_some_params{
            .connection_id = connection_id_,
            .request_id = request_id_,
            .ssl = bump_state_ == bump_state::established,
            .host = upstream_requested_host_,
            .request_pre_body = request_pre_body_,
            .response_pre_body = response_pre_body_,
            .response_chunked_trailer = response_chunked_trailer_,
            .response_body_forbidden = response_body_forbidden_,
            .response_has_more_body = response_has_more_body,
            .body_begin = body_begin,
            .body_end = body_end,
            .response_body_length_representation =
                response_body_length_representation_,
            .response_body_length = response_body_length_,
            .outgoing_downstream_buffers = outgoing_downstream_buffers_,
            .outgoing_downstream_buffers_strings =
                outgoing_downstream_buffers_strings_,
            .callback =
                boost::bind(&connection::process_upstream_read_step_3_action,
                            shared_from_this())};
    callbacks_.async_on_response_body_some(on_response_body_some_params);
    return;
  }
  process_upstream_read_step_3_action();
}

void connection::process_upstream_read_step_3_action() {
  if (request_state_ == request_state::body &&
      !expect_body_continue_from_downstream_) {
    // We got rejection
    request_state_ = request_state::finished;
    wrote_something_to_upstream_ = true;
  }
  if (outgoing_downstream_buffers_.empty()) {
    if (response_state_ == response_state::finished) {
      finish_request();
    } else {
      read_from_upstream();
    }
  } else {
    downstream_write(boost::bind(&connection::handle_downstream_write,
                                 shared_from_this(),
                                 boost::asio::placeholders::error));
  }
}

void connection::handle_downstream_read(const boost::system::error_code &e,
                                        std::size_t bytes_transferred) {
  if (!e) {
    if (request_state_ == request_state::tunnel) {
      boost::asio::async_write(
          upstream_socket_,
          boost::asio::const_buffer(&*downstream_read_buffer_.begin(),
                                    bytes_transferred),
          boost::bind(&connection::handle_upstream_write, shared_from_this(),
                      boost::asio::placeholders::error));
    } else {
      downstream_read_buffer_begin_ = downstream_read_buffer_.begin();
      downstream_read_buffer_end_ =
          downstream_read_buffer_.begin() + bytes_transferred;
      process_downstream_read_step_1_pre_body();
    }
  } else if (e != boost::asio::error::operation_aborted) {
    connection_manager_stop();
  }
}

void connection::process_downstream_read_step_1_pre_body() {
  DVLOG(1) << logging::FORMAT_FG_MAGENTA
           << "process_downstream_read_step_1_pre_body(" << connection_id_
           << ", " << request_id_ << "):\n"
           << boost::string_view(&*downstream_read_buffer_begin_,
                                 downstream_read_buffer_end_ -
                                     downstream_read_buffer_begin_)
           << logging::FORMAT_RESET;

  if (request_state_ == request_state::pre_body) {
    boost::tribool result;
    boost::tie(result, downstream_read_buffer_begin_) =
        request_pre_body_parser_.parse(request_pre_body_,
                                       downstream_read_buffer_begin_,
                                       downstream_read_buffer_end_);
    if (result) {
      if (request_pre_body_.method == "CONNECT") {
        std::vector<std::string> url_parts;
        boost::split(url_parts, request_pre_body_.uri, boost::is_any_of(":"));

        if (url_parts.size() == 2 && url_parts[1].length() > 0 &&
            url_parts[1].length() < 6 &&
            downstream_read_buffer_begin_ == downstream_read_buffer_end_) {
          bool port_is_valid = true;
          for (char &ch : url_parts[1]) {
            if (!util::misc_strings::is_digit(ch)) {
              port_is_valid = false;
              break;
            }
          }
          if (port_is_valid) {
            int port = std::stoi(url_parts[1]);
            if (port > 65535 || port < 1) {
              port_is_valid = false;
            }
          }
          if (port_is_valid) {
            upstream_requested_host_ = url_parts[0];
            upstream_requested_service_ = url_parts[1];
            callbacks::proxy_callbacks::on_connect_method_params
                on_connect_method_params{
                    .connection_id = connection_id_,
                    .host = upstream_requested_host_,
                    .service = upstream_requested_service_,
                    .request_pre_body = request_pre_body_,
                    .callback = boost::bind(
                        &connection::async_on_connect_method_callback,
                        shared_from_this(), boost::placeholders::_1)};
            callbacks_.async_on_connect_method(on_connect_method_params);
            return;
          } else {
            reply_ = reply::stock_reply(reply::status_type::bad_request);
            outgoing_downstream_buffers_ = reply_.to_buffers();
            downstream_write(boost::bind(
                &connection::handle_downstream_error_write, shared_from_this(),
                boost::asio::placeholders::error));
            return;
          }
        } else {
          reply_ = reply::stock_reply(reply::status_type::bad_request);
          outgoing_downstream_buffers_ = reply_.to_buffers();
          downstream_write(boost::bind(
              &connection::handle_downstream_error_write, shared_from_this(),
              boost::asio::placeholders::error));
          return;
        }
      } else {
        request_state_ = request_state::body;
        bool detect_body_length_result;
        boost::tie(detect_body_length_result,
                   request_body_length_representation_, request_body_length_) =
            http_parser::detect_body_length(request_pre_body_.headers);
        if (!detect_body_length_result) {
          reply_ = reply::stock_reply(reply::status_type::bad_request);
          outgoing_downstream_buffers_ = reply_.to_buffers();
          downstream_write(boost::bind(
              &connection::handle_downstream_error_write, shared_from_this(),
              boost::asio::placeholders::error));
          return;
        } else if (request_body_length_representation_ !=
                   http::body_length_representation::chunked) {
          if (http::request_has_body(request_body_length_representation_,
                                     request_body_length_)) {
            request_body_with_length_parser_.reset(request_body_length_);
          } else {
            request_state_ = request_state::finished;
          }
        }
        if (bump_state_ == bump_state::no_bump) {
          bool extract_host_result;
          boost::tie(extract_host_result, upstream_requested_host_,
                     upstream_requested_service_) =
              extract_host_and_fix_request();
          if (!extract_host_result) {
            connection_manager_stop();
            return;
          }
        }
        if (is_connection_close(request_pre_body_.http_version_string,
                                request_pre_body_.headers)) {
          connection_close_ = true;
        }
        http::header_container::name_iterator expect_it =
            request_pre_body_.headers.find("expect");
        expect_100_continue_from_upstream_ =
            expect_it != request_pre_body_.headers.end() &&
            expect_it->value == "100-continue";

        callbacks::proxy_callbacks::on_request_pre_body_params
            on_request_pre_body_params{
                .connection_id = connection_id_,
                .request_id = request_id_,
                .ssl = bump_state_ == bump_state::established,
                .host = upstream_requested_host_,
                .request_pre_body = request_pre_body_,
                .response_pre_body = response_pre_body_,
                .expect_body_continue_from_downstream =
                    expect_body_continue_from_downstream_,
                .expect_100_continue_from_upstream =
                    expect_100_continue_from_upstream_,
                .request_chunked_trailer = request_chunked_trailer_,
                .response_chunked_trailer = response_chunked_trailer_,
                .request_body_length_representation =
                    request_body_length_representation_,
                .request_body_length = request_body_length_,
                .outgoing_downstream_buffers = outgoing_downstream_buffers_,
                .outgoing_downstream_buffers_strings =
                    outgoing_downstream_buffers_strings_,
                .outgoing_upstream_buffers = outgoing_upstream_buffers_,
                .outgoing_upstream_buffers_strings =
                    outgoing_upstream_buffers_strings_,
                .callback = boost::bind(
                    request_state_ == request_state::finished
                        ? &connection::process_downstream_read_step_2_no_body
                        : &connection::process_downstream_read_step_2_body,
                    shared_from_this(), boost::placeholders::_1)};
        callbacks_.async_on_request_pre_body(on_request_pre_body_params);
        return;
      }
    } else if (!result) {
      reply_ = reply::stock_reply(reply::status_type::bad_request);
      outgoing_downstream_buffers_ = reply_.to_buffers();
      downstream_write(boost::bind(&connection::handle_downstream_error_write,
                                   shared_from_this(),
                                   boost::asio::placeholders::error));
      return;
    } else {
      read_from_downstream();
      return;
    }
  }
  process_downstream_read_step_2_body(false);
}

void connection::async_on_connect_method_callback(bool bump) {
  if (bump) {
    bump_state_ = bump_state::handshake;
    connect_connection_established_outgoing_line_ =
        request_pre_body_.http_version_string +
        " 200 Connection established\r\n\r\n";
    outgoing_downstream_buffers_.emplace_back(
        boost::asio::buffer(connect_connection_established_outgoing_line_));
    downstream_write(boost::bind(&connection::handle_downstream_write,
                                 shared_from_this(),
                                 boost::asio::placeholders::error));
  } else {
    request_state_ = request_state::tunnel;
    response_state_ = response_state::tunnel;
    connect_to_upstream(upstream_requested_host_, upstream_requested_service_);
  }
}

void connection::process_downstream_read_step_2_no_body(bool send_immediately) {
  callbacks::proxy_callbacks::on_request_body_some_params
      on_request_body_some_params{
          .connection_id = connection_id_,
          .request_id = request_id_,
          .ssl = bump_state_ == bump_state::established,
          .host = upstream_requested_host_,
          .request_pre_body = request_pre_body_,
          .response_pre_body = response_pre_body_,
          .expect_100_continue_from_upstream =
              expect_100_continue_from_upstream_,
          .request_chunked_trailer = request_chunked_trailer_,
          .response_chunked_trailer = response_chunked_trailer_,
          .request_has_more_body = false,
          .body_begin = downstream_read_buffer_begin_,
          .body_end = downstream_read_buffer_end_,
          .request_body_length_representation =
              request_body_length_representation_,
          .request_body_length = request_body_length_,
          .outgoing_downstream_buffers = outgoing_downstream_buffers_,
          .outgoing_downstream_buffers_strings =
              outgoing_downstream_buffers_strings_,
          .outgoing_upstream_buffers = outgoing_upstream_buffers_,
          .outgoing_upstream_buffers_strings =
              outgoing_upstream_buffers_strings_,
          .callback =
              boost::bind(&connection::process_downstream_read_step_3_action,
                          shared_from_this())};
  callbacks_.async_on_request_body_some(on_request_body_some_params);
}

void connection::process_downstream_read_step_2_body(bool send_immediately) {
  if (!send_immediately && request_state_ == request_state::body &&
      downstream_read_buffer_begin_ != downstream_read_buffer_end_) {
    const std::string::iterator body_begin = downstream_read_buffer_begin_;
    bool request_has_more_body = true;
    std::string::iterator body_end;
    if (request_body_length_representation_ ==
        http::body_length_representation::chunked) {
      boost::logic::tribool result;
      boost::tie(result, downstream_read_buffer_begin_, body_end) =
          request_body_without_length_parser_.parse(
              request_chunked_trailer_, downstream_read_buffer_begin_,
              downstream_read_buffer_end_);
      if (result) {
        request_state_ = request_state::finished;
        request_has_more_body = false;
      } else if (!result) {
        connection_manager_stop();
        return;
      }
    } else {
      boost::tribool result;
      boost::tie(result, downstream_read_buffer_begin_) =
          request_body_with_length_parser_.parse(downstream_read_buffer_begin_,
                                                 downstream_read_buffer_end_);
      body_end = downstream_read_buffer_begin_;
      if (result) {
        request_state_ = request_state::finished;
        request_has_more_body = false;
      } else if (!result) {
        connection_manager_stop();
        return;
      }
    }
    callbacks::proxy_callbacks::on_request_body_some_params
        on_request_body_some_params{
            .connection_id = connection_id_,
            .request_id = request_id_,
            .ssl = bump_state_ == bump_state::established,
            .host = upstream_requested_host_,
            .request_pre_body = request_pre_body_,
            .response_pre_body = response_pre_body_,
            .expect_100_continue_from_upstream =
                expect_100_continue_from_upstream_,
            .request_chunked_trailer = request_chunked_trailer_,
            .response_chunked_trailer = response_chunked_trailer_,
            .request_has_more_body = request_has_more_body,
            .body_begin = body_begin,
            .body_end = body_end,
            .request_body_length_representation =
                request_body_length_representation_,
            .request_body_length = request_body_length_,
            .outgoing_downstream_buffers = outgoing_downstream_buffers_,
            .outgoing_downstream_buffers_strings =
                outgoing_downstream_buffers_strings_,
            .outgoing_upstream_buffers = outgoing_upstream_buffers_,
            .outgoing_upstream_buffers_strings =
                outgoing_upstream_buffers_strings_,
            .callback =
                boost::bind(&connection::process_downstream_read_step_3_action,
                            shared_from_this())};
    callbacks_.async_on_request_body_some(on_request_body_some_params);
    return;
  }
  process_downstream_read_step_3_action();
}

void connection::process_downstream_read_step_3_action() {
  if (!outgoing_upstream_buffers_.empty()) {
    wrote_something_to_upstream_ = true;
    connect_to_upstream(upstream_requested_host_, upstream_requested_service_);
  } else if (outgoing_downstream_buffers_.empty()) {
    if (request_state_ == request_state::finished) {
      if (wrote_something_to_upstream_) {
        read_from_upstream();
      } else {
        // Both buffers are empty and we have nothing to write - we read
        // everything already and now we just have to move to next request.
        finish_request();
      }
    } else {
      read_from_downstream();
    }
  } else {
    if (!expect_body_continue_from_downstream_) {
      request_state_ = request_state::finished;
    }
    downstream_write(boost::bind(&connection::handle_downstream_write,
                                 shared_from_this(),
                                 boost::asio::placeholders::error));
  }
}

void connection::handle_downstream_error_write(
    const boost::system::error_code &e) {
  if (e != boost::asio::error::operation_aborted) {
    // This will deallocate outgoing_{downstream,upstream}_buffers_strings.
    connection_manager_stop();
  }
}

} // namespace server
} // namespace proxy
