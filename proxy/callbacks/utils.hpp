#ifndef PROXY_CALLBACKS_UTILS_HPP
#define PROXY_CALLBACKS_UTILS_HPP

#include "callbacks.hpp"

namespace proxy {
namespace callbacks {

void write_chunk_prefix(
    unsigned long long length, std::vector<boost::asio::const_buffer> &buffers,
    std::vector<std::unique_ptr<std::string>> &buffers_strings);

void write_chunk_suffix(
    std::vector<boost::asio::const_buffer> &buffers,
    std::vector<std::unique_ptr<std::string>> &buffers_strings);

void write_200_response_with_length(
    http::request_pre_body &request_pre_body,
    http::response_pre_body &response_pre_body,
    std::unique_ptr<std::string> response,
    std::vector<boost::asio::const_buffer> &buffers,
    std::vector<std::unique_ptr<std::string>> &buffers_strings);

} // namespace callbacks
} // namespace proxy

#endif // PROXY_CALLBACKS_UTILS_HPP