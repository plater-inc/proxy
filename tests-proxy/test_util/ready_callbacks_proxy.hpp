#ifndef TESTS_PROXY_UTIL_READY_CALLBACKS_PROXY_HPP
#define TESTS_PROXY_UTIL_READY_CALLBACKS_PROXY_HPP

#include "fstream"
#include "proxy/callbacks/callbacks.hpp"

namespace tests_proxy {
namespace util {

typedef unsigned long long connection_id;
typedef unsigned long long request_id;

struct ready_callbacks_proxy : public proxy::callbacks::proxy_callbacks {
  ready_callbacks_proxy(std::ofstream &debug_pipe) : debug_pipe_(debug_pipe) {}

  virtual void on_ready(unsigned short port_number) {
    debug_pipe_ << port_number << "\n" << std::flush;
    proxy::callbacks::proxy_callbacks::on_ready(port_number);
  }

protected:
  std::ofstream &debug_pipe_;
};

} // namespace util
} // namespace tests_proxy

#endif // TESTS_PROXY_UTIL_READY_CALLBACKS_PROXY_HPP