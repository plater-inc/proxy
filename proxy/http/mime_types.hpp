#ifndef PROXY_HTTP_MIME_TYPES_HPP
#define PROXY_HTTP_MIME_TYPES_HPP

#include <string>

namespace proxy {
namespace http {
namespace mime_types {

/// Convert a file extension into a MIME type.
std::string extension_to_type(const std::string &extension);

} // namespace mime_types
} // namespace http
} // namespace proxy

#endif // PROXY_HTTP_MIME_TYPES_HPP
