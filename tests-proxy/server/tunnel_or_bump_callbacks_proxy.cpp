#include "proxy/logging/logging.hpp"
#include "proxy/server/server.hpp"
#include "tests-proxy/test_util/ready_callbacks_proxy.hpp"
#include <clocale>
#include <fstream>
#include <ostream>

struct tunnel_or_bump_callbacks_proxy
    : public tests_proxy::util::ready_callbacks_proxy {
  tunnel_or_bump_callbacks_proxy(std::ofstream &debug_pipe, bool bump)
      : tests_proxy::util::ready_callbacks_proxy(debug_pipe), bump_(bump) {}

  virtual void
  async_on_connection(proxy::callbacks::connection_id connection_id,
                      BOOST_ASIO_MOVE_ARG(boost::function<void()>) callback) {
    debug_pipe_ << "connection\n" << std::flush;
    tests_proxy::util::ready_callbacks_proxy::async_on_connection(
        connection_id, std::forward<boost::function<void()>>(callback));
  }

  virtual void async_on_connect_method(on_connect_method_params &params) {
    debug_pipe_ << "connect " << params.host << " " << params.service << "\n"
                << std::flush;
    params.callback(bump_);
  }

  virtual void async_on_request_pre_body(on_request_pre_body_params &params) {
    debug_pipe_ << "request_pre_body " << params.request_pre_body.uri << "\n"
                << std::flush;
    tests_proxy::util::ready_callbacks_proxy::async_on_request_pre_body(params);
  };

  virtual void async_on_request_body_some(on_request_body_some_params &params) {
    if (!params.request_has_more_body) {
      debug_pipe_ << "request_body_some_last " << params.request_pre_body.uri
                  << "\n"
                  << std::flush;
    }
    tests_proxy::util::ready_callbacks_proxy::async_on_request_body_some(
        params);
  };

  virtual void async_on_response_pre_body(on_response_pre_body_params &params) {
    debug_pipe_ << "response_pre_body " << params.request_pre_body.uri << " "
                << params.response_pre_body.code << "\n"
                << std::flush;
    tests_proxy::util::ready_callbacks_proxy::async_on_response_pre_body(
        params);
  };

  virtual void
  async_on_response_body_some(on_response_body_some_params &params) {
    if (!params.response_has_more_body) {
      debug_pipe_ << "response_body_some_last " << params.request_pre_body.uri
                  << "\n"
                  << std::flush;
    }
    tests_proxy::util::ready_callbacks_proxy::async_on_response_body_some(
        params);
  };

  virtual void
  async_on_response_finished(proxy::callbacks::connection_id connection_id,
                             proxy::callbacks::request_id request_id,
                             BOOST_ASIO_MOVE_ARG(boost::function<void(bool)>)
                                 callback) {
    debug_pipe_ << "response_finished\n" << std::flush;
    tests_proxy::util::ready_callbacks_proxy::async_on_response_finished(
        connection_id, request_id,
        std::forward<boost::function<void(bool)>>(callback));
  }

  virtual void
  on_connection_finished(proxy::callbacks::connection_id connection_id) {
    debug_pipe_ << "connection_finished\n" << std::flush;
    tests_proxy::util::ready_callbacks_proxy::on_connection_finished(
        connection_id);
  }

private:
  bool bump_;
};

int main(int argc, char *argv[]) {
  // For lowercase.
  std::setlocale(LC_ALL, "en_US.iso88591");

  proxy::logging::init(argv[0]);

  std::ofstream debug_pipe{argv[1]};
  bool bump = argv[2] == std::string("bump");

  setbuf(stdout, NULL);
  setbuf(stderr, NULL);

  tunnel_or_bump_callbacks_proxy callbacks(debug_pipe, bump);

  // Initialize the server.
  proxy::server::server s("127.0.0.1", "0", callbacks);

  // Run the server until stopped.
  s.run();

  return 0;
}
