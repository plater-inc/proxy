#include "trailer.hpp"
#include "proxy/util/misc_strings.hpp"

namespace proxy {
namespace http {

const std::string trailer::prefix = "0";

std::vector<boost::asio::const_buffer> trailer::to_buffers() {
  std::vector<boost::asio::const_buffer> buffers;
  buffers.push_back(boost::asio::buffer(prefix));
  buffers.push_back(boost::asio::buffer(extension));
  buffers.push_back(boost::asio::buffer(util::misc_strings::crlf));

  for (header_container::iterator it = headers.begin(); it != headers.end();
       it++) {
    const header &h = *it;
    buffers.push_back(boost::asio::buffer(h.name));
    buffers.push_back(
        boost::asio::buffer(util::misc_strings::name_value_separator));
    buffers.push_back(boost::asio::buffer(h.value));
    buffers.push_back(boost::asio::buffer(util::misc_strings::crlf));
  }
  buffers.push_back(boost::asio::buffer(util::misc_strings::crlf));
  return buffers;
}

} // namespace http
} // namespace proxy