#include "utils.hpp"
#include <iostream>

namespace proxy {
namespace util {

namespace utils {

bool icompare_with_lowercase(const std::string &s1,
                             const std::string &s2_lowercase) {
  int index1 = 0;
  int index2 = 0;
  for (; index1 < s1.size() && index2 < s2_lowercase.size();
       index1++, index2++) {
    if (static_cast<char>(std::tolower(
            static_cast<unsigned char>(s1[index1]))) != s2_lowercase[index2]) {
      return false;
    }
  }
  return index1 == s1.size() && index2 == s2_lowercase.size();
}

void print_vector_of_buffers(
    const std::vector<boost::asio::const_buffer> &buffers) {
  for (boost::asio::const_buffer buffer : buffers) {
    std::cout.write(static_cast<const char *>(buffer.data()),
                    boost::asio::buffer_size(buffer));
  }
}

std::string vector_of_buffers_to_string(
    const std::vector<boost::asio::const_buffer> &buffers) {
  std::string result;
  for (boost::asio::const_buffer buffer : buffers) {
    result += std::string_view(static_cast<const char *>(buffer.data()),
                               boost::asio::buffer_size(buffer));
  }
  return result;
}

} // namespace utils

} // namespace util
} // namespace proxy