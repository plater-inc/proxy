#include "utils.hpp"
#include <sstream>

namespace proxy {
namespace callbacks {

void write_chunk_prefix(
    unsigned long long length, std::vector<boost::asio::const_buffer> &buffers,
    std::vector<std::unique_ptr<std::string>> &buffers_strings) {
  std::stringstream length_stream;
  length_stream << std::hex << length << "\r\n";
  std::unique_ptr<std::string> prefix(new std::string(length_stream.str()));
  buffers.emplace_back(boost::asio::buffer(*prefix));
  buffers_strings.push_back(std::move(prefix));
}

void write_chunk_suffix(
    std::vector<boost::asio::const_buffer> &buffers,
    std::vector<std::unique_ptr<std::string>> &buffers_strings) {
  std::unique_ptr<std::string> suffix(new std::string("\r\n"));
  buffers.emplace_back(boost::asio::buffer(*suffix));
  buffers_strings.push_back(std::move(suffix));
}

void write_200_response_with_length(
    http::request_pre_body &request_pre_body,
    http::response_pre_body &response_pre_body,
    std::unique_ptr<std::string> response,
    std::vector<boost::asio::const_buffer> &buffers,
    std::vector<std::unique_ptr<std::string>> &buffers_strings) {
  response_pre_body.code = "200";
  response_pre_body.http_version_string = request_pre_body.http_version_string;
  response_pre_body.reason = "OK";
  response_pre_body.headers.push_back(
      {"Content-Length", std::to_string(response->length())});
  buffers = response_pre_body.to_buffers();
  buffers.emplace_back(boost::asio::buffer(*response));
  buffers_strings.push_back(std::move(response));
}

} // namespace callbacks
} // namespace proxy