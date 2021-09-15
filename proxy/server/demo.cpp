#include "proxy/callbacks/callbacks.hpp"
#include "proxy/logging/logging.hpp"
#include "server.hpp"
#include <clocale>

int main(int argc, char *argv[]) {
  // For lowercase.
  std::setlocale(LC_ALL, "en_US.iso88591");

  proxy::logging::init(argv[0]);

  // Check command line arguments.
  if (argc != 3) {
    std::cerr << "Usage: bazel run proxy/server:demo <address> <port>\n";
    std::cerr << "  For IPv4, try:\n";
    std::cerr << "    bazel run proxy/server:demo 0.0.0.0 80\n";
    std::cerr << "  For IPv6, try:\n";
    std::cerr << "    bazel run proxy/server:demo 0::0 80\n";
    return 1;
  }

  proxy::callbacks::proxy_callbacks callbacks;

  // Initialize the server.
  proxy::server::server s(argv[1], argv[2], callbacks);

  // Run the server until stopped.
  s.run();

  return 0;
}
