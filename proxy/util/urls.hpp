#ifndef PROXY_UTIL_URLS_HPP
#define PROXY_UTIL_URLS_HPP

#include <boost/tuple/tuple.hpp>
#include <string>
#include <vector>

namespace proxy {
namespace util {

/**
 * Return <success, host, port, path>
 */
[[nodiscard]] boost::tuple<bool, bool, std::string, std::string, std::string>
extract_url_parts(std::string &url);

} // namespace util
} // namespace proxy
#endif // PROXY_UTIL_URLS_HPP
