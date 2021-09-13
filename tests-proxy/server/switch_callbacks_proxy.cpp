#include "proxy/callbacks/utils.hpp"
#include "proxy/logging/logging.hpp"
#include "proxy/server/server.hpp"
#include "proxy/util/utils.hpp"
#include "tests-proxy/test_util/ready_callbacks_proxy.hpp"
#include <clocale>

struct switch_callbacks_proxy
    : public tests_proxy::util::ready_callbacks_proxy {
  switch_callbacks_proxy(std::ofstream &debug_pipe, std::string behavior,
                         bool send_immediately)
      : tests_proxy::util::ready_callbacks_proxy(debug_pipe),
        behavior_(behavior), send_immediately_(send_immediately) {}

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
    if ((behavior_ == "request_body_last_generates_response_with_body" ||
         behavior_ == "request_body_last_generates_response_without_body") &&
        params.expect_100_continue_from_upstream) {
      params.response_pre_body.code = "100";
      params.response_pre_body.http_version_string =
          params.request_pre_body.http_version_string;
      params.response_pre_body.reason = "Continue";
      params.outgoing_downstream_buffers =
          params.response_pre_body.to_buffers();
    } else if (behavior_ ==
                   "request_pre_body_generates_404_response_with_body" ||
               behavior_ ==
                   "request_pre_body_generates_404_response_without_body" ||
               behavior_ == "request_pre_body_generates_response_with_body" ||
               behavior_ ==
                   "request_pre_body_generates_response_without_body") {
      if (params.expect_100_continue_from_upstream) {
        if (behavior_ == "request_pre_body_generates_404_response_with_body" ||
            behavior_ ==
                "request_pre_body_generates_404_response_without_body") {
          params.expect_body_continue_from_downstream = false;
        } else {
          proxy::http::response_pre_body response_100_pre_body;
          response_100_pre_body.code = "100";
          response_100_pre_body.http_version_string =
              params.request_pre_body.http_version_string;
          response_100_pre_body.reason = "Continue";
          std::vector<boost::asio::const_buffer> buffers =
              response_100_pre_body.to_buffers();
          std::unique_ptr<std::string> response_100(
              std::make_unique<std::string>(
                  proxy::util::utils::vector_of_buffers_to_string(buffers)));
          params.outgoing_downstream_buffers.emplace_back(
              boost::asio::buffer(*response_100));
          params.outgoing_downstream_buffers_strings.push_back(
              std::move(response_100));
        }
      }
      params.response_pre_body.code =
          behavior_ == "request_pre_body_generates_404_response_with_body" ||
                  behavior_ ==
                      "request_pre_body_generates_404_response_without_body"
              ? "404"
              : "200";
      params.response_pre_body.http_version_string =
          params.request_pre_body.http_version_string;
      params.response_pre_body.reason =
          behavior_ == "request_pre_body_generates_404_response_with_body" ||
                  behavior_ ==
                      "request_pre_body_generates_404_response_without_body"
              ? "Not Found"
              : "OK";
      std::unique_ptr<std::string> response;
      if (behavior_ == "request_pre_body_generates_response_with_body" ||
          behavior_ == "request_pre_body_generates_404_response_with_body") {
        response.reset(new std::string("<h1>" + behavior_ + "</h1>"));
        params.response_pre_body.headers.push_back(
            {"Content-Length", std::to_string(response->length())});
      } else {
        params.response_pre_body.headers.push_back({"Content-Length", "0"});
      }
      std::vector<boost::asio::const_buffer> buffers =
          params.response_pre_body.to_buffers();
      params.outgoing_downstream_buffers.insert(
          params.outgoing_downstream_buffers.end(), buffers.begin(),
          buffers.end());
      if (params.request_pre_body.method != "HEAD" &&
          behavior_ == "request_pre_body_generates_response_with_body") {
        params.outgoing_downstream_buffers.emplace_back(
            boost::asio::buffer(*response));
        params.outgoing_downstream_buffers_strings.push_back(
            std::move(response));
      }
    } else if (behavior_.starts_with("buffer_request_")) {
      per_request_data_[params.connection_id][params.request_id]
          .request_buffer = std::make_unique<std::string>(
          proxy::util::utils::vector_of_buffers_to_string(
              params.request_pre_body.to_buffers()));
    } else if (behavior_ != "request_body_last_generates_response_with_body" &&
               behavior_ !=
                   "request_body_last_generates_response_without_body") {
      params.outgoing_upstream_buffers = params.request_pre_body.to_buffers();
    }
    debug_pipe_ << "request_pre_body " << params.request_pre_body.uri
                << (params.outgoing_upstream_buffers.empty()
                        ? (params.outgoing_downstream_buffers.empty()
                               ? " none"
                               : " downstream")
                        : " upstream")
                << "\n"
                << std::flush;
    params.callback(send_immediately_);
  };

  virtual void async_on_request_body_some(on_request_body_some_params &params) {
    if (!params.request_has_more_body) {
      debug_pipe_ << "request_body_some_last " << params.request_pre_body.uri
                  << "\n"
                  << std::flush;
    }

    if (params.request_pre_body.method != "HEAD" &&
        (behavior_ == "request_pre_body_generates_response_with_body" ||
         behavior_ == "request_pre_body_generates_response_without_body")) {
      // We have to drop request body if we generate response in
      // async_on_request_pre_body.
      params.callback();
    } else if (behavior_.starts_with("buffer_request_")) {
      if (params.body_begin != params.body_end) {
        per_request_data_[params.connection_id][params.request_id]
            .request_buffer->append(&*params.body_begin,
                                    params.body_end - params.body_begin);
      }
      if (!params.request_has_more_body) {
        if (params.request_body_length_representation ==
            proxy::http::body_length_representation::chunked) {
          per_request_data_[params.connection_id][params.request_id]
              .request_buffer->append(
                  proxy::util::utils::vector_of_buffers_to_string(
                      params.request_chunked_trailer.to_buffers()));
        }
        params.outgoing_upstream_buffers.emplace_back(boost::asio::buffer(
            *per_request_data_[params.connection_id][params.request_id]
                 .request_buffer));
        params.outgoing_upstream_buffers_strings.push_back(
            std::move(per_request_data_[params.connection_id][params.request_id]
                          .request_buffer));
      }
      params.callback();
    } else if (behavior_ == "request_body_last_generates_response_with_body" ||
               behavior_ ==
                   "request_body_last_generates_response_without_body") {
      if (!params.request_has_more_body) {
        params.response_pre_body.code = "200";
        params.response_pre_body.http_version_string =
            params.request_pre_body.http_version_string;
        params.response_pre_body.reason = "OK";
        std::unique_ptr<std::string> response;
        if (behavior_ == "request_body_last_generates_response_with_body") {
          response.reset(new std::string("<h1>" + behavior_ + "</h1>"));
          params.response_pre_body.headers.push_back(
              {"Content-Length", std::to_string(response->length())});
        } else {
          params.response_pre_body.headers.push_back({"Content-Length", "0"});
        }
        std::vector<boost::asio::const_buffer> buffers =
            params.response_pre_body.to_buffers();
        params.outgoing_downstream_buffers.insert(
            params.outgoing_downstream_buffers.end(), buffers.begin(),
            buffers.end());
        if (params.request_pre_body.method != "HEAD" &&
            behavior_ == "request_body_last_generates_response_with_body") {
          params.outgoing_downstream_buffers.emplace_back(
              boost::asio::buffer(*response));
          params.outgoing_downstream_buffers_strings.push_back(
              std::move(response));
        }
      }
      params.callback();
    } else {
      tests_proxy::util::ready_callbacks_proxy::async_on_request_body_some(
          params);
    }
  };

  virtual void async_on_response_pre_body(on_response_pre_body_params &params) {
    if (behavior_.ends_with("response_pre_body_generates_response_with_body") ||
        behavior_.ends_with(
            "response_pre_body_generates_response_without_body")) {
      if (params.response_pre_body.code == "100") {
        std::vector<boost::asio::const_buffer> buffers =
            params.response_pre_body.to_buffers();
        std::unique_ptr<std::string> response_100(std::make_unique<std::string>(
            proxy::util::utils::vector_of_buffers_to_string(buffers)));
        params.outgoing_downstream_buffers.emplace_back(
            boost::asio::buffer(*response_100));
        params.outgoing_downstream_buffers_strings.push_back(
            std::move(response_100));
      }
      params.response_pre_body.code = "200";
      params.response_pre_body.http_version_string =
          params.request_pre_body.http_version_string;
      params.response_pre_body.reason = "OK";
      params.response_pre_body.headers.clear();
      std::unique_ptr<std::string> response;
      if (behavior_.ends_with(
              "response_pre_body_generates_response_with_body")) {
        response.reset(new std::string("<h1>" + behavior_ + "</h1>"));
        params.response_pre_body.headers.push_back(
            {"Content-Length", std::to_string(response->length())});
      } else {
        params.response_pre_body.headers.push_back({"Content-Length", "0"});
      }
      std::vector<boost::asio::const_buffer> buffers =
          params.response_pre_body.to_buffers();
      params.outgoing_downstream_buffers.insert(
          params.outgoing_downstream_buffers.end(), buffers.begin(),
          buffers.end());
      if (params.request_pre_body.method != "HEAD" &&
          behavior_.ends_with(
              "response_pre_body_generates_response_with_body")) {
        params.outgoing_downstream_buffers.emplace_back(
            boost::asio::buffer(*response));
        params.outgoing_downstream_buffers_strings.push_back(
            std::move(response));
      }
    } else if (params.response_pre_body.code != "100" &&
               (behavior_.ends_with("response_pre_body_prepend") ||
                behavior_.ends_with("response_body_prepend") ||
                behavior_.ends_with("response_body_append"))) {
      std::unique_ptr<std::string> extra_response{new std::string(
          behavior_.ends_with("response_body_append") ? "<h1>Post body</h1>"
                                                      : "<h1>Pre body</h1>")};
      if (params.response_body_length_representation ==
          proxy::http::body_length_representation::content_length) {
        params.response_pre_body.headers.find("content-length")->value =
            std::to_string(params.response_body_length +
                           extra_response->size());
      }
      std::vector<boost::asio::const_buffer> buffers =
          params.response_pre_body.to_buffers();
      params.outgoing_downstream_buffers.insert(
          params.outgoing_downstream_buffers.end(), buffers.begin(),
          buffers.end());
      if (!params.response_body_forbidden &&
          behavior_.ends_with("response_pre_body_prepend")) {
        if (params.response_body_length_representation ==
            proxy::http::body_length_representation::chunked) {
          proxy::callbacks::write_chunk_prefix(
              extra_response->size(), params.outgoing_downstream_buffers,
              params.outgoing_downstream_buffers_strings);
        }
        params.outgoing_downstream_buffers.emplace_back(
            boost::asio::buffer(*extra_response));
        params.outgoing_downstream_buffers_strings.push_back(
            std::move(extra_response));
        if (params.response_body_length_representation ==
            proxy::http::body_length_representation::chunked) {
          proxy::callbacks::write_chunk_suffix(
              params.outgoing_downstream_buffers,
              params.outgoing_downstream_buffers_strings);
        }
      }
    } else {
      std::vector<boost::asio::const_buffer> buffers =
          params.response_pre_body.to_buffers();
      params.outgoing_downstream_buffers.insert(
          params.outgoing_downstream_buffers.end(), buffers.begin(),
          buffers.end());
    }
    debug_pipe_ << "response_pre_body " << params.request_pre_body.uri << " "
                << params.response_pre_body.code
                << (params.outgoing_downstream_buffers.empty() ? " none"
                                                               : " downstream")
                << "\n"
                << std::flush;
    params.callback(send_immediately_);
  };

  virtual void
  async_on_response_body_some(on_response_body_some_params &params) {
    if (!params.response_has_more_body) {
      debug_pipe_ << "response_body_some_last " << params.request_pre_body.uri
                  << "\n"
                  << std::flush;
    }
    if (behavior_.ends_with("response_pre_body_generates_response_with_body") ||
        behavior_.ends_with(
            "response_pre_body_generates_response_without_body")) {
      params.callback();
    } else if (behavior_.ends_with("response_body_prepend") &&
               per_request_data_[params.connection_id][params.request_id]
                   .first_response_body) {
      per_request_data_[params.connection_id][params.request_id]
          .first_response_body = false;
      std::unique_ptr<std::string> pre_response{
          new std::string("<h1>Pre body</h1>")};
      if (params.response_body_length_representation ==
          proxy::http::body_length_representation::chunked) {
        proxy::callbacks::write_chunk_prefix(
            pre_response->size(), params.outgoing_downstream_buffers,
            params.outgoing_downstream_buffers_strings);
      }
      params.outgoing_downstream_buffers.emplace_back(
          boost::asio::buffer(*pre_response));
      params.outgoing_downstream_buffers_strings.push_back(
          std::move(pre_response));
      if (params.response_body_length_representation ==
          proxy::http::body_length_representation::chunked) {
        proxy::callbacks::write_chunk_suffix(
            params.outgoing_downstream_buffers,
            params.outgoing_downstream_buffers_strings);
      }
      tests_proxy::util::ready_callbacks_proxy::async_on_response_body_some(
          params);
    } else if (behavior_.ends_with("response_body_append") &&
               !params.response_has_more_body) {
      if (params.body_begin != params.body_end) {
        params.outgoing_downstream_buffers.emplace_back(
            &*params.body_begin, params.body_end - params.body_begin);
      }
      std::unique_ptr<std::string> post_response{
          new std::string("<h1>Post body</h1>")};
      if (params.response_body_length_representation ==
          proxy::http::body_length_representation::chunked) {
        proxy::callbacks::write_chunk_prefix(
            post_response->size(), params.outgoing_downstream_buffers,
            params.outgoing_downstream_buffers_strings);
      }
      params.outgoing_downstream_buffers.emplace_back(
          boost::asio::buffer(*post_response));
      params.outgoing_downstream_buffers_strings.push_back(
          std::move(post_response));
      if (params.response_body_length_representation ==
          proxy::http::body_length_representation::chunked) {
        proxy::callbacks::write_chunk_suffix(
            params.outgoing_downstream_buffers,
            params.outgoing_downstream_buffers_strings);
      }
      if (params.response_body_length_representation ==
          proxy::http::body_length_representation::chunked) {
        std::vector<boost::asio::const_buffer> append =
            params.response_chunked_trailer.to_buffers();
        params.outgoing_downstream_buffers.insert(
            params.outgoing_downstream_buffers.end(), append.begin(),
            append.end());
      }
      params.callback();
    } else {
      tests_proxy::util::ready_callbacks_proxy::async_on_response_body_some(
          params);
    }
  };

  virtual void
  async_on_response_finished(proxy::callbacks::connection_id connection_id,
                             proxy::callbacks::request_id request_id,
                             BOOST_ASIO_MOVE_ARG(boost::function<void()>)
                                 callback) {
    debug_pipe_ << "response_finished\n" << std::flush;
    std::map<unsigned long long,
             std::map<unsigned long long, request_data>>::iterator it =
        per_request_data_.find(connection_id);
    if (it != per_request_data_.end()) {
      per_request_data_[connection_id].erase(request_id);
    }

    tests_proxy::util::ready_callbacks_proxy::async_on_response_finished(
        connection_id, request_id,
        std::forward<boost::function<void()>>(callback));
  }

  virtual void
  on_connection_finished(proxy::callbacks::connection_id connection_id) {
    debug_pipe_ << "connection_finished\n" << std::flush;
    per_request_data_.erase(connection_id);
    tests_proxy::util::ready_callbacks_proxy::on_connection_finished(
        connection_id);
  }

private:
  std::string behavior_;
  bool send_immediately_;
  struct request_data {
    bool first_response_body{true};
    std::unique_ptr<std::string> request_buffer{};
  };
  std::map<proxy::callbacks::connection_id,
           std::map<proxy::callbacks::request_id, request_data>>
      per_request_data_;
};

int main(int argc, char *argv[]) {
  // For lowercase.
  std::setlocale(LC_ALL, "en_US.iso88591");

  proxy::logging::init(argv[0]);

  std::ofstream debug_pipe{argv[1]};
  std::string behavior = argv[2];
  bool send_immediately = std::string(argv[3]) == "immediately";

  setbuf(stdout, NULL);
  setbuf(stderr, NULL);

  switch_callbacks_proxy callbacks(debug_pipe, behavior, send_immediately);

  // Initialize the server.
  proxy::server::server s("127.0.0.1", "0", callbacks);

  // Run the server until stopped.
  s.run();

  return 0;
}
