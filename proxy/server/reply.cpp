#include "reply.hpp"
#include "proxy/util/misc_strings.hpp"
#include <boost/lexical_cast.hpp>
#include <string>

namespace proxy {
namespace server {

namespace status_strings {

const std::string bad_request = "HTTP/1.0 400 Bad Request\r\n";
const std::string bad_gateway = "HTTP/1.0 502 Bad Gateway\r\n";

boost::asio::const_buffer to_buffer(reply::status_type status) {
  switch (status) {
  case reply::status_type::bad_request:
    return boost::asio::buffer(bad_request);
  case reply::status_type::bad_gateway:
    return boost::asio::buffer(bad_gateway);
  }
}

} // namespace status_strings

std::vector<boost::asio::const_buffer> reply::to_buffers() {
  std::vector<boost::asio::const_buffer> buffers;
  buffers.push_back(status_strings::to_buffer(status));
  for (std::size_t i = 0; i < headers.size(); ++i) {
    http::header &h = headers[i];
    buffers.push_back(boost::asio::buffer(h.name));
    buffers.push_back(
        boost::asio::buffer(util::misc_strings::name_value_separator));
    buffers.push_back(boost::asio::buffer(h.value));
    buffers.push_back(boost::asio::buffer(util::misc_strings::crlf));
  }
  buffers.push_back(boost::asio::buffer(util::misc_strings::crlf));
  buffers.push_back(boost::asio::buffer(content));
  return buffers;
}

namespace stock_replies {
const char bad_request[] = "<html>"
                           "<head><title>Bad Request</title></head>"
                           "<body><h1>400 Bad Request</h1></body>"
                           "</html>";
const char bad_gateway[] = "<html>"
                           "<head><title>Bad Gateway</title></head>"
                           "<body><h1>502 Bad Gateway</h1></body>"
                           "</html>";

std::string to_string(reply::status_type status) {
  switch (status) {
  case reply::status_type::bad_request:
    return bad_request;
  case reply::status_type::bad_gateway:
    return bad_gateway;
  }
}

} // namespace stock_replies

reply reply::stock_reply(reply::status_type status) {
  reply rep;
  rep.status = status;
  rep.content = stock_replies::to_string(status);
  rep.headers.reserve(2);
  rep.headers.push_back(
      {"Content-Length", boost::lexical_cast<std::string>(rep.content.size())});
  rep.headers.push_back({"Content-Type", "text/html"});
  return rep;
}

} // namespace server
} // namespace proxy
