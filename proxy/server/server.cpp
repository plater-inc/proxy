#include "server.hpp"
#include "proxy/cert/certificate_generator.hpp"
#include <boost/bind/bind.hpp>
#include <fstream>
#include <signal.h>

namespace proxy {
namespace server {

void write_file_to_disk(std::string file_name, std::string content) {
  std::ofstream out(file_name);
  out << content;
  out.close();
}

std::string read_file_from_disk(std::string file_name) {
  std::ifstream f(file_name);
  if (!f.good()) {
    return "";
  }
  std::string ret((std::istreambuf_iterator<char>(f)),
                  std::istreambuf_iterator<char>());
  f.close();
  return ret;
}

server::server(const std::string &address, const std::string &port,
               callbacks::proxy_callbacks &callbacks)
    : io_context_(), signals_(io_context_), acceptor_(io_context_),
      connection_manager_(), new_connection_(), callbacks_(callbacks) {
  // Register to handle the signals that indicate when the server should exit.
  // It is safe to register for the same signal multiple times in a program,
  // provided all registration for the specified signal is made through Asio.
  signals_.add(SIGINT);
  signals_.add(SIGTERM);
#if defined(SIGQUIT)
  signals_.add(SIGQUIT);
#endif // defined(SIGQUIT)
  signals_.async_wait(boost::bind(&server::handle_stop, this));

  ca_private_key_ = read_file_from_disk("ca.key");
  ca_certificate_ = read_file_from_disk("ca.pem");

  if (ca_private_key_ == "" || ca_certificate_ == "") {
    boost::tie(ca_private_key_, ca_certificate_) =
        cert::generate_root_certificate();
    write_file_to_disk("ca.key", ca_private_key_);
    write_file_to_disk("ca.pem", ca_certificate_);
  }

  // Open the acceptor with the option to reuse the address (i.e. SO_REUSEADDR).
  resolver_.reset(new boost::asio::ip::tcp::resolver(io_context_));
  boost::asio::ip::tcp::endpoint endpoint =
      *resolver_->resolve(address, port).begin();
  acceptor_.open(endpoint.protocol());
  acceptor_.set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));
  acceptor_.bind(endpoint);
  acceptor_.listen();

  start_accept();
}

void server::run() {
  callbacks_.on_ready(acceptor_.local_endpoint().port());
  // The io_context::run() call will block until all asynchronous operations
  // have finished. While the server is running, there is always at least one
  // asynchronous operation outstanding: the asynchronous accept call waiting
  // for new incoming connections.
  io_context_.run();
}

void server::start_accept() {
  new_connection_.reset(new connection(
      io_context_, connection_manager_, resolver_, ca_private_key_,
      ca_certificate_, domain_certificates_, connection_id_++, callbacks_));
  acceptor_.async_accept(new_connection_->downstream_socket(),
                         boost::bind(&server::handle_accept, this,
                                     boost::asio::placeholders::error));
}

void server::handle_accept(const boost::system::error_code &e) {
  // Check whether the server was stopped by a signal before this completion
  // handler had a chance to run.
  if (!acceptor_.is_open()) {
    return;
  }

  if (!e) {
    connection_manager_.start(new_connection_);
  }

  start_accept();
}

void server::handle_stop() {
  // The server is stopped by cancelling all outstanding asynchronous
  // operations. Once all operations have finished the io_context::run() call
  // will exit.
  acceptor_.close();
  connection_manager_.stop_all();
}

} // namespace server
} // namespace proxy
