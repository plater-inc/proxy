#include "request_pre_body.hpp"
#include "proxy/util/misc_strings.hpp"

namespace proxy {
namespace http {

std::vector<boost::asio::const_buffer> request_pre_body::to_buffers() {
  std::vector<boost::asio::const_buffer> buffers;
  buffers.push_back(boost::asio::buffer(method));
  buffers.push_back(
      boost::asio::buffer(util::misc_strings::method_line_separator));
  buffers.push_back(boost::asio::buffer(uri));
  buffers.push_back(
      boost::asio::buffer(util::misc_strings::method_line_separator));
  buffers.push_back(boost::asio::buffer(http_version_string));
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

bool request_has_body(body_length_representation representation,
                      unsigned long long length) {
  // There ARE NO infinite request bodies.
  return representation == body_length_representation::chunked ||
         (representation == body_length_representation::content_length &&
          length > 0ULL);
}

} // namespace http
} // namespace proxy
