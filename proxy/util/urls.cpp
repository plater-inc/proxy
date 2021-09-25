#include "urls.hpp"
#include <boost/algorithm/string.hpp>
#include <string>

namespace proxy {
namespace util {

[[nodiscard]] boost::tuple<bool, bool, std::string, std::string, std::string>
extract_url_parts(std::string &url) {
  int index_start = -1;
  int count = 0;
  int slash_count = 0;
  for (int i = 0; i < url.length(); i++) {
    if (url[i] == '/') {
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
    return boost::make_tuple(false, false, "" /* ignored */, "" /* ignored */,
                             "" /* ignored */);
  }

  std::string ssl = url.substr(0, index_start);
  bool is_ssl = ssl == "https://" ? true : false;
  std::string host_port = url.substr(index_start, count);
  std::vector<std::string> host_port_parts;
  boost::split(host_port_parts, host_port, boost::is_any_of(":"));
  // We check for count == 0 before, so here we know the vector will not be
  // empty.
  std::string host = host_port_parts[0];
  std::string port = host_port_parts.size() == 1 ? "80" : host_port_parts[1];
  std::string path = "/";

  if (slash_count > 2) {
    path = url.substr(index_start + count);
  }
  return boost::make_tuple(true, is_ssl, host, port, path);
}
} // namespace util
} // namespace proxy
