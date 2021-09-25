#include "proxy/util/urls.hpp"
#include <iostream>
#include <ostream>
#include <string>

int main(int argc, char *argv[]) {
  bool success;
  bool ssl;
  std::string host;
  std::string port;
  std::string path;

  struct test_case_struct {
    std::string url;
    bool success{true};
    bool ssl{};
    std::string host{};
    std::string port{"80"};
    std::string path{};
  };
  std::vector<test_case_struct> test_cases = {
      {.url = "https://wikipedia.org",
       .ssl = true,
       .host = "wikipedia.org",
       .path = "/"},
      {.url = "http://wikipedia.org/",
       .ssl = false,
       .host = "wikipedia.org",
       .path = "/"},
      {.url = "https://en.wikipedia.org/path",
       .ssl = true,
       .host = "en.wikipedia.org",
       .path = "/path"},
      {
          .url = "wikipedia.org",
          .success = false,
      },
      {.url = "https://en.wikipedia.org:8080/path",
       .ssl = true,
       .host = "en.wikipedia.org",
       .port = "8080",
       .path = "/path"},
  };

  for (const auto &test : test_cases) {
    std::string url = test.url;
    boost::tie(success, ssl, host, port, path) =
        proxy::util::extract_url_parts(url);
    if (!success && test.success) {
      std::cerr << "Url: " << url << " should be invalid!" << std::endl;
      return 1;
    } else if (success) {
      if (ssl != test.ssl) {
        std::cerr << "Url: " << url << " ssl should be " << test.ssl << "!"
                  << std::endl;
        return 1;
      }
      if (host != test.host) {
        std::cerr << "Url: " << url << " host should be " << test.host << "!"
                  << std::endl;
        return 1;
      }
      if (port != test.port) {
        std::cerr << "Url: " << url << " port should be " << test.port << "!"
                  << std::endl;
        return 1;
      }
      if (path != test.path) {
        std::cerr << "Url: " << url << " path should be " << test.path << "!"
                  << std::endl;
        return 1;
      }
    }
  }

  return 0;
}
