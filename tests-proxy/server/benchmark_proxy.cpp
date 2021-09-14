#include "proxy/logging/logging.hpp"
#include "proxy/server/server.hpp"
#include "tests-proxy/test_util/ready_callbacks_proxy.hpp"
#include <clocale>

int main(int argc, char *argv[]) {
  // For lowercase.
  std::setlocale(LC_ALL, "en_US.iso88591");

  proxy::logging::init(argv[0]);

  std::ofstream debug_pipe{argv[1]};

  setbuf(stdout, NULL);
  setbuf(stderr, NULL);

  tests_proxy::util::ready_callbacks_proxy callbacks(debug_pipe);

  // Initialize the server.
  proxy::server::server s("127.0.0.1", "0", callbacks);

  // Run the server until stopped.
  s.run();

  return 0;
}
