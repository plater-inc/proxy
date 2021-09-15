#include "proxy/callbacks/utils.hpp"
#include "proxy/logging/logging.hpp"
#include "proxy/server/server.hpp"
#include "proxy/util/utils.hpp"
#include "tests-proxy/test_util/ready_callbacks_proxy.hpp"
#include <clocale>

struct mix_callbacks_proxy : public tests_proxy::util::ready_callbacks_proxy {
  mix_callbacks_proxy(std::ofstream &debug_pipe)
      : tests_proxy::util::ready_callbacks_proxy(debug_pipe) {}

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
    params.callback(false);
  }

  virtual void async_on_request_pre_body(on_request_pre_body_params &params) {
    debug_pipe_ << "request_pre_body " << params.request_pre_body.uri << "\n"
                << std::flush;
    if (params.request_pre_body.uri == "/def/") {
      params.response_pre_body.code = "200";
      params.response_pre_body.http_version_string =
          params.request_pre_body.http_version_string;
      params.response_pre_body.reason = "OK";
      std::unique_ptr<std::string> response;

      response.reset(new std::string("<h1>def</h1>"));
      params.response_pre_body.headers.push_back(
          {"Content-Length", std::to_string(response->length())});

      params.outgoing_downstream_buffers =
          params.response_pre_body.to_buffers();

      params.outgoing_downstream_buffers.emplace_back(
          boost::asio::buffer(*response));
      params.outgoing_downstream_buffers_strings.push_back(std::move(response));

      params.callback(false);
    } else {
      tests_proxy::util::ready_callbacks_proxy::async_on_request_pre_body(
          params);
    }
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
    debug_pipe_ << "response_pre_body " << params.request_pre_body.uri << "\n"
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
};

int main(int argc, char *argv[]) {
  // For lowercase.
  std::setlocale(LC_ALL, "en_US.iso88591");

  proxy::logging::init(argv[0]);

  std::ofstream debug_pipe{argv[1]};

  setbuf(stdout, NULL);
  setbuf(stderr, NULL);

  mix_callbacks_proxy callbacks(debug_pipe);

  // Initialize the server.
  proxy::server::server s("127.0.0.1", "0", callbacks);

  // Run the server until stopped.
  s.run();

  return 0;
}
