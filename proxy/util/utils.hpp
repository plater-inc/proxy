#ifndef PROXY_UTIL_UTILS_HPP
#define PROXY_UTIL_UTILS_HPP

#include <boost/asio.hpp>
#include <string>

namespace proxy {
namespace util {

namespace utils {

bool icompare_with_lowercase(const std::string &s1,
                             const std::string &s2_lowercase);

void print_vector_of_buffers(
    const std::vector<boost::asio::const_buffer> &buffers);

std::string vector_of_buffers_to_string(
    const std::vector<boost::asio::const_buffer> &buffers);

} // namespace utils

} // namespace util
} // namespace proxy

#endif // PROXY_UTIL_UTILS_HPP